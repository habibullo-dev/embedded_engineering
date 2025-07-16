/* persistent_logging.c - Flash-based persistent logging with interactive viewer */
#include "main.h"
#include "persistent_logging.h"
#include "stm32f7xx_hal_flash_ex.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

extern UART_HandleTypeDef huart4;

// Flash configuration - Sector 5 (256KB)
#define LOG_FLASH_SECTOR     FLASH_SECTOR_5    
#define LOG_FLASH_ADDRESS    0x08040000        
#define LOGS_PER_PAGE        10               

static SemaphoreHandle_t flashMutex = NULL;

// Forward declarations
static uint32_t calculate_checksum(const FlashLogEntry_t* entry);
static HAL_StatusTypeDef flash_write_word(uint32_t address, uint32_t data);
static void print_to_terminal(const char* message);
static const char* get_level_string(LogLevel_t level);

// Calculate XOR checksum for data integrity
static uint32_t calculate_checksum(const FlashLogEntry_t* entry) {
    uint32_t checksum = 0;
    const uint8_t* data = (const uint8_t*)entry;
    for (size_t i = 0; i < sizeof(FlashLogEntry_t) - sizeof(uint32_t); i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Flash write with cache coherency for STM32F7
static HAL_StatusTypeDef flash_write_word(uint32_t address, uint32_t data) {
    HAL_StatusTypeDef status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data);
    if (status == HAL_OK) {
        SCB_InvalidateDCache_by_Addr((uint32_t*)address, 4);
    }
    return status;
}

static void print_to_terminal(const char* message) {
    HAL_UART_Transmit(&huart4, (uint8_t*)message, strlen(message), 1000);
}

/**
 * Convert log level to readable string
 */
static const char* get_level_string(LogLevel_t level) {
    switch(level) {
        case LOG_ERROR:   return "ERROR";
        case LOG_WARNING: return "WARN ";
        case LOG_INFO:    return "INFO ";
        case LOG_LOGIN:   return "LOGIN";
        case LOG_SUCCESS: return "SUCCS";
        case LOG_SENSOR:  return "SENSR";
        case LOG_DEBUG:   return "DEBUG";
        default:          return "UNKN ";
    }
}

void PersistentLog_EraseAll(void) {
    if (flashMutex == NULL) return;

    if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        HAL_StatusTypeDef status = HAL_FLASH_Unlock();
        if (status != HAL_OK) {
            xSemaphoreGive(flashMutex);
            return;
        }

        // Erase the entire flash sector
        FLASH_EraseInitTypeDef eraseInit;
        eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
        eraseInit.Sector = LOG_FLASH_SECTOR;
        eraseInit.NbSectors = 1;
        eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        uint32_t sectorError = 0;
        status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
        if (status == HAL_OK) {
            // Initialize header with magic number
            status = flash_write_word(LOG_FLASH_ADDRESS, 0xCAFEBABE);
        }
        
        HAL_FLASH_Lock();

        // Invalidate cache for the entire sector to ensure coherency
        SCB_InvalidateDCache_by_Addr((uint32_t*)LOG_FLASH_ADDRESS, 128*1024);

        xSemaphoreGive(flashMutex);
    }
}

void PersistentLog_Init(void) {
    // Create mutex for thread-safe flash access
    flashMutex = xSemaphoreCreateMutex();

    // Check if flash contains valid log data, initialize if not
    volatile uint32_t* header_magic = (volatile uint32_t*)LOG_FLASH_ADDRESS;
    if (*header_magic != 0xCAFEBABE) {
        PersistentLog_EraseAll();
    }
}

