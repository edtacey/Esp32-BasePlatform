# ESP32 IoT Orchestrator - System Architecture Document

## 1. Executive Summary

The ESP32 IoT Orchestrator is a modular, component-based IoT platform designed to manage diverse sensors, actuators, and services through a unified orchestration layer. The system provides dynamic component registration, JSON-based configuration, web-based management, and both scheduled and on-demand execution models.

## 2. System Overview

### 2.1 Core Design Principles
- **Modularity**: Component-based architecture with pluggable modules
- **Flexibility**: Runtime configuration without recompilation
- **Scalability**: Support for unlimited component instances
- **Efficiency**: Optimized for ESP32 resource constraints
- **Interoperability**: JSON-based communication protocol
- **Maintainability**: Clear separation of concerns

### 2.2 System Context
```
┌─────────────────────────────────────────────────────────┐
│                    External Systems                      │
│  (MQTT Brokers, Cloud Services, Mobile Apps, Web UIs)   │
└────────────────────┬───────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│                  ESP32 IoT Orchestrator                  │
├───────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  Web Server │  │  Orchestrator │  │Config Storage │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
├───────────────────────────────────────────────────────────┤
│                   Component Registry                      │
├───────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────┐  │
│  │  Sensor  │  │ Actuator │  │  Service │  │ Custom │  │
│  │Components│  │Components│  │Components│  │ Comps  │  │
│  └──────────┘  └──────────┘  └──────────┘  └────────┘  │
└─────────────────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│              Hardware Layer (GPIO, I2C, SPI)             │
└─────────────────────────────────────────────────────────┘
```

## 3. Architectural Components

### 3.1 Core Orchestrator
**Responsibility**: Central coordination and lifecycle management

**Key Functions**:
- Component discovery and registration
- Execution scheduling based on NextCheckMs
- Resource allocation and monitoring
- Error handling and recovery
- System state management

**Implementation Requirements**:
- Non-blocking execution model
- Priority-based scheduling
- Watchdog timer integration
- Memory management optimization

### 3.2 Component Registry
**Responsibility**: Dynamic component management

**Key Functions**:
- Component registration/deregistration
- Instance creation from templates
- Dependency resolution
- Component metadata management

**Data Structures**:
```cpp
struct ComponentMetadata {
    String componentType;
    String version;
    uint32_t memoryFootprint;
    String[] dependencies;
    JsonDocument capabilities;
};
```

### 3.3 Configuration Storage
**Responsibility**: Persistent configuration management

**Storage Layers**:
- **SPIFFS/LittleFS**: Component configurations
- **NVS**: System settings and credentials
- **RAM Cache**: Active configuration cache

**Configuration Schema**:
```json
{
  "system": {
    "version": "1.0.0",
    "deviceId": "esp32-orchestrator-001",
    "timezone": "UTC"
  },
  "components": [
    {
      "id": "unique-component-id",
      "type": "component-type",
      "enabled": true,
      "config": {},
      "schedule": {}
    }
  ]
}
```

### 3.4 Web Server Routes
**Responsibility**: HTTP API and web interface

**Core Endpoints**:
- `GET /api/system/status` - System health and metrics
- `GET /api/components` - List all components
- `POST /api/components` - Register new component
- `GET /api/components/{id}` - Component details
- `PUT /api/components/{id}` - Update component
- `DELETE /api/components/{id}` - Remove component
- `POST /api/components/{id}/execute` - Execute component now
- `GET /api/components/{id}/options` - Get execution options

## 4. Component Architecture

### 4.1 BaselineComponent Interface
```cpp
class BaselineComponent {
public:
    // Lifecycle Methods
    virtual bool init(const JsonDocument& config) = 0;
    virtual void execute() = 0;
    virtual void cleanup() = 0;
    
    // Execution Control
    virtual bool ExecuteNow(const String& inputJson, 
                           String& outputJson, 
                           int* resultCode, 
                           String* resultDescription) = 0;
    virtual String ExecuteOptions() const = 0;
    
    // State Management
    virtual JsonDocument getState() const = 0;
    virtual bool setState(const JsonDocument& state) = 0;
    
    // Metadata
    virtual String getComponentType() const = 0;
    virtual String getVersion() const = 0;
    
    // Scheduling
    uint32_t NextCheckMs = 0;
    
protected:
    String componentId;
    bool enabled = true;
    JsonDocument configuration;
};
```

