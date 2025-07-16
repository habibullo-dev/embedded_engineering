/* terminal_ui.h - Terminal User Interface Module */
#ifndef TERMINAL_UI_H
#define TERMINAL_UI_H

#include "main.h"

// Public function declarations
void TerminalUI_Init(void);
void TerminalUI_ProcessInput(void);
void TerminalUI_ShowBanner(void);
void TerminalUI_ShowPrompt(void);
void TerminalUI_SendString(const char* str);
void TerminalUI_PrintStatus(const char* message, const char* color);

// Command processing
void TerminalUI_ProcessCommand(void);
void TerminalUI_ShowHelp(void);

// Session management
uint8_t TerminalUI_IsLoggedIn(void);
void TerminalUI_Logout(void);
void TerminalUI_CheckTimeout(void);

// Display functions
void TerminalUI_ShowSystemInfo(void);
void TerminalUI_ShowUptime(void);
void TerminalUI_ShowAllSensors(void);
void TerminalUI_ShowDetailedAccel(void);

// I2C diagnostics
void TerminalUI_I2CScan(void);
void TerminalUI_I2CTest(void);

// Logs mode functions
void TerminalUI_EnterLogsMode(void);
void TerminalUI_ProcessLogsMode(char c);
uint8_t TerminalUI_IsInLogsMode(void);

#endif /* TERMINAL_UI_H */