void PersistentLog_Add(LogLevel_t level, const char* module, const char* message) {
    if (flashMutex == NULL) return;

    if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        volatile FlashLogHeader_t* header = (volatile FlashLogHeader_t*)LOG_FLASH_ADDRESS;

        // Find next available slot by scanning for empty entries
        uint32_t next_slot = MAX_FLASH_LOGS;
        uint32_t log_count = 0;
        
        for (uint32_t i = 0; i < MAX_FLASH_LOGS; i++) {
            if (header->logs[i].magic == 0xDEADBEEF) {
                log_count++;
            } else if (header->logs[i].magic == 0xFFFFFFFF && next_slot == MAX_FLASH_LOGS) {
                next_slot = i;  // Found first empty slot
            }
        }
        
        // If sector is full, erase and start over
        if (next_slot == MAX_FLASH_LOGS) {
            xSemaphoreGive(flashMutex);
            PersistentLog_EraseAll();
            if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
            header = (volatile FlashLogHeader_t*)LOG_FLASH_ADDRESS;
            next_slot = 0;
        }

        // Prepare log entry
        FlashLogEntry_t entry;
        entry.magic = 0xDEADBEEF;
        entry.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        entry.level = level;
        strncpy(entry.module, module, 15);
        entry.module[15] = '\0';
        strncpy(entry.message, message, 63);
        entry.message[63] = '\0';
        entry.checksum = calculate_checksum(&entry);

        // Write log entry to flash
        HAL_StatusTypeDef status = HAL_FLASH_Unlock();
        if (status != HAL_OK) {
            xSemaphoreGive(flashMutex);
            return;
        }

        // Write entry word by word to flash
        uint32_t* entry_words = (uint32_t*)&entry;
        uint32_t base_addr = LOG_FLASH_ADDRESS + 
                           offsetof(FlashLogHeader_t, logs) +
                           (next_slot * sizeof(FlashLogEntry_t));

        for (size_t i = 0; i < sizeof(FlashLogEntry_t) / 4; i++) {
            status = flash_write_word(base_addr + (i * 4), entry_words[i]);
            if (status != HAL_OK) {
                break;
            }
        }

        HAL_FLASH_Lock();
        
        // Invalidate cache for the written area to ensure coherency
        SCB_InvalidateDCache_by_Addr((uint32_t*)base_addr, sizeof(FlashLogEntry_t));
        
        xSemaphoreGive(flashMutex);
    }
}

/**
 * Get count of valid log entries in flash
 */
uint32_t PersistentLog_GetCount(void) {
    // Ensure cache coherency before reading
    SCB_InvalidateDCache_by_Addr((uint32_t*)LOG_FLASH_ADDRESS, 128*1024);
    
    volatile FlashLogHeader_t* header = (volatile FlashLogHeader_t*)LOG_FLASH_ADDRESS;
    if (header->header_magic != 0xCAFEBABE) {
        return 0;
    }

    uint32_t count = 0;
    for (uint32_t i = 0; i < MAX_FLASH_LOGS; i++) {
        if (header->logs[i].magic == 0xDEADBEEF) {
            count++;
        }
    }
    return count;
}

/**
 * Display all persistent logs (non-interactive fallback)
 */
void PersistentLog_EnterViewerMode(void) {
    // Ensure cache coherency before reading
    SCB_InvalidateDCache_by_Addr((uint32_t*)LOG_FLASH_ADDRESS, 128*1024);
    
    volatile FlashLogHeader_t* header = (volatile FlashLogHeader_t*)LOG_FLASH_ADDRESS;

    if (header->header_magic != 0xCAFEBABE) {
        print_to_terminal("\r\n=== PERSISTENT LOGS ===\r\n");
        print_to_terminal("No persistent logs found in flash.\r\n");
        return;
    }

    // Count total valid logs
    uint32_t total_logs = PersistentLog_GetCount();
    if (total_logs == 0) {
        print_to_terminal("\r\n=== PERSISTENT LOGS ===\r\n");
        print_to_terminal("No valid persistent logs found.\r\n");
        return;
    }

    // Collect valid log indices for display
    uint32_t valid_indices[MAX_FLASH_LOGS];
    uint32_t valid_count = 0;
    
    for (uint32_t i = 0; i < MAX_FLASH_LOGS; i++) {
        if (header->logs[i].magic == 0xDEADBEEF) {
            valid_indices[valid_count] = i;
            valid_count++;
        }
    }

    print_to_terminal("\r\n╭─────────────────────────────────────────╮\r\n");
    print_to_terminal("│        PERSISTENT LOGS VIEWER          │\r\n");
    print_to_terminal("╰─────────────────────────────────────────╯\r\n");
    
    char header_info[100];
    sprintf(header_info, "Total Logs Found: %lu\r\n\r\n", valid_count);
    print_to_terminal(header_info);

    // Display all logs
    for (uint32_t i = 0; i < valid_count; i++) {
        volatile FlashLogEntry_t* entry = &header->logs[valid_indices[i]];
        
        uint32_t seconds = entry->timestamp / 1000;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;

        char log_line[150];
        sprintf(log_line, "%02lu. [%02lu:%02lu:%02lu] %s | %s: %s\r\n",
               i + 1,
               hours % 24, minutes % 60, seconds % 60,
               get_level_string(entry->level),
               (char*)entry->module, (char*)entry->message);
        print_to_terminal(log_line);
        
        // Small delay every 10 logs to prevent overwhelming the terminal
        if ((i + 1) % 10 == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    print_to_terminal("\r\n╭─────────────────────────────────────────╮\r\n");
    print_to_terminal("│            END OF LOGS                 │\r\n");
    print_to_terminal("╰─────────────────────────────────────────╯\r\n");
}
