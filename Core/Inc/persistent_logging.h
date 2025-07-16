#ifndef PERSISTENT_LOGGING_H
#define PERSISTENT_LOGGING_H

#include "system_logging.h"
#include <stdint.h>

#define MAX_FLASH_LOGS       50

typedef struct {
    uint32_t magic;           // 0xDEADBEEF for valid entries
    uint32_t timestamp;       // System uptime in milliseconds  
    LogLevel_t level;         
    char module[16];          
    char message[64];         
    uint32_t checksum;        // XOR checksum for integrity
} FlashLogEntry_t;

typedef struct {
    uint32_t header_magic;    // 0xCAFEBABE indicates valid flash area
    uint32_t log_counter;     
    uint32_t reserved2;       
    FlashLogEntry_t logs[MAX_FLASH_LOGS];
} FlashLogHeader_t;

// Core functions
void PersistentLog_Init(void);
void PersistentLog_Add(LogLevel_t level, const char* module, const char* message);
void PersistentLog_EraseAll(void);

// Viewer functions  
void PersistentLog_EnterViewerMode(void);
uint32_t PersistentLog_GetCount(void);

#endif
