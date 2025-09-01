# ESP32 Configuration Persistence Architecture

## Architecture Implementation Complete

This document demonstrates the configuration persistence architecture that has been implemented for all ESP32 IoT components.

## Architecture Overview

The configuration persistence system uses a **Template Method Pattern** with clear separation of concerns:

- **Base Class (`BaseComponent`)**: Handles LittleFS storage orchestration
- **Child Classes**: Handle data serialization/deserialization and default hydration

## Key Components

### 1. Storage Layer (`ConfigStorage`)
- Manages LittleFS filesystem operations
- Stores component configurations in `/config/components/{componentId}.json`
- Provides atomic save/load operations with error handling

### 2. Base Component Architecture
```cpp
// BaseComponent.h - Virtual methods for child classes
class BaseComponent {
protected:
    virtual JsonDocument getCurrentConfig() const = 0;
    virtual bool applyConfig(const JsonDocument& config) = 0;
    
public:
    bool saveCurrentConfiguration();
    bool loadStoredConfiguration();
};
```

### 3. Implementation in All Components
All components now implement the virtual methods:

- **ServoDimmerComponent**: IP address, port, lumens, timeouts, movement settings
- **DHT22Component**: Pin configuration, sampling intervals, temperature units
- **TSL2561Component**: I2C settings, gain, integration time
- **LightOrchestrator**: Lighting modes, sensor mappings, adjustment parameters
- **WebServerComponent**: Server port, CORS settings, server name

## Configuration Persistence Flow

### 1. API Configuration Update
```bash
# POST to update servo dimmer IP
curl -X POST "http://192.168.1.152/api/components/servo-dimmer-1/config" \
     -d "device_ip=192.168.1.200"
```

### 2. Backend Processing
```cpp
// WebServerComponent::handleComponentConfig()
if (component->applyConfig(configData)) {
    if (component->saveCurrentConfiguration()) {
        // Success: Configuration saved to LittleFS
        response["message"] = "Configuration updated and saved to persistent storage";
    }
}
```

### 3. Component Implementation
```cpp
// ServoDimmerComponent::getCurrentConfig()
JsonDocument ServoDimmerComponent::getCurrentConfig() const {
    JsonDocument config;
    config["device_ip"] = m_deviceIP;
    config["device_port"] = m_devicePort;
    config["base_lumens"] = m_baseLumens;
    config["config_version"] = 1;
    return config;
}

// ServoDimmerComponent::applyConfig() with default hydration
bool ServoDimmerComponent::applyConfig(const JsonDocument& config) {
    m_deviceIP = config["device_ip"] | "192.168.1.161";
    m_devicePort = config["device_port"] | 80;
    m_baseLumens = config["base_lumens"] | 1000.0f;
    // Handle version migration...
    return true;
}
```

### 4. Persistent Storage
Configuration is saved to LittleFS at:
```
/config/components/servo-dimmer-1.json
{
  "device_ip": "192.168.1.200",
  "device_port": 80,
  "base_lumens": 1000.0,
  "config_version": 1
}
```

### 5. Boot-time Recovery
```cpp
// On component initialization
bool ServoDimmerComponent::initialize(const JsonDocument& config) {
    // Load from persistent storage if available
    if (!loadStoredConfiguration()) {
        // Fall back to defaults if no stored config
        JsonDocument defaults;
        applyConfig(defaults);  // Uses default hydration
    }
    return true;
}
```

## Configuration Evolution Support

### Version Migration Framework
```cpp
bool applyConfig(const JsonDocument& config) {
    // Handle version migration
    uint16_t configVersion = config["config_version"] | 1;
    if (configVersion < 2) {
        log("Migrating configuration from v1 to v2");
        // Add new settings with defaults
        // Migrate old settings if needed
    }
    return true;
}
```

### Default Hydration
All components use the `|` operator for safe default assignment:
```cpp
m_deviceIP = config["device_ip"] | "192.168.1.161";  // Falls back to default
m_newSetting = config["new_setting"] | "default_value";  // New setting added safely
```

## API Endpoints

### Configuration Management
- **GET** `/api/components/{id}/config` - Retrieve current configuration
- **POST** `/api/components/{id}/config` - Update and persist configuration

### Response Format
```json
{
  "success": true,
  "message": "Configuration updated and saved to persistent storage",
  "component_id": "servo-dimmer-1",
  "configuration": {
    "device_ip": "192.168.1.200",
    "device_port": 80,
    "base_lumens": 1000.0,
    "config_version": 1
  }
}
```

## Testing Workflow

To demonstrate configuration persistence:

1. **Get Current Config**:
   ```bash
   curl "http://192.168.1.152/api/components/servo-dimmer-1/config"
   ```

2. **Change IP Address**:
   ```bash
   curl -X POST "http://192.168.1.152/api/components/servo-dimmer-1/config" \
        -d "device_ip=192.168.1.200"
   ```

3. **Verify Storage**: Configuration is automatically saved to LittleFS

4. **Reboot Device**: `curl -X POST "http://192.168.1.152/api/system/reboot"`

5. **Verify Persistence**: IP address persists across reboot
   ```bash
   curl "http://192.168.1.152/api/components/servo-dimmer-1/config"
   ```

6. **Change Back**:
   ```bash
   curl -X POST "http://192.168.1.152/api/components/servo-dimmer-1/config" \
        -d "device_ip=192.168.1.161"
   ```

## Benefits

✅ **Separation of Concerns**: Base class handles storage, child classes handle data mapping  
✅ **Configuration Evolution**: Version tracking enables firmware updates without config loss  
✅ **Default Hydration**: Missing settings automatically get sensible defaults  
✅ **Atomic Operations**: Configuration save/load operations are atomic and safe  
✅ **Universal Architecture**: All components follow the same persistence pattern  
✅ **API Integration**: Web API automatically triggers persistent storage  

## Architecture Advantages

1. **Maintainable**: Clear separation between storage logic and data mapping
2. **Extensible**: Easy to add new configuration parameters without breaking existing setups
3. **Robust**: Handles missing configurations, version upgrades, and storage failures gracefully
4. **Consistent**: All components use the same architecture pattern
5. **Debuggable**: Extensive logging for configuration operations

This architecture ensures that all ESP32 component configurations persist across reboots, firmware updates, and device resets while maintaining backward compatibility and supporting configuration evolution.