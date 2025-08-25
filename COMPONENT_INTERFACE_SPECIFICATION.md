# Component Interface Specification

## 1. Overview

This document defines the authoritative interface specifications for all components in the ESP32 IoT Orchestrator system. All components must implement these interfaces to ensure compatibility with the orchestration framework.

## 2. Base Component Interface

### 2.1 IBaselineComponent

```cpp
#ifndef IBASELINE_COMPONENT_H
#define IBASELINE_COMPONENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

enum class ComponentState {
    UNINITIALIZED = 0,
    INITIALIZING = 1,
    READY = 2,
    EXECUTING = 3,
    ERROR = 4,
    DISABLED = 5,
    CLEANUP = 6
};

enum class ComponentType {
    SENSOR = 0,
    ACTUATOR = 1,
    CONTROLLER = 2,
    SERVICE = 3,
    CUSTOM = 4
};

struct ComponentInfo {
    String id;
    String name;
    String type;
    String version;
    String description;
    uint32_t memoryUsage;
    ComponentState state;
};

struct ExecutionResult {
    bool success;
    int code;
    String message;
    JsonDocument data;
    uint32_t executionTimeMs;
};

class IBaselineComponent {
public:
    virtual ~IBaselineComponent() = default;
    
    // === Lifecycle Management ===
    virtual bool init(const JsonDocument& config) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void cleanup() = 0;
    
    // === Execution Methods ===
    virtual void execute() = 0;
    virtual ExecutionResult executeNow(const JsonDocument& input) = 0;
    virtual JsonDocument getExecutionOptions() const = 0;
    
    // === State Management ===
    virtual ComponentState getState() const = 0;
    virtual JsonDocument getStatus() const = 0;
    virtual bool setState(ComponentState newState) = 0;
    
    // === Configuration ===
    virtual bool configure(const JsonDocument& config) = 0;
    virtual JsonDocument getConfiguration() const = 0;
    virtual JsonDocument getConfigurationSchema() const = 0;
    virtual bool validateConfiguration(const JsonDocument& config) const = 0;
    
    // === Component Information ===
    virtual ComponentInfo getInfo() const = 0;
    virtual ComponentType getComponentType() const = 0;
    
    // === Scheduling ===
    virtual uint32_t getNextExecutionMs() const = 0;
    virtual void setNextExecutionMs(uint32_t ms) = 0;
    
    // === Error Handling ===
    virtual String getLastError() const = 0;
    virtual void clearError() = 0;
    
    // === Events ===
    virtual void onStateChange(std::function<void(ComponentState)> callback) = 0;
    virtual void onDataAvailable(std::function<void(const JsonDocument&)> callback) = 0;
};

#endif // IBASELINE_COMPONENT_H
```

### 2.2 BaseComponent Abstract Class

```cpp
#ifndef BASE_COMPONENT_H
#define BASE_COMPONENT_H

#include "IBaselineComponent.h"
#include <vector>

class BaseComponent : public IBaselineComponent {
protected:
    ComponentInfo info;
    ComponentState state = ComponentState::UNINITIALIZED;
    JsonDocument configuration;
    JsonDocument lastData;
    String lastError;
    uint32_t nextExecutionMs = 0;
    uint32_t lastExecutionMs = 0;
    
    std::vector<std::function<void(ComponentState)>> stateChangeCallbacks;
    std::vector<std::function<void(const JsonDocument&)>> dataCallbacks;
    
    // Protected helper methods
    virtual void notifyStateChange(ComponentState newState);
    virtual void notifyDataAvailable(const JsonDocument& data);
    virtual bool checkPreconditions();
    virtual void updateMemoryUsage();
    
public:
    BaseComponent(const String& id, const String& name, const String& type);
    virtual ~BaseComponent() = default;
    
    // Default implementations
    virtual ComponentState getState() const override { return state; }
    virtual ComponentInfo getInfo() const override { return info; }
    virtual uint32_t getNextExecutionMs() const override { return nextExecutionMs; }
    virtual void setNextExecutionMs(uint32_t ms) override { nextExecutionMs = ms; }
    virtual String getLastError() const override { return lastError; }
    virtual void clearError() override { lastError = ""; }
    virtual JsonDocument getConfiguration() const override { return configuration; }
    
    // Event management
    virtual void onStateChange(std::function<void(ComponentState)> callback) override;
    virtual void onDataAvailable(std::function<void(const JsonDocument&)> callback) override;
};

#endif // BASE_COMPONENT_H
```

## 3. Sensor Component Interface

### 3.1 ISensorComponent

