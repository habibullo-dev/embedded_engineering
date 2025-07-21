/* user_config.c - User Configuration Management Implementation */
#include "user_config.h"
#include "system_logging.h"
#include "persistent_logging.h"
#include <string.h>
#include <stdio.h>

// Flash storage location for user config (adjust address as needed)
#define USER_CONFIG_FLASH_ADDR    0x08060000  // Use a different sector than logs

// Global user configuration
static UserConfig_t current_config;
static uint8_t config_loaded = 0;

// Private function declarations
static void load_config_from_flash(void);
static uint8_t save_config_to_flash(const UserConfig_t* config);
static void set_default_config(void);

/**
 * Initialize user configuration system
 */
void UserConfig_Init(void) {
    load_config_from_flash();
    
    if (!config_loaded) {
        SystemLog_Add(LOG_INFO, "user_config", "No valid config found, using defaults");
        set_default_config();
    } else {
        SystemLog_Add(LOG_INFO, "user_config", "Loaded custom credentials from flash");
    }
}

/**
 * Validate provided credentials against stored/default credentials
 */
uint8_t UserConfig_ValidateCredentials(const char* username, const char* password) {
    if (!username || !password) {
        return 0;
    }
    
    // Check against current configuration
    if (strcmp(username, current_config.username) == 0 && 
        strcmp(password, current_config.password) == 0) {
        return 1;
    }
    
    return 0;
}

/**
 * Change user credentials and save to flash
 */
uint8_t UserConfig_ChangeCredentials(const char* new_username, const char* new_password) {
    if (!new_username || !new_password) {
        return 0;
    }
    
    // Validate lengths
    if (strlen(new_username) >= MAX_USERNAME_LENGTH || 
        strlen(new_password) >= MAX_PASSWORD_LENGTH) {
        return 0;
    }
    
    // Validate that username and password are not empty
    if (strlen(new_username) == 0 || strlen(new_password) == 0) {
        return 0;
    }
    
    // Create new configuration
    UserConfig_t new_config;
    new_config.magic = USER_CONFIG_MAGIC;
    strncpy(new_config.username, new_username, MAX_USERNAME_LENGTH - 1);
    strncpy(new_config.password, new_password, MAX_PASSWORD_LENGTH - 1);
    new_config.username[MAX_USERNAME_LENGTH - 1] = '\0';
    new_config.password[MAX_PASSWORD_LENGTH - 1] = '\0';
    new_config.checksum = UserConfig_CalculateChecksum(&new_config);
    
    // Save to flash
    if (save_config_to_flash(&new_config)) {
        // Update current configuration
        memcpy(&current_config, &new_config, sizeof(UserConfig_t));
        
        char log_msg[80];
        sprintf(log_msg, "Credentials changed to user: %s", new_username);
        PersistentLog_Add(LOG_LOGIN, "account", log_msg);
        SystemLog_Add(LOG_INFO, "user_config", "Credentials successfully updated");
        
        return 1;
    }
    
    SystemLog_Add(LOG_ERROR, "user_config", "Failed to save new credentials to flash");
    return 0;
}

/**
 * Get current username
 */
void UserConfig_GetCurrentUsername(char* buffer, uint8_t max_len) {
    if (buffer && max_len > 0) {
        strncpy(buffer, current_config.username, max_len - 1);
        buffer[max_len - 1] = '\0';
    }
}

/**
 * Check if using default credentials
 */
uint8_t UserConfig_IsUsingDefaults(void) {
    return (strcmp(current_config.username, DEFAULT_USERNAME) == 0 && 
            strcmp(current_config.password, DEFAULT_PASSWORD) == 0);
}

/**
 * Reset to default credentials
 */
void UserConfig_ResetToDefaults(void) {
    set_default_config();
    
    // Erase flash configuration
    // Note: In a real implementation, you'd erase the flash sector here
    SystemLog_Add(LOG_INFO, "user_config", "Reset to default credentials");
    PersistentLog_Add(LOG_LOGIN, "account", "Credentials reset to defaults");
}

/**
 * Calculate simple checksum for configuration integrity
 */
uint32_t UserConfig_CalculateChecksum(const UserConfig_t* config) {
    if (!config) return 0;
    
    uint32_t checksum = config->magic;
    
    // Add username bytes
    for (int i = 0; i < MAX_USERNAME_LENGTH && config->username[i]; i++) {
        checksum += (uint32_t)config->username[i] * (i + 1);
    }
    
    // Add password bytes
    for (int i = 0; i < MAX_PASSWORD_LENGTH && config->password[i]; i++) {
        checksum += (uint32_t)config->password[i] * (i + 1);
    }
    
    return checksum;
}

// ==================== PRIVATE FUNCTIONS ====================

/**
 * Load configuration from flash memory
 */
static void load_config_from_flash(void) {
    // In a real implementation, you would read from flash here
    // For now, we'll simulate by checking if we have valid data
    
    volatile UserConfig_t* flash_config = (volatile UserConfig_t*)USER_CONFIG_FLASH_ADDR;
    
    // Check if flash contains valid configuration
    if (flash_config->magic == USER_CONFIG_MAGIC) {
        // Copy from flash to RAM
        memcpy(&current_config, (void*)flash_config, sizeof(UserConfig_t));
        
        // Verify checksum
        uint32_t calculated_checksum = UserConfig_CalculateChecksum(&current_config);
        if (calculated_checksum == current_config.checksum) {
            config_loaded = 1;
            return;
        } else {
            SystemLog_Add(LOG_ERROR, "user_config", "Checksum mismatch in flash config");
        }
    }
    
    // If we get here, flash config is invalid
    config_loaded = 0;
}

/**
 * Save configuration to flash memory
 */
static uint8_t save_config_to_flash(const UserConfig_t* config) {
    if (!config) return 0;
    
    // In a real implementation, you would:
    // 1. Unlock flash
    // 2. Erase the sector
    // 3. Write the new configuration
    // 4. Lock flash
    
    // For this example, we'll simulate success
    // Note: You'll need to implement actual flash operations based on your STM32 HAL
    
    /*
    HAL_StatusTypeDef status;
    
    // Unlock flash
    HAL_FLASH_Unlock();
    
    // Erase sector (you'll need to define the correct sector)
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_SECTOR_X;  // Define appropriate sector
    EraseInitStruct.NbSectors = 1;
    
    status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return 0;
    }
    
    // Write configuration data
    uint32_t* data = (uint32_t*)config;
    for (int i = 0; i < sizeof(UserConfig_t) / 4; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                                   USER_CONFIG_FLASH_ADDR + (i * 4), 
                                   data[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return 0;
        }
    }
    
    HAL_FLASH_Lock();
    return 1;
    */
    
    // For simulation purposes, return success
    SystemLog_Add(LOG_INFO, "user_config", "Simulated flash write (implement actual flash operations)");
    return 1;
}

/**
 * Set default configuration
 */
static void set_default_config(void) {
    current_config.magic = USER_CONFIG_MAGIC;
    strncpy(current_config.username, DEFAULT_USERNAME, MAX_USERNAME_LENGTH - 1);
    strncpy(current_config.password, DEFAULT_PASSWORD, MAX_PASSWORD_LENGTH - 1);
    current_config.username[MAX_USERNAME_LENGTH - 1] = '\0';
    current_config.password[MAX_PASSWORD_LENGTH - 1] = '\0';
    current_config.checksum = UserConfig_CalculateChecksum(&current_config);
    config_loaded = 1;
}
