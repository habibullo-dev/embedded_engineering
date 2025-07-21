/* user_config.h - User Configuration Management */
#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#include "main.h"
#include <stdint.h>

// Configuration constants
#define MAX_USERNAME_LENGTH 16
#define MAX_PASSWORD_LENGTH 16
#define USER_CONFIG_MAGIC 0xC0FABCD0

// Default credentials (fallback)
#define DEFAULT_USERNAME "admin"
#define DEFAULT_PASSWORD "1234"

// User configuration structure
typedef struct {
    uint32_t magic;                              // Magic number for validation
    char username[MAX_USERNAME_LENGTH];          // Custom username
    char password[MAX_PASSWORD_LENGTH];          // Custom password
    uint32_t checksum;                           // Simple checksum for integrity
} UserConfig_t;

// Function declarations
void UserConfig_Init(void);
uint8_t UserConfig_ValidateCredentials(const char* username, const char* password);
uint8_t UserConfig_ChangeCredentials(const char* new_username, const char* new_password);
void UserConfig_GetCurrentUsername(char* buffer, uint8_t max_len);
uint8_t UserConfig_IsUsingDefaults(void);
void UserConfig_ResetToDefaults(void);
uint32_t UserConfig_CalculateChecksum(const UserConfig_t* config);

#endif /* USER_CONFIG_H */