```cpp
#ifndef ISENSOR_COMPONENT_H
#define ISENSOR_COMPONENT_H

#include "BaseComponent.h"

enum class SensorStatus {
    OK = 0,
    WARMING_UP = 1,
    CALIBRATING = 2,
    ERROR = 3,
    TIMEOUT = 4,
    OUT_OF_RANGE = 5
};

struct SensorReading {
    float value;
    String unit;
    uint32_t timestamp;
    SensorStatus status;
    float accuracy;  // 0.0 to 1.0
};

class ISensorComponent : public BaseComponent {
public:
    // Sensor-specific methods
    virtual SensorReading read() = 0;
    virtual bool calibrate(const JsonDocument& calibrationData) = 0;
    virtual JsonDocument getSensorInfo() const = 0;
    virtual float getMinValue() const = 0;
    virtual float getMaxValue() const = 0;
    virtual String getUnit() const = 0;
    virtual uint32_t getSamplingRateMs() const = 0;
    virtual void setSamplingRateMs(uint32_t rate) = 0;
    
    // Data filtering
    virtual void enableFiltering(bool enable) = 0;
    virtual void setFilterParameters(const JsonDocument& params) = 0;
    
    // Threshold alerts
    virtual void setThreshold(float min, float max) = 0;
    virtual void onThresholdExceeded(std::function<void(float)> callback) = 0;
};

#endif // ISENSOR_COMPONENT_H
```

## 4. Actuator Component Interface

### 4.1 IActuatorComponent

```cpp
#ifndef IACTUATOR_COMPONENT_H
#define IACTUATOR_COMPONENT_H

#include "BaseComponent.h"

enum class ActuatorStatus {
    IDLE = 0,
    ACTIVE = 1,
    TRANSITIONING = 2,
    ERROR = 3,
    BLOCKED = 4
};

struct ActuatorCommand {
    String action;
    JsonDocument parameters;
    uint32_t duration;  // ms, 0 for instant
    bool async;         // non-blocking execution
};

class IActuatorComponent : public BaseComponent {
public:
    // Actuator-specific methods
    virtual bool executeCommand(const ActuatorCommand& command) = 0;
    virtual ActuatorStatus getActuatorStatus() const = 0;
    virtual JsonDocument getSupportedActions() const = 0;
    virtual bool isActionValid(const String& action) const = 0;
    
    // State control
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;
    virtual bool toggle() = 0;
    
    // Safety features
    virtual void setEmergencyStop(bool stop) = 0;
    virtual bool setSafetyLimits(const JsonDocument& limits) = 0;
    virtual JsonDocument getSafetyStatus() const = 0;
    
    // Feedback
    virtual float getCurrentPosition() const = 0;  // For positional actuators
    virtual float getCurrentPower() const = 0;     // Power consumption
};

#endif // IACTUATOR_COMPONENT_H
```

## 5. Service Component Interface

### 5.1 IServiceComponent

```cpp
#ifndef ISERVICE_COMPONENT_H
#define ISERVICE_COMPONENT_H

#include "BaseComponent.h"

enum class ServiceStatus {
    STOPPED = 0,
    STARTING = 1,
    RUNNING = 2,
    STOPPING = 3,
    ERROR = 4,
    MAINTENANCE = 5
};

class IServiceComponent : public BaseComponent {
public:
    // Service lifecycle
    virtual bool startService() = 0;
    virtual bool stopService() = 0;
    virtual bool restartService() = 0;
    virtual ServiceStatus getServiceStatus() const = 0;
    
    // Service operations
    virtual JsonDocument processRequest(const JsonDocument& request) = 0;
    virtual bool subscribe(const String& topic, std::function<void(const JsonDocument&)> callback) = 0;
    virtual bool publish(const String& topic, const JsonDocument& data) = 0;
    
    // Health monitoring
    virtual JsonDocument getHealthStatus() const = 0;
    virtual bool performHealthCheck() = 0;
    virtual void setHealthCheckInterval(uint32_t intervalMs) = 0;
    
    // Metrics
    virtual JsonDocument getMetrics() const = 0;
    virtual void resetMetrics() = 0;
};

#endif // ISERVICE_COMPONENT_H
```

## 6. Component Factory Interface

### 6.1 IComponentFactory

```cpp
#ifndef ICOMPONENT_FACTORY_H
#define ICOMPONENT_FACTORY_H

#include "IBaselineComponent.h"
#include <memory>
#include <map>

class IComponentFactory {
public:
    virtual ~IComponentFactory() = default;
    
    // Factory methods
    virtual std::unique_ptr<IBaselineComponent> createComponent(
        const String& type,
        const String& id,
        const JsonDocument& config
    ) = 0;
    
    // Registration
    virtual bool registerComponentType(
        const String& type,
        std::function<std::unique_ptr<IBaselineComponent>(const String&, const JsonDocument&)> creator
    ) = 0;
    
    // Discovery
    virtual std::vector<String> getAvailableTypes() const = 0;
    virtual JsonDocument getTypeSchema(const String& type) const = 0;
    virtual bool isTypeRegistered(const String& type) const = 0;
};

#endif // ICOMPONENT_FACTORY_H
```

## 7. Component Communication Interface

### 7.1 IComponentMessaging

```cpp
#ifndef ICOMPONENT_MESSAGING_H
#define ICOMPONENT_MESSAGING_H

#include <ArduinoJson.h>
#include <functional>

enum class MessagePriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

struct ComponentMessage {
    String sourceId;
    String targetId;
    String messageType;
    JsonDocument payload;
    MessagePriority priority;
    uint32_t timestamp;
    bool requiresAck;
};

class IComponentMessaging {
public:
    virtual ~IComponentMessaging() = default;
    
    // Message sending
    virtual bool sendMessage(const ComponentMessage& message) = 0;
    virtual bool broadcast(const String& messageType, const JsonDocument& payload) = 0;
    
    // Message receiving
    virtual void onMessage(std::function<void(const ComponentMessage&)> handler) = 0;
    virtual void subscribe(const String& messageType, std::function<void(const ComponentMessage&)> handler) = 0;
    
    // Message filtering
    virtual void setMessageFilter(std::function<bool(const ComponentMessage&)> filter) = 0;
    
    // Acknowledgments
    virtual bool acknowledgeMessage(const String& messageId) = 0;
    virtual void onAcknowledgment(std::function<void(const String&)> handler) = 0;
};

#endif // ICOMPONENT_MESSAGING_H
```

## 8. Component Persistence Interface

### 8.1 IComponentPersistence

```cpp
#ifndef ICOMPONENT_PERSISTENCE_H
#define ICOMPONENT_PERSISTENCE_H

#include <ArduinoJson.h>

class IComponentPersistence {
public:
    virtual ~IComponentPersistence() = default;
    
    // State persistence
    virtual bool saveState(const String& componentId, const JsonDocument& state) = 0;
    virtual bool loadState(const String& componentId, JsonDocument& state) = 0;
    virtual bool deleteState(const String& componentId) = 0;
    
    // Configuration persistence
    virtual bool saveConfiguration(const String& componentId, const JsonDocument& config) = 0;
    virtual bool loadConfiguration(const String& componentId, JsonDocument& config) = 0;
    
    // Data logging
    virtual bool logData(const String& componentId, const JsonDocument& data) = 0;
    virtual bool queryData(const String& componentId, uint32_t startTime, uint32_t endTime, JsonDocument& result) = 0;
    
    // Backup and restore
    virtual bool createBackup(const String& componentId) = 0;
    virtual bool restoreFromBackup(const String& componentId, uint32_t backupTime) = 0;
    virtual std::vector<uint32_t> listBackups(const String& componentId) = 0;
};

#endif // ICOMPONENT_PERSISTENCE_H
```

## 9. Component Validation Interface

### 9.1 IComponentValidator

```cpp
#ifndef ICOMPONENT_VALIDATOR_H
#define ICOMPONENT_VALIDATOR_H

#include <ArduinoJson.h>
#include <vector>

struct ValidationError {
    String field;
    String message;
    String code;
};

class IComponentValidator {
public:
    virtual ~IComponentValidator() = default;
    
    // Configuration validation
    virtual bool validateConfiguration(
        const JsonDocument& config,
        const JsonDocument& schema,
        std::vector<ValidationError>& errors
    ) = 0;
    
    // Data validation
    virtual bool validateData(
        const JsonDocument& data,
        const JsonDocument& schema,
        std::vector<ValidationError>& errors
    ) = 0;
    
    // Schema validation
    virtual bool validateSchema(const JsonDocument& schema) = 0;
    
    // Custom validators
    virtual void registerValidator(
        const String& validatorName,
        std::function<bool(const JsonVariant&, ValidationError&)> validator
    ) = 0;
};

#endif // ICOMPONENT_VALIDATOR_H
```

## 10. Implementation Guidelines

### 10.1 Component Implementation Checklist

- [ ] Inherit from appropriate base interface
- [ ] Implement all pure virtual methods
- [ ] Handle all error cases gracefully
- [ ] Validate input parameters
- [ ] Implement proper cleanup in destructor
- [ ] Use RAII for resource management
- [ ] Thread-safe for concurrent access
- [ ] Document all public methods
- [ ] Unit test coverage > 80%
- [ ] Integration test with orchestrator

### 10.2 Memory Management Rules

1. Use smart pointers for dynamic allocation
2. Implement move semantics where applicable
3. Avoid memory fragmentation
4. Monitor stack usage
5. Implement memory pools for frequent allocations

### 10.3 Error Handling Convention

```cpp
try {
    // Operation
} catch (const std::exception& e) {
    lastError = String(e.what());
    state = ComponentState::ERROR;
    notifyStateChange(state);
    return ExecutionResult{false, -1, lastError, JsonDocument(), 0};
}
```

### 10.4 Logging Convention

```cpp
#define COMPONENT_LOG(level, component, message) \
    Serial.printf("[%s][%s] %s\n", level, component, message)

// Usage
COMPONENT_LOG("INFO", info.name.c_str(), "Component initialized");
```

---

**Document Version**: 1.0.0  
**Last Updated**: 2025-01-25  
**Status**: Authoritative