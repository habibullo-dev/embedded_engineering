# STM32F767ZI Embedded Systems Learning Journey

> **Professional embedded development progression from basic GPIO control to advanced interrupt-driven programming**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![STM32](https://img.shields.io/badge/STM32-F767ZI-orange.svg)](https://www.st.com/en/microcontrollers-microprocessors/stm32f767zi.html)
[![HAL](https://img.shields.io/badge/HAL-Programming-green.svg)](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)

## 🎯 Project Mission

This repository documents a comprehensive embedded systems learning journey, progressing from fundamental GPIO operations to advanced interrupt-driven architectures. Each implementation demonstrates professional development practices while building essential embedded programming skills.

## 📋 Learning Implementations

### 🔄 Polling-Based Implementation
**Foundation Level - Direct Hardware Control**

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

### ⚡ Interrupt-Driven Implementation  
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

## 🛠️ Hardware Platform

**Target Board**: NUCLEO-F767ZI Development Board
- **Microcontroller**: STM32F767ZI (Cortex-M7, 216MHz)
- **LEDs**: 3x onboard (Green PB0, Blue PB7, Red PB14)
- **Button**: User button PC13 (active LOW)
- **Programmer**: Integrated ST-Link

## 🔧 Development Environment

- **IDE**: STM32CubeIDE 1.8+
- **Configuration**: STM32CubeMX (integrated)
- **Library**: STM32F7 HAL
- **Toolchain**: ARM GCC

## 📊 Performance Comparison

| Metric | Polling | Interrupt |
|--------|---------|-----------|
| **Response Time** | 0-20ms | <1ms |
| **CPU Efficiency** | 90% checking | 99% idle |
| **Event Detection** | Can miss brief events | Never misses |
| **Power Consumption** | Higher | Lower |
| **Code Complexity** | Simple | Advanced |

## 🎓 Key Learning Outcomes

### Hardware Abstraction Layer (HAL)
- Direct GPIO configuration without BSP dependencies
- Clock management and peripheral initialization
- Portable code design principles

### State Machine Design
- Finite state machine implementation
- Non-blocking timing techniques
- Sequential behavior management

### Interrupt Programming
- EXTI configuration and NVIC management
- ISR design best practices
- Safe inter-context communication

### Professional Practices
- Version control with Git
- Documentation standards
- Code organization and modularity

## 🚀 Quick Start

### Clone and Build
```bash
git clone https://github.com/habibullo-dev/embedded_engineering
cd /embedded_engineering

# Import to STM32CubeIDE
# Build and program to NUCLEO-F767ZI
```

### Test Functionality
1. **Power On**: All LEDs off
2. **Button Press**: Starts LED sequence (Green → Blue → Red → Off)
3. **During Sequence**: Button press stops immediately
4. **Responsiveness**: Compare polling vs interrupt versions

## 🌟 Repository Highlights

### Branch Structure
- **`main`**: Complete implementations and documentation
- **`feature/polling-implementation`**: Polling-based development
- **`feature/interrupt-implementation`**: Interrupt-driven development
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