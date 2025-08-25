# ESP32 IoT Orchestrator - Baseline Implementation

## Overview

This is the baseline implementation of the ESP32 IoT Orchestrator, focusing on fundamental components and schema-driven configuration. The system provides a solid foundation for IoT device management with two default sensors.

## Architecture

- **Schema-Driven Configuration**: Components define default schemas with fallbacks
- **Persistent Storage**: SPIFFS-based configuration persistence
- **Component Lifecycle**: Full component registration, initialization, and execution
- **Minimal but Complete**: Focused on core functionality without extra interfaces

## Default Components

### DHT22 Temperature/Humidity Sensor
- **Default Pin**: 15
- **Sampling Rate**: 5 seconds
- **Outputs**: Temperature (°C), Humidity (%), Heat Index
- **Schema**: Auto-generated with validation

### TSL2561 Digital Light Sensor  
- **Default I2C**: SDA=21, SCL=22, Address=0x39
- **Sampling Rate**: 2 seconds  
- **Outputs**: Lux, Broadband, Infrared readings
- **Schema**: Auto-generated with I2C configuration

## File Structure

```
src/
├── main.cpp                    # Arduino main entry point
├── core/
│   ├── Orchestrator.h         # Main orchestrator class
│   └── Orchestrator.cpp       # Component lifecycle management
├── components/
│   ├── BaseComponent.h        # Abstract base with schema support
│   ├── BaseComponent.cpp      # Base implementation
│   ├── DHT22Component.h       # DHT22 temperature/humidity
│   ├── DHT22Component.cpp     # DHT22 implementation
│   ├── TSL2561Component.h     # TSL2561 light sensor
│   └── TSL2561Component.cpp   # TSL2561 implementation
├── storage/
│   ├── ConfigStorage.h        # Persistent configuration storage
│   └── ConfigStorage.cpp      # SPIFFS-based storage
└── utils/
    ├── Logger.h               # Simple logging utility
    └── Logger.cpp             # Serial-based logging
```

## Key Features

### 1. Schema-Driven Components
Each component defines a default schema with sensible defaults:
- DHT22: Pin 15, 5-second intervals, Celsius
- TSL2561: I2C pins 21/22, address 0x39, 2-second intervals

### 2. Configuration Persistence
- Components save/load configurations via SPIFFS
- Fallback to defaults when no saved configuration exists
- JSON-based configuration with validation

### 3. Component Lifecycle
- Registration → Initialization → Ready → Executing → Cleanup
- Error handling with automatic recovery attempts
- Real-time execution scheduling

### 4. System Monitoring
- Component health tracking
- Execution statistics and error counts
- Memory usage monitoring
- Uptime and system status

## Hardware Setup

### DHT22 Connection
```
DHT22 Pin 1 (VCC) → ESP32 3.3V
DHT22 Pin 2 (Data) → ESP32 GPIO 15
DHT22 Pin 3 (NC) → Not connected
DHT22 Pin 4 (GND) → ESP32 GND
Add 10kΩ pull-up resistor between pins 1 and 2
```

### TSL2561 Connection
```
TSL2561 VCC → ESP32 3.3V
TSL2561 GND → ESP32 GND
TSL2561 SDA → ESP32 GPIO 21
TSL2561 SCL → ESP32 GPIO 22
TSL2561 ADDR → GND (for address 0x39)
```

## Building and Flashing

### Prerequisites
- PlatformIO installed
- ESP32 board support
- USB cable for flashing

### Build Commands
```bash
# Build the project
pio run

# Upload to ESP32
pio run -t upload

# Monitor serial output
pio device monitor
```