### 4.2 Component Lifecycle

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Created │────▶│Configured│────▶│  Active  │────▶│ Cleanup  │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
                                        │
                                        ▼
                                   ┌──────────┐
                                   │Execute() │
                                   └──────────┘
                                        │
                                   ┌────┴────┐
                                   │NextCheck│
                                   └─────────┘
```

## 5. Data Flow Architecture

### 5.1 Request Processing Flow
```
Web Request → Route Handler → Validation → Component Registry
                                              │
                                              ▼
                                         Component Instance
                                              │
                                              ▼
                                         Execute/Configure
                                              │
                                              ▼
                                         JSON Response
```

### 5.2 Scheduled Execution Flow
```
Main Loop → Orchestrator → Check NextCheckMs → Priority Queue
                                                     │
                                                     ▼
                                               Execute Component
                                                     │
                                                     ▼
                                               Update NextCheckMs
```

## 6. Security Architecture

### 6.1 Authentication Layers
- **Device Level**: Unique device ID and certificate
- **API Level**: Bearer token or API key authentication
- **Component Level**: Permission-based access control

### 6.2 Data Protection
- Configuration encryption in storage
- Secure credential management in NVS
- TLS for external communications
- Input validation and sanitization

## 7. Performance Considerations

### 7.1 Memory Management
- Component memory pooling
- Dynamic allocation monitoring
- Stack size optimization per task
- Heap fragmentation prevention

### 7.2 Execution Optimization
- Cooperative multitasking
- Priority-based scheduling
- Lazy loading of components
- Caching of frequently accessed data

## 8. Scalability Design

### 8.1 Horizontal Scaling
- Multi-ESP32 orchestration support
- Component distribution across devices
- Mesh networking capability

### 8.2 Vertical Scaling
- Dynamic component loading/unloading
- Resource-based component limits
- Adaptive scheduling algorithms

## 9. Monitoring and Diagnostics

### 9.1 System Metrics
- CPU utilization
- Memory usage (heap/stack)
- Component execution times
- Network statistics
- Error rates and types

### 9.2 Logging Framework
```cpp
enum LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

class Logger {
    void log(LogLevel level, String component, String message);
    void logMetric(String metric, float value);
};
```

## 10. Deployment Architecture

### 10.1 Build Configuration
- PlatformIO environment setup
- Partition table optimization
- OTA update support
- Rollback mechanisms

### 10.2 Development Environment
- Component development SDK
- Simulation framework
- Testing harness
- CI/CD pipeline integration

## 11. Technology Stack

### 11.1 Core Technologies
- **Platform**: ESP32 (ESP-IDF / Arduino)
- **Build System**: PlatformIO
- **Storage**: SPIFFS/LittleFS, NVS
- **Networking**: WiFi, HTTP/HTTPS, WebSockets
- **Serialization**: ArduinoJson
- **Web Framework**: ESPAsyncWebServer

### 11.2 Development Tools
- **IDE**: VSCode with PlatformIO
- **Version Control**: Git
- **Testing**: Unity (embedded testing)
- **Documentation**: Doxygen

## 12. Quality Attributes

### 12.1 Reliability
- Watchdog timer implementation
- Automatic recovery mechanisms
- Component isolation
- Graceful degradation

### 12.2 Maintainability
- Modular component design
- Clear interface contracts
- Comprehensive logging
- Self-documenting code

### 12.3 Usability
- RESTful API design
- Web-based management UI
- Clear error messages
- Configuration validation

## 13. Constraints and Assumptions

### 13.1 Technical Constraints
- ESP32 memory limitations (RAM/Flash)
- WiFi connectivity requirement
- Power consumption considerations
- Real-time execution requirements

### 13.2 Design Assumptions
- JSON as primary data format
- HTTP/HTTPS for primary communication
- Component independence
- Configuration-driven behavior

## 14. Future Considerations

### 14.1 Planned Enhancements
- MQTT integration
- Bluetooth/BLE support
- Rule engine for automation
- Cloud service integration
- Machine learning at edge

### 14.2 Extension Points
- Custom protocol handlers
- Plugin architecture
- External storage support
- Multi-language support

---

**Document Version**: 1.0.0  
**Last Updated**: 2025-01-25  
**Status**: Authoritative