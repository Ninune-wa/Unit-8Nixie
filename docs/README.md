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

#### Step Count Configuration
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x60` | steps (1-20) | Set cross-fade steps | Configure number of transition steps |

#### Speed Configuration
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x70` | speed (0-10) | Set cross-fade speed | Configure transition speed |

**Speed Table:**
- `0x70 + 0`: Ultra-fast (~10ms interval) → 5 steps = ~50ms
- `0x70 + 1`: Very fast (~20ms interval) → 5 steps = ~100ms
- `0x70 + 2`: Fast (~25ms interval) → 5 steps = ~125ms
- `0x70 + 3`: Moderately fast (~40ms interval) → 5 steps = ~200ms
- `0x70 + 4`: **Standard (~50ms interval) → 5 steps = ~250ms** (Default)
- `0x70 + 5`: Moderately slow (~60ms interval) → 5 steps = ~300ms
- `0x70 + 6`: Slow (~100ms interval) → 5 steps = ~500ms
- `0x70 + 7`: Very slow (~150ms interval) → 5 steps = ~750ms
- `0x70 + 8`: Ultra-slow (~200ms interval) → 5 steps = ~1 second
- `0x70 + 9`: Extremely slow (~250ms interval) → 5 steps = ~1.25 seconds
- `0x70 + 10`: Maximum slow (~500ms interval) → 5 steps = ~2.5 seconds

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
| `0x7C` | Emergency stop | Immediately stop all shuffles (debug use) |

#### Individual Shuffle Stop
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0xA0-0xA7` | digit (0-9) | Stop tube 0-7 shuffle | Stop individual tube and set to specified digit |

### Dot Display Commands

#### Global Dot Control
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x80` | state (0/1) | All tubes left dot | Control all left dots (0=off, 1=on) |
| `0x81` | state (0/1) | All tubes right dot | Control all right dots (0=off, 1=on) |

#### Individual Dot Control
| Command | Parameter | Function | Description |
|---------|-----------|----------|-------------|
| `0x90-0x97` | state (0/1) | Tube 0-7 left dot | Control individual left dots |
| `0x98-0x9F` | state (0/1) | Tube 0-7 right dot | Control individual right dots |

### Special Commands

#### System Commands
| Command | Function | Description |
|---------|----------|-------------|
| `0xFF` | Timing test | Internal timing accuracy test (development use) |

## Auto-Stop Feature

The system automatically stops shuffling when digit setting commands are executed:
- Global digit commands (`0x00-0x09`) automatically stop all shuffles
- Individual digit commands (`0x10-0x17`) automatically stop shuffle for the specific tube
- This provides seamless transition from shuffle mode to normal display mode

### Clock Display Example
```c
// Display "12:34:56" with blinking colons
// Set digits
Wire.beginTransmission(0x2A); Wire.write(0x10); Wire.write(1); Wire.endTransmission(); // Tube 0: 1
Wire.beginTransmission(0x2A); Wire.write(0x11); Wire.write(2); Wire.endTransmission(); // Tube 1: 2
Wire.beginTransmission(0x2A); Wire.write(0x12); Wire.write(3); Wire.endTransmission(); // Tube 2: 3
Wire.beginTransmission(0x2A); Wire.write(0x13); Wire.write(4); Wire.endTransmission(); // Tube 3: 4
Wire.beginTransmission(0x2A); Wire.write(0x14); Wire.write(5); Wire.endTransmission(); // Tube 4: 5
Wire.beginTransmission(0x2A); Wire.write(0x15); Wire.write(6); Wire.endTransmission(); // Tube 5: 6

// Blink colons (tube 2 and 4 right dots)
Wire.beginTransmission(0x2A); Wire.write(0x9A); Wire.write(1); Wire.endTransmission(); // Tube 2 right dot on
Wire.beginTransmission(0x2A); Wire.write(0x9C); Wire.write(1); Wire.endTransmission(); // Tube 4 right dot on
delay(1000);
Wire.beginTransmission(0x2A); Wire.write(0x9A); Wire.write(0); Wire.endTransmission(); // Tube 2 right dot off
Wire.beginTransmission(0x2A); Wire.write(0x9C); Wire.write(0); Wire.endTransmission(); // Tube 4 right dot off
```

## Technical Implementation Details

### ISR (Interrupt Service Routine)
- **Frequency**: 1320Hz (TCA0.SINGLE.PER = 1249)
- **Function**: High-speed multiplexing and cross-fade processing
- **Phase Management**: Each tube uses unique phase offset for smooth operation
- **Brightness Control**: PWM-like time-ratio control for cross-fade effects

### Random Number Generation
- **Algorithm**: Linear Congruential Generator (LCG)
- **Formula**: `next = (prev * 1103515245 + 12345) & 0x7FFFFFFF`
- **Quality**: High-quality pseudo-random sequence suitable for visual effects

### Memory Usage
- **Program Memory**: Optimized for ATtiny402's limited flash
- **RAM Usage**: Efficient state management for 8 tubes
- **Variables**: Tube states, cross-fade parameters, shuffle states, dot states

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