### Expected Output
```
=== ESP32 IoT Orchestrator Logger Initialized ===
Log level: INFO
================================================

[00:00:00.123][INFO][main] ESP32 IoT Orchestrator - Baseline v1.0.0
[00:00:00.145][INFO][main] Starting system initialization...
[00:00:00.167][INFO][Orchestrator] Initializing ESP32 IoT Orchestrator...
[00:00:00.234][INFO][ConfigStorage] ConfigStorage initialized successfully
[00:00:00.456][INFO][DHT22:dht22-1] DHT22 initialized on pin 15, interval=5000ms
[00:00:00.567][INFO][TSL2561:tsl2561-1] TSL2561 initialized - I2C addr=0x39, SDA=21, SCL=22
[00:00:00.678][INFO][Orchestrator] System started with 2 components
[00:00:00.789][INFO][main] Entering main loop...

[00:00:05.123][INFO][DHT22:dht22-1] T=22.5°C, H=45.2%
[00:00:07.234][INFO][TSL2561:tsl2561-1] Lux=150.2, BB=1024, IR=256
...
```

## Configuration

### Default Behavior
- Both sensors use default configurations on first boot
- Configurations are saved to SPIFFS after successful initialization
- Subsequent boots load saved configurations

### Custom Configuration
To customize sensor settings, modify the initialization in `Orchestrator::initializeDefaultComponents()`:

```cpp
// Custom DHT22 configuration
JsonDocument dhtConfig;
dhtConfig["pin"] = 16;              // Use pin 16 instead of 15
dhtConfig["samplingIntervalMs"] = 10000;  // 10-second interval
dhtConfig["fahrenheit"] = true;     // Report in Fahrenheit

DHT22Component* dht = new DHT22Component("dht22-1", "Custom DHT22");
dht->initialize(dhtConfig);
```

## Development Guidelines

### Adding New Components
1. Inherit from `BaseComponent`
2. Implement pure virtual methods:
   - `getDefaultSchema()` - Define component schema with defaults
   - `initialize()` - Apply configuration and setup hardware
   - `execute()` - Main component function
   - `cleanup()` - Resource cleanup

### Schema Format
Components must provide JSON schemas with default values:
```cpp
JsonDocument MyComponent::getDefaultSchema() const {
    JsonDocument schema;
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["type"] = "object";
    
    JsonArray required = schema.createNestedArray("required");
    required.add("pin");
    
    JsonObject properties = schema.createNestedObject("properties");
    JsonObject pinProp = properties.createNestedObject("pin");
    pinProp["type"] = "integer";
    pinProp["default"] = 15;  // Default value
    
    return schema;
}
```

## Troubleshooting

### Common Issues

#### DHT22 Not Reading
- Check wiring and pull-up resistor
- Verify pin assignment (default: GPIO 15)
- DHT22 requires 2-second delays between readings

#### TSL2561 Not Found
- Check I2C wiring (SDA=21, SCL=22)
- Verify I2C address (0x39 with ADDR pin to GND)
- Try I2C scanner to detect device

#### Memory Issues
- Monitor heap usage in serial output
- System warns when free heap < 10KB
- Use `getSystemStats()` for detailed memory info

#### Configuration Issues
- Check SPIFFS initialization in serial output
- Use `formatStorage()` to clear all configurations
- Default values are used when saved config is invalid

### Debug Logging
Enable verbose logging by changing Logger level in `main.cpp`:
```cpp
Logger::init(Logger::DEBUG);  // Instead of Logger::INFO
```

## Testing

### Manual Testing
1. Flash firmware and monitor serial output
2. Verify both sensors initialize successfully
3. Observe periodic sensor readings
4. Check system statistics every 30 seconds

### Integration Testing
- Components should execute on schedule
- Error states should recover automatically  
- Memory usage should remain stable
- Configuration should persist across reboots

## Next Steps

This baseline implementation provides the foundation for:
- Web interface development
- Additional sensor/actuator components
- MQTT integration
- REST API implementation
- Advanced scheduling features

The architecture supports easy extension while maintaining the core schema-driven configuration approach.

---

**Version**: 1.0.0  
**Hardware**: ESP32 DevKit V1  
**Framework**: Arduino/ESP-IDF via PlatformIO