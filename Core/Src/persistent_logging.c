/* persistent_logging.c - Flash-based persistent logging */
#include "main.h"
#include "system_logging.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

// Flash sector for logs (use last sector to avoid overwriting code)
#define LOG_FLASH_SECTOR     FLASH_SECTOR_11
#define LOG_FLASH_ADDRESS    0x080E0000  // Sector 11 start address
#define MAX_FLASH_LOGS       50

typedef struct {
    uint32_t magic;           // 0xDEADBEEF for valid entry
    uint32_t timestamp;       // System uptime in ms
    LogLevel_t level;
    char module[16];
    char message[64];
    uint32_t checksum;        // Simple XOR checksum
} FlashLogEntry_t;

typedef struct {
    uint32_t header_magic;    // 0xCAFEBABE
    uint32_t log_count;
    uint32_t next_index;
    FlashLogEntry_t logs[MAX_FLASH_LOGS];
} FlashLogHeader_t;

static SemaphoreHandle_t flashMutex = NULL;

// Forward declarations
static uint32_t calculate_checksum(const FlashLogEntry_t* entry);
static void flash_write_word(uint32_t address, uint32_t data);

// Calculate simple checksum
static uint32_t calculate_checksum(const FlashLogEntry_t* entry) {
    uint32_t checksum = 0;
    const uint8_t* data = (const uint8_t*)entry;
    for (size_t i = 0; i < sizeof(FlashLogEntry_t) - sizeof(uint32_t); i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Simple flash write helper
static void flash_write_word(uint32_t address, uint32_t data) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data);
}

void PersistentLog_EraseAll(void) {
    if (flashMutex == NULL) return;

    if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        HAL_FLASH_Unlock();

        // Erase flash sector
        FLASH_EraseInitTypeDef eraseInit;
        eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
        eraseInit.Sector = LOG_FLASH_SECTOR;
        eraseInit.NbSectors = 1;
        eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        uint32_t sectorError;
        HAL_FLASHEx_Erase(&eraseInit, &sectorError);

        // Initialize header
        flash_write_word(LOG_FLASH_ADDRESS, 0xCAFEBABE);  // magic
        flash_write_word(LOG_FLASH_ADDRESS + 4, 0);       // log_count
        flash_write_word(LOG_FLASH_ADDRESS + 8, 0);       // next_index

        HAL_FLASH_Lock();

        xSemaphoreGive(flashMutex);
    }
}

void PersistentLog_Init(void) {
    flashMutex = xSemaphoreCreateMutex();

    // Check if flash contains valid log data
    volatile uint32_t* header_magic = (volatile uint32_t*)LOG_FLASH_ADDRESS;
    if (*header_magic != 0xCAFEBABE) {
        // Initialize flash log area
        PersistentLog_EraseAll();
    }
}

void PersistentLog_Add(LogLevel_t level, const char* module, const char* message) {
    if (flashMutex == NULL) return;

    if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        volatile FlashLogHeader_t* header = (volatile FlashLogHeader_t*)LOG_FLASH_ADDRESS;

        // Check if we need to erase (sector full)
        if (header->log_count >= MAX_FLASH_LOGS) {
            xSemaphoreGive(flashMutex);
            PersistentLog_EraseAll();
            if (xSemaphoreTake(flashMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
            header = (volatile FlashLogHeader_t*)LOG_FLASH_ADDRESS;
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

        HAL_FLASH_Unlock();

        // Write log entry word by word
        uint32_t* entry_words = (uint32_t*)&entry;
        uint32_t base_addr = LOG_FLASH_ADDRESS + sizeof(FlashLogHeader_t) -
                           (MAX_FLASH_LOGS * sizeof(FlashLogEntry_t)) +
                           (header->next_index * sizeof(FlashLogEntry_t));

        for (size_t i = 0; i < sizeof(FlashLogEntry_t) / 4; i++) {
            flash_write_word(base_addr + (i * 4), entry_words[i]);
        }

        // Update header counters
        uint32_t new_count = header->log_count + 1;
        uint32_t new_index = (header->next_index + 1) % MAX_FLASH_LOGS;

        flash_write_word(LOG_FLASH_ADDRESS + 4, new_count);
        flash_write_word(LOG_FLASH_ADDRESS + 8, new_index);

        HAL_FLASH_Lock();

        xSemaphoreGive(flashMutex);
    }
}

void PersistentLog_Display(void) {
    volatile FlashLogHeader_t* header = (volatile FlashLogHeader_t*)LOG_FLASH_ADDRESS;

    if (header->header_magic != 0xCAFEBABE) {
        SystemLog_Add(LOG_INFO, "pflash", "No persistent logs found");
        return;
    }

    SystemLog_Add(LOG_INFO, "pflash", "=== PERSISTENT LOGS FROM FLASH ===");

    uint32_t count = (header->log_count < MAX_FLASH_LOGS) ? header->log_count : MAX_FLASH_LOGS;

    for (uint32_t i = 0; i < count; i++) {
        volatile FlashLogEntry_t* entry = &header->logs[i];

        // Validate entry
        if (entry->magic == 0xDEADBEEF) {
            char flash_msg[100];
            uint32_t seconds = entry->timestamp / 1000;
            uint32_t minutes = seconds / 60;
            uint32_t hours = minutes / 60;

            sprintf(flash_msg, "[%02lu:%02lu:%02lu] %s: %s",
                   hours % 24, minutes % 60, seconds % 60,
                   (char*)entry->module, (char*)entry->message);

            SystemLog_Add(entry->level, "flash", flash_msg);
        }
    }
}

// Modified SystemLog_Add to also save to flash for critical logs
void SystemLog_AddPersistent(LogLevel_t level, const char* module, const char* message) {
    // Add to RAM log
    SystemLog_Add(level, module, message);

    // Save critical logs to flash
    if (level == LOG_ERROR || level == LOG_WARNING || level == LOG_LOGIN) {
        PersistentLog_Add(level, module, message);
    }
}
