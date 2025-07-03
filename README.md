# STM32F767ZI Embedded Systems Learning Journey

> **Professional embedded development progression from basic GPIO control to advanced system programming**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![STM32](https://img.shields.io/badge/STM32-F767ZI-orange.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f767zi.html)
[![HAL](https://img.shields.io/badge/HAL-Programming-green.svg)](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)

## 🎯 Project Mission

This repository documents a comprehensive embedded systems learning journey, progressing from fundamental GPIO operations to advanced interrupt-driven architectures and system programming. Each implementation demonstrates professional development practices while building essential embedded programming skills.

## 📋 Learning Implementations

### 🔄 [Polling-Based LED Controller](https://github.com/habibullo-dev/embedded_engineering/tree/feature/polling-implementation)
**Foundation Level - State Machine & Hardware Control**

- **Core Concepts**: HAL GPIO programming, state machines, non-blocking timing
- **Architecture**: Sequential main loop with continuous input monitoring
- **Key Skills**: Hardware abstraction, debouncing algorithms, timing precision
- **Response Time**: 0-20ms variable latency

```c
// Polling approach - active monitoring
while (1) {
    if (read_button_state_polling()) {
        handle_button_press();
    }
    LED_Sequence_Update();
    HAL_Delay(10);
}
```

### ⚡ [Interrupt-Driven LED Controller](https://github.com/habibullo-dev/embedded_engineering/tree/feature/interrupt-implementation)
**Advanced Level - Event-Driven Architecture**

- **Core Concepts**: GPIO interrupts, NVIC management, ISR design
- **Architecture**: Event-driven with dual execution contexts
- **Key Skills**: Interrupt handling, volatile variables, atomic operations
- **Response Time**: <1ms immediate hardware response

```c
// Interrupt approach - hardware-driven events
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    button_comm.button_event_pending = 1;  // Signal main context
}
```

### 🖥️ [Professional Shell Interface](https://github.com/habibullo-dev/embedded_engineering/tree/feature/shell-implementation)
**System Level - UART Communication & Command Processing**

- **Core Concepts**: UART programming, command parsing, session management
- **Architecture**: Interrupt-driven UART with command-line interface
- **Key Skills**: String processing, circular buffers, terminal protocols
- **Features**: Authentication, command history, LED control, system monitoring

```c
// Advanced shell with full editing capabilities
void process_character(char c) {
    if (c == '\r' || c == '\n') {
        process_command();
    } else if (c == 127 || c == 8) {
        delete_char_at_cursor();
    } else {
        insert_char_at_cursor(c);
    }
}
```

## 🛠️ Hardware Platform

**Target Board**: NUCLEO-F767ZI Development Board
- **Microcontroller**: STM32F767ZI (Cortex-M7, 216MHz)
- **LEDs**: 3x onboard (Green PB0, Blue PB7, Red PB14)
- **Button**: User button PC13 (active LOW)
- **Communication**: UART4 for terminal interface
- **Programmer**: Integrated ST-Link

## 🔧 Development Environment

- **IDE**: STM32CubeIDE 1.8+
- **Configuration**: STM32CubeMX (integrated)
- **Library**: STM32F7 HAL
- **Toolchain**: ARM GCC

## 📊 Implementation Comparison

| Feature | Polling | Interrupt | Shell |
|---------|---------|-----------|-------|
| **Response Time** | 0-20ms | <1ms | <1ms |
| **CPU Efficiency** | 90% checking | 99% idle | 95% idle |
| **Event Detection** | Can miss brief events | Never misses | Never misses |
| **User Interface** | LED only | LED only | Full CLI |
| **Code Complexity** | Simple | Advanced | Professional |
| **System Features** | Basic | Basic | Authentication, Logging |

## 🎓 Key Learning Outcomes

### Hardware Abstraction Layer (HAL)
- Direct GPIO configuration without BSP dependencies
- Clock management and peripheral initialization
- UART communication and interrupt handling

### State Machine Design
- Finite state machine implementation
- Non-blocking timing techniques
- Sequential behavior management

### Interrupt Programming
- EXTI configuration and NVIC management
- ISR design best practices
- Safe inter-context communication

### System Programming
- Command-line interface development
- Session management and authentication
- Real-time data processing and logging

### Professional Practices
- Version control with Git branching
- Documentation standards
- Code organization and modularity

## 🚀 Quick Start

### Clone and Explore
```bash
git clone https://github.com/habibullo-dev/embedded_engineering
cd embedded_engineering

# Explore different implementations
git checkout feature/polling-implementation       # Basic GPIO control
git checkout feature/interrupt-implementation     # Advanced interrupts
git checkout feature/shell-implementation         # Professional shell

# Import to STM32CubeIDE and build
```

### Test Each Implementation
1. **Polling LED Controller**: Button controls LED progression
2. **Interrupt Version**: Immediate response testing
3. **Shell Interface**: Connect via UART at 115200 baud, login with `admin/1234`

## 🌟 Repository Structure

### Branch Organization
- **`main`**: Complete documentation and project overview
- **`feature/polling-implementation`**: [Polling-based LED control](https://github.com/habibullo-dev/embedded_engineering/tree/feature/polling-implementation)
- **`feature/interrupt-implementation`**: [Interrupt-driven development](https://github.com/habibullo-dev/embedded_engineering/tree/feature/interrupt-implementation)
- **`feature/shell-implementation`**: [Professional shell system](https://github.com/habibullo-dev/embedded_engineering/tree/feature/shell-implementation)
- **`experiment/*`**: Various timing and configuration experiments

### Documentation
- Comprehensive README files for each implementation
- Code comments explaining embedded concepts
- Performance analysis and comparisons
- Professional Git workflow examples

### Code Quality
- HAL-based programming (no BSP dependencies)
- Professional commenting and structure
- Configurable timing parameters
- Error handling and validation

## 🎯 Next Steps in Learning Journey

This foundation prepares for advanced embedded topics:

- **Timer Interrupts**: Hardware-based precise timing
- **Communication Protocols**: UART, SPI, I2C implementation
- **DMA Operations**: Direct memory access for efficiency
- **FreeRTOS Integration**: Real-time operating system concepts
- **Low Power Modes**: Energy optimization techniques
- **Bootloader Development**: Firmware update mechanisms