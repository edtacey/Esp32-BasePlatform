# TSL2561 Light Sensor Test Harness MVP

A comprehensive test harness for validating TSL2561 digital light sensor hardware on ESP32 platforms using PlatformIO.

## Hardware Requirements

- ESP32 development board
- TSL2561 digital light sensor breakout board
- Jumper wires for I2C connection

## Wiring

| TSL2561 Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| VCC         | 3.3V      | Power supply |
| GND         | GND       | Ground |
| SDA         | GPIO 21   | I2C Data |
| SCL         | GPIO 22   | I2C Clock |
| ADDR        | Float/GND | I2C Address (0x39 default) |

## Features

### Hardware Validation
- I2C communication testing
- Device identification verification
- Power management validation
- Register access verification
- Wiring validation

### Sensor Testing
- Raw channel readings (CH0/CH1)
- Lux calculation validation
- Gain setting tests (1x, 16x)
- Integration time tests (13ms, 101ms, 402ms)
- Auto-gain adjustment
- Saturation detection
- Reading averaging

### Known Configuration Testing
- Validates sensor with proven settings
- Tests against expected behavior patterns
- Calibration verification

### Diagnostics
- I2C address scanning
- Register dump functionality
- Performance measurement
- Continuous monitoring mode

## Build Instructions

### Prerequisites

1. Install PlatformIO CLI or use PlatformIO IDE
2. Install Node.js (for Archon MCP server)
3. Clone/copy this project

### Archon MCP Setup

This project uses Archon MCP server for task management and knowledge integration:

```bash
# Install Archon MCP globally
npm run setup-archon

# Verify Archon is configured (restart Claude Code after installation)
# The .clauderc file will automatically load Archon when Claude Code starts
```

### Hardware Setup

1. Connect your hardware according to the wiring diagram
2. Build and upload:

```bash
# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

## Configuration

Edit `platformio.ini` to modify:

- I2C pins (SDA/SCL)
- I2C frequency
- Target I2C address
- Serial monitor settings

## Test Results

The test harness provides detailed output including:

```
=== TSL2561 Test Harness MVP ===
Version: 1.0.0
================================

I2C device found at address 0x39
Device ID: 0x50

[PASS] I2C Communication
[PASS] Device Identification  
[PASS] Power Management
[PASS] Register Access
[PASS] Sensor Readings
[PASS] Gain Settings
[PASS] Integration Times
[PASS] Auto Gain
[PASS] Saturation Detection
[PASS] Averaging
[PASS] Known Configuration
[PASS] Calibration

==================================================
 Test Results Summary
==================================================
Total Tests:    12
Passed:         12
Failed:         0
Warnings:       0
Skipped:        0
Duration:       3456 ms
Pass Rate:      100.0%
STATUS: ALL TESTS PASSED! ✓
```

## Continuous Monitoring

After test completion, the system enters continuous monitoring mode:

```
[MONITOR] Lux: 245.67 | Ch0: 1234 | Ch1: 567 | Gain: 1x | OK
[MONITOR] Lux: 248.12 | Ch0: 1245 | Ch1: 571 | Gain: 1x | OK
```

## Troubleshooting

### No I2C devices found
- Check wiring connections
- Verify power supply (3.3V)
- Ensure pull-up resistors on I2C lines (usually built into breakout boards)

### Device ID mismatch
- Verify TSL2561 vs TSL2560 (different device IDs)
- Check for counterfeit sensors

### Readings always zero
- Check lighting conditions
- Verify sensor is not covered
- Test different gain settings

### High variance in readings
- Shield from vibrations
- Allow sensor to stabilize
- Check power supply stability

## Advanced Usage

### Custom Test Configuration

Modify test parameters in `TestHarness.cpp`:

```cpp
_config.enable_stress_tests = true;  // Enable stress testing
_config.verbose_output = false;      // Reduce output verbosity
_config.test_timeout_ms = 10000;     // Extend test timeout
```

### Integration with Other Systems

The driver can be used independently:

```cpp
#include "TSL2561Driver.h"

TSL2561Driver sensor;
sensor.begin(0x39);

TSL2561Reading reading;
if (sensor.readSensor(reading)) {
    Serial.printf("Lux: %.2f\n", reading.lux);
}
```

## Known Configurations

The test harness validates against these proven configurations:

- **Standard Config**: 402ms integration, 1x gain, auto-gain enabled
- **Low Light Config**: 402ms integration, 16x gain
- **Fast Config**: 13ms integration, 1x gain

## Archon Integration

This project is configured to work with Archon MCP server for:

- **Task Management**: Automated task creation and tracking
- **Knowledge Integration**: RAG queries for hardware documentation
- **Code Examples**: Search for relevant implementation patterns
- **Project Organization**: Structured development workflow

### Archon Workflow

When Archon is available, the development workflow follows:

1. **Task-Driven Development**: All work organized around specific tasks
2. **Research-First Approach**: Query knowledge base before implementation
3. **Systematic Validation**: Each task includes verification steps
4. **Progress Tracking**: Automated status updates and completion tracking

### Without Archon

If Archon MCP is not available, the project falls back to standard Claude Code workflow using TodoWrite for basic task tracking.

## License

This test harness is designed for hardware validation and development purposes.