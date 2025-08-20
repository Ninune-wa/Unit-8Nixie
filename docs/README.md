# ATtiny402 8-Tube Nixie Display Controller

## Overview

This project implements a sophisticated 8-tube Nixie display controller using an ATtiny402 microcontroller with HV5523/HV513 high-voltage shift register drivers. The system provides advanced features including smooth cross-fade transitions, random shuffling effects, dot display control, and comprehensive I2C command interface.

## Hardware Specifications

### Microcontroller
- **MCU**: ATtiny402
- **Clock Frequency**: 20MHz
- **Operating Voltage**: 5V
- **I2C Address**: 0x2A (42 decimal)

### Display Drivers
- **Primary Driver**: HV5523 (High-voltage shift register)
- **Secondary Driver**: HV513 (High-voltage shift register)
- **Output Voltage**: Up to 170V for Nixie tube operation
- **Tube Count**: 8 tubes (numbered 0-7 from left to right)

### Display Capabilities
- **Digit Display**: 0-9 on each tube
- **Dot Display**: Left and Right dots on each tube (L/R dot control)
- **Cross-fade**: Smooth transitions between digits
- **Random Shuffle**: High-speed random digit cycling
- **Individual Control**: Each tube can be controlled independently

## Key Features

### 1. High-Speed Multiplexing
- **ISR Frequency**: 1320Hz interrupt-driven display refresh
- **Flicker-Free**: Ultra-fast time-division multiplexing eliminates visible flicker
- **Phase Distribution**: Each tube uses different phase offsets for optimal visual smoothness

### 2. Cross-Fade Effects
- **Smooth Transitions**: Gradual brightness transitions between old and new digits
- **Configurable Speed**: 11 speed levels (10ms to 500ms intervals)
- **Configurable Steps**: 1-20 transition steps
- **Default Settings**: 5 steps × 50ms = 250ms total transition time

### 3. Random Shuffle
- **High-Quality Random**: Linear congruential generator for pseudo-random numbers
- **Independent Processing**: Shuffle processing in main loop, independent of ISR
- **Speed Control**: Adjustable shuffle speed (10-200 range, approximately 8-150ms intervals)
- **Auto-Stop**: Automatic shuffle termination when digit commands are executed
- **Emergency Stop**: Immediate shuffle termination command available

### 4. Dot Display Control
- **Dual Dots**: Left and Right dots on each tube
- **Independent Control**: Each dot can be controlled separately
- **Clock Display**: Perfect for colon (:) display in time applications
- **Pattern Effects**: Support for various dot patterns and animations

## I2C Command Interface

### Basic Digit Commands

#### Global Digit Control
| Command | Function | Description |
|---------|----------|-------------|
| `0x00-0x09` | Set all tubes to digit 0-9 | Sets all 8 tubes to the same digit with cross-fade |

#### Individual Tube Control
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x10-0x17` | digit (0-9) | Set tube 0-7 to specific digit | Individual tube control with cross-fade |

### Cross-Fade Configuration

#### Global Step Count Configuration
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x60` | steps (1-10) | Set cross-fade steps for all tubes | Configure number of transition steps |

#### Individual Step Count Configuration
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x61-0x68` | steps (1-10) | Set cross-fade steps for tube 0-7 | Individual tube step configuration |

#### Speed Configuration
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x70` | speed (0-6) | Set cross-fade speed | Configure transition speed |

**Speed Table:**
- `0x70 + 0`: Fast (~26ms interval)
- `0x70 + 1`: Moderately fast (~33ms interval)
- `0x70 + 2`: **Standard (~66ms interval)** (Default)
- `0x70 + 3`: Moderately slow (~132ms interval)
- `0x70 + 4`: Slow (~198ms interval)
- `0x70 + 5`: Very slow (~264ms interval)
- `0x70 + 6`: Ultra-slow (~330ms interval)

### Random Shuffle Commands

#### Shuffle Speed Configuration
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x71` | speed (10-200) | Set shuffle speed | Configure shuffle interval (Default: 26 ≈ 20ms) |

#### Global Shuffle Control
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x72` | - | Start global shuffle | Begin shuffling all tubes |
| `0x73` | digit (0-9) | Stop global shuffle | Stop all tubes and set to specified digit |

#### Individual Shuffle Control
| Command | Function | Description |
|---------|----------|-------------|
| `0x74-0x7B` | Start tube 0-7 shuffle | Begin shuffling individual tube |
| `0x7C` | Emergency stop | Immediately stop all shuffles |

#### Individual Shuffle Stop
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0xA0-0xA7` | digit (0-9) | Stop tube 0-7 shuffle | Stop individual tube and set to specified digit |

### Dot Display Commands

#### Individual Dot Control
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x20-0x27` | state (0/1) | Tube 0-7 left dot | Control individual left dots |
| `0x30-0x37` | state (0/1) | Tube 0-7 right dot | Control individual right dots |

#### Global Dot Control
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x80` | state (0/1) | All tubes left dot | Control all left dots (0=off, 1=on) |
| `0x81` | state (0/1) | All tubes right dot | Control all right dots (0=off, 1=on) |

### Tube Extinguishing Commands

#### Global Dot Extinguishing
| Command | Function | Description |
|---------|----------|-------------|
| `0xBF` | All tubes left dot off | Turn off all left dots |
| `0xCF` | All tubes right dot off | Turn off all right dots |
| `0xDF` | All tubes LR dots off | Turn off all left and right dots |

#### Individual Dot Extinguishing
| Command | Function | Description |
|---------|----------|-------------|
| `0xD1-0xD7` | Tube 0-6 LR dots off | Turn off left and right dots for individual tubes |

#### Digit Extinguishing
| Command | Function | Description |
|---------|----------|-------------|
| `0xE0-0xE7` | Tube 0-7 digit off | Turn off digit display for individual tubes |
| `0xEF` | All tubes digits off | Turn off all digit displays |

#### Complete Extinguishing
| Command | Function | Description |
|---------|----------|-------------|
| `0xF0-0xF7` | Tube 0-7 complete off | Turn off digit and LR dots for individual tubes |
| `0xFF` | All tubes complete off | Turn off all digits and LR dots |

### System Commands

#### Lock Control
| Command | Parameters | Function | Description |
|---------|------------|----------|-------------|
| `0x4D` | `0x53`, `0x58` | Unlock system | Unlock with "MSX" sequence |

## Auto-Stop Feature

The system automatically stops shuffling when digit setting commands are executed:
- Global digit commands (`0x00-0x09`) automatically stop all shuffles
- Individual digit commands (`0x10-0x17`) automatically stop shuffle for the specific tube
- This provides seamless transition from shuffle mode to normal display mode

## Development Environment

### PlatformIO Configuration
```ini
[env:ATtiny402]
platform = atmelmegaavr
board = ATtiny402
framework = arduino
lib_deps = simsso/ShiftRegister74HC595@^1.3.1
upload_protocol = serialupdi
upload_port = /dev/cu.usbserial-110
upload_speed = 57600
board_build.f_cpu = 20000000L
```

### Dependencies
- **ShiftRegister74HC595**: Library for shift register control
- **Wire**: I2C communication library
- **Arduino Framework**: Core functionality

## License

This project is open source. Please refer to the license file for details.
