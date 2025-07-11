# STM32F767 Multi-Sensor Terminal

A secure, feature-rich terminal interface for STM32F767 microcontrollers with command line editing, user authentication, LED control, and comprehensive multi-sensor monitoring capabilities.

## 🎯 Project Overview

This project implements a professional terminal interface accessible via UART, featuring modern command-line editing, secure login, and comprehensive system management with real-time sensor monitoring. The terminal provides an intuitive way to interact with STM32 hardware through a familiar command-line interface.

### Key Features

- **Secure Authentication**: Username/password login system with session management and logging
- **Advanced Command Line**: Full editing support with cursor movement, backspace, command history, and in-line editing
- **LED Control**: Direct hardware control with timer-based auto-off functionality, group/all control, and status feedback
- **Multi-Sensor Monitoring**: 
  - **Climate Sensor (HDC1080)**: Real-time temperature/humidity monitoring with comfort zone analysis
  - **Accelerometer (ADXL345)**: 3-axis acceleration, tilt detection, and orientation analysis
  - **Sensor Diagnostics**: Comprehensive testing and I2C bus scanning
- **Professional UI**: Color-coded output, modern terminal aesthetics, and dynamic banners with live sensor data
- **Session Management**: Automatic timeout with configurable duration and auto-logout
- **Modular Architecture**: Clean separation of concerns with dedicated modules for sensors, UI, logging, and LED control

### 🎬 Live Demonstration

Watch the professional terminal interface in action! The demonstration shows the complete feature set including login, command editing, multi-sensor monitoring, and LED control:

![Terminal Demo](./demonstration/i2c_shell.gif)

*See the secure login process, advanced command-line editing, real-time sensor monitoring, diagnostic commands, and comprehensive system status in action.*

## 🔧 Hardware Requirements

### Development Board
- **NUCLEO-F767ZI** development board with STM32F767ZI microcontroller
- Built-in ST-Link programmer/debugger
- USB cable for power and programming

### Sensor Hardware
- **HDC1080**: Temperature/humidity sensor (I2C address: 0x40)
- **ADXL345**: 3-axis accelerometer (I2C address: 0x53)
- **I2C Connection**: Both sensors on I2C2 bus (PF0=SDA, PF1=SCL)
- **Power**: 3.3V supply for both sensors

### Hardware Configuration
- **LEDs**: Three onboard LEDs (PB0, PB7, PB14) for status indication
- **UART**: UART4 for terminal communication
- **I2C**: I2C2 for sensor communication
- **Clock**: 216MHz system clock for optimal performance

### Terminal Access
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None

## 💻 Software Requirements

### Development Environment
- **STM32CubeIDE** version 1.8 or later
- **STM32CubeMX** for peripheral configuration
- **STM32F7 HAL Library** (managed automatically)

### Terminal Software
- **Windows**: PuTTY, Tera Term, or Windows Terminal
- **macOS/Linux**: screen, minicom, or any serial terminal
- **Cross-platform**: Arduino IDE Serial Monitor

## 🏗️ Project Architecture

### Modular Code Organization
```
Core/
├── Src/
│   ├── main.c              # Main system orchestration
│   ├── sensors.c           # Multi-sensor management (HDC1080 + ADXL345)
│   ├── terminal_ui.c       # Terminal interface and command processing
│   ├── system_logging.c    # Centralized logging system
│   ├── led_control.c       # LED management with timers
│   ├── usart.c             # UART configuration and handlers
│   ├── gpio.c              # GPIO initialization
│   ├── i2c.c               # I2C2 configuration
│   └── system_stm32f7xx.c  # System configuration
├── Inc/
│   ├── main.h              # Core definitions
│   ├── sensors.h           # Sensor interface declarations
│   ├── terminal_ui.h       # Terminal function prototypes
│   ├── system_logging.h    # Logging system interface
│   ├── system_config.h     # System-wide configuration
│   ├── led_control.h       # LED control interface
│   ├── usart.h             # UART declarations
│   ├── gpio.h              # GPIO function declarations
│   └── i2c.h               # I2C configuration
```

### Terminal Architecture

The terminal implements a state machine with three main states:
- **LOGIN_STATE**: Username input and validation
- **PASSWORD_STATE**: Password input with masking
- **AUTHENTICATED_STATE**: Full command access with editing capabilities

### Command System

Available commands include:
- **System Commands**: `whoami`, `uptime`, `status`, `help`, `logout`
- **LED Control**: `led on|off [1-3|all] [-t SEC]` (with timer and group/all support)
- **Multi-Sensor Monitoring**: 
  - `sensors` - Show all sensors status
  - `climate` - Temperature/humidity details with comfort analysis
  - `accel` - Detailed accelerometer with orientation analysis
- **Diagnostics**: 
  - `sensortest` - Comprehensive sensor diagnostics
  - `i2cscan` - Scan I2C bus for devices
  - `i2ctest` - Test I2C2 configuration
- **Utility Commands**: `clear`, `history`, `logs`

## ⚙️ Configuration Options

### Login Credentials
```c
#define USERNAME "admin"
#define PASSWORD "1234"
```

### Session Management
```c
#define SESSION_TIMEOUT_MS 300000  // 5 minutes
```

### Sensor Update Intervals
```c
#define SENSOR_UPDATE_INTERVAL_MS 5000    // Update sensors every 5 seconds
#define LED_UPDATE_INTERVAL_MS 100        // Update LED timers every 100ms
```

### Multi-Sensor Configuration
```c
// HDC1080 Climate Sensor
#define HDC1080_ADDRESS     0x40 << 1
typedef struct {
    float temperature;
    float humidity;
    uint8_t sensor_ok;
    uint32_t last_update;
} ClimateData_t;

// ADXL345 Accelerometer
#define ADXL345_ADDRESS     0x53 << 1
typedef struct {
    int16_t x_raw, y_raw, z_raw;
    float x_g, y_g, z_g;
    float magnitude;
    float tilt_x, tilt_y;
    uint8_t sensor_ok;
    uint32_t last_update;
} AccelData_t;
```

### System Colors
```c
#define COLOR_SUCCESS   "\033[38;5;46m"   // Soft green
#define COLOR_ERROR     "\033[38;5;196m"  // Soft red
#define COLOR_WARNING   "\033[38;5;214m"  // Soft orange
#define COLOR_INFO      "\033[38;5;81m"   // Soft cyan
#define COLOR_ACCENT    "\033[38;5;141m"  // Soft purple
```

## 🚀 Getting Started

1. **Hardware Setup**: 
   - Connect NUCLEO-F767ZI via USB
   - Wire HDC1080 and ADXL345 to I2C2 (PF0/PF1)
   - Ensure 3.3V power and pull-up resistors
2. **Build Project**: Compile and flash using STM32CubeIDE
3. **Terminal Connection**: Connect to virtual COM port at 115200 baud
4. **Login**: Use credentials `admin` / `1234`
5. **Test Sensors**: Run `sensortest` to verify hardware
6. **Explore**: Type `help` to see available commands

## 🧪 Testing and Validation

### Sensor Testing Commands
- **`sensortest`**: Comprehensive test of both HDC1080 and ADXL345
- **`i2cscan`**: Verify sensor presence on I2C bus
- **`i2ctest`**: Test I2C2 peripheral configuration
- **`sensors`**: Real-time data from all sensors
- **`climate`**: Detailed temperature/humidity with comfort analysis
- **`accel`**: Detailed accelerometer with tilt and orientation

### Functional Testing
- **Authentication**: Verify login/logout functionality and session timeout
- **Command Editing**: Test cursor movement, in-line editing, and character insertion/deletion
- **LED Control**: Confirm hardware control, timer, and group/all functionality
- **Multi-Sensor Monitoring**: Validate real-time sensor data and error handling
- **Diagnostics**: Test I2C communication and sensor initialization

### Performance Testing
- **Responsiveness**: Command execution should be immediate
- **Sensor Updates**: 5-second automatic refresh cycle
- **LED Timers**: Precise timing accuracy
- **Memory Usage**: Monitor buffer usage during extended sessions

## 📡 UART Protocol

The terminal uses standard UART communication with VT100-compatible escape sequences for:
- **Cursor Control**: `\033[A` (Up), `\033[B` (Down), `\033[C` (Right), `\033[D` (Left)
- **Screen Control**: `\033[2J` (Clear screen), `\033[K` (Clear line)
- **Color Codes**: ANSI 256-color palette for professional appearance
- **Password Masking**: Input characters displayed as asterisks

## 🌡️ Sensor Features

### Climate Monitoring (HDC1080)
- **Temperature**: ±0.2°C accuracy, -40°C to +125°C range
- **Humidity**: ±2% RH accuracy, 0-100% RH range
- **Comfort Analysis**: Automatic comfort zone detection (20-25°C, 40-60% RH)
- **Status Indicators**: Color-coded comfort status (Too Hot, Too Cold, Too Dry, Too Humid)

### Motion Sensing (ADXL345)
- **Acceleration**: ±2g range, 3-axis measurement
- **Tilt Detection**: X/Y tilt angles in degrees
- **Orientation**: Automatic orientation detection (Level, Tilted, Motion)
- **Raw Data**: 16-bit resolution with LSB conversion

### Diagnostic Features
- **I2C Bus Scanning**: Automatic device discovery
- **Communication Testing**: Protocol verification
- **Error Handling**: Graceful degradation when sensors offline
- **Live Updates**: Real-time sensor monitoring

## 🔐 Security Features

- **Password Masking**: Input characters displayed as asterisks
- **Session Timeout**: Automatic logout after inactivity
- **Command Logging**: All actions logged with timestamps and log levels
- **Input Validation**: Buffer overflow protection and command length limits
- **Activity Logging**: All login, logout, and error events are logged

## 🎨 UI Design

The terminal features a modern, professional appearance with:
- **Color Coding**: Different colors for commands, status, errors, and information
- **Professional Banner**: Startup screen with live sensor data
- **Consistent Formatting**: Structured output with clear separators
- **Live Sensor Display**: Real-time temperature, humidity, and orientation in banner
- **Diagnostic Feedback**: Detailed sensor status and troubleshooting information