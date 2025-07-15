#ifndef PERSISTENT_LOGGING_H
#define PERSISTENT_LOGGING_H

#include "system_logging.h"

void PersistentLog_Init(void);
void PersistentLog_Add(LogLevel_t level, const char* module, const char* message);
void PersistentLog_Display(void);
void PersistentLog_EraseAll(void);
void SystemLog_AddPersistent(LogLevel_t level, const char* module, const char* message);

#endif
