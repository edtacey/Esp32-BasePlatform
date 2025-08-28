# ESP32 IoT Orchestrator - Component Architecture Guide

## Overview

This document defines the architecture rules, naming conventions, and patterns for implementing IoT capabilities as components in the ESP32 IoT Orchestrator system. All components must follow these patterns to ensure proper integration with the orchestrator's execution loop, configuration persistence, and inter-component communication systems.

## Component Types

### 1. SENSORS
**Purpose**: Read data from environment or hardware devices
**Examples**: DHT22Component (temperature/humidity), TSL2561Component (light), PHSensorComponent (pH)
**Key Characteristics**:
- Read data from GPIO pins, I2C/SPI devices, or external sources
- Generate JsonDocument with sensor readings in execute()
- Typically schedule regular execution intervals (1-30 seconds)
- Support mock mode for testing without hardware

### 2. ACTUATORS
**Purpose**: Control physical devices and hardware
**Examples**: ServoDimmerComponent (servo control), PeristalticPumpComponent (pumps)
**Key Characteristics**:
- Control GPIO outputs, PWM signals, or communication protocols
- Accept commands via action system from other components
- Maintain state about current actuator position/status
- Support calibration and safety limits

### 3. ORCHESTRATORS
**Purpose**: Coordinate multiple components and implement higher-level logic
**Examples**: LightOrchestrator (lighting control), Orchestrator (system coordinator)
**Key Characteristics**:
- Read data from sensor components via inter-component communication
- Send commands to actuator components via action system
- Implement decision-making logic based on multiple inputs
- Manage complex timing and coordination scenarios

### 4. SERVICES
**Purpose**: Provide system-level services and external connectivity
**Examples**: WebServerComponent (HTTP API), MqttBroadcastComponent (MQTT publishing)
**Key Characteristics**:
- Interface with external systems (network, storage, protocols)
- Provide APIs for component management and monitoring
- Handle system-level concerns (logging, monitoring, communication)

## Core Architecture Requirements

### BaseComponent Inheritance
```cpp
class YourComponent : public BaseComponent {
public:
    YourComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    ~YourComponent() override;

    // REQUIRED: Must implement all 4 pure virtual methods
    JsonDocument getDefaultSchema() const override;
    bool initialize(const JsonDocument& config) override;
    ExecutionResult execute() override;
    void cleanup() override;

protected:
    // REQUIRED: Must implement all 4 configuration methods
    JsonDocument getCurrentConfig() const override;
    bool applyConfig(const JsonDocument& config) override;
    std::vector<ComponentAction> getSupportedActions() const override;
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;
};
```

### Critical Execution Loop Integration
```cpp
bool YourComponent::initialize(const JsonDocument& config) {
    // ... initialization logic ...
    
    // CRITICAL: Must call this to enable scheduling
    setNextExecutionMs(millis() + m_yourInterval);
    
    setState(ComponentState::READY);
    return true;
}

ExecutionResult YourComponent::execute() {
    // ... execution logic ...
    
    // CRITICAL: Must call both to track executions
    updateExecutionStats();  // Updates m_executionCount
    setNextExecutionMs(millis() + m_yourInterval);  // Schedule next execution
    
    return result;
}
```

## Naming Conventions

### Component Class Names
- **Format**: `{Function}{Type}Component`
- **Examples**: 
  - `DHT22Component` (sensor type + Component)
  - `ServoDimmerComponent` (function + type + Component)
  - `LightOrchestrator` (orchestrator - no Component suffix)

### Component IDs (Instance Names)
- **Format**: `{function}-{type}-{number}`
- **Examples**: 
  - `dht22-1`, `temp-sensor-2`
  - `servo-dimmer-1`, `pump-controller-3`
  - `light-orchestrator-1`

### Configuration Parameters
- **Format**: `snake_case`
- **Examples**: `gpio_pin`, `sampling_interval_ms`, `enable_mock_mode`
- **Special**: Use `_ms` suffix for time values in milliseconds

### Member Variables
- **Format**: `m_{snake_case}`
- **Examples**: `m_gpioPin`, `m_samplingIntervalMs`, `m_deviceName`

### Action Names
- **Format**: `snake_case_verbs`
- **Examples**: `read_sensor`, `set_position`, `start_calibration`

## Configuration Schema Pattern

### Schema Structure
```cpp
JsonDocument YourComponent::getDefaultSchema() const {
    JsonDocument schema;
    
    // Required metadata
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["title"] = "Your Component Type";
    schema["description"] = "Component description";
    schema["type"] = "object";
    schema["version"] = 1;
    
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // Every parameter needs: type, default, description
    // Optional: minimum, maximum, enum, maxLength
    JsonObject paramProp = properties["your_parameter"].to<JsonObject>();
    paramProp["type"] = "integer";  // or "number", "string", "boolean", "array", "object"
    paramProp["default"] = 5000;
    paramProp["description"] = "Description of parameter purpose";
    paramProp["minimum"] = 100;     // Optional validation
    paramProp["maximum"] = 60000;   // Optional validation
    
    return schema;
}
```

### Default Hydration Pattern
```cpp
bool YourComponent::applyConfig(const JsonDocument& config) {
    // CRITICAL: Use | operator for defaults - enables empty config support
    m_yourParameter = config["your_parameter"] | defaultValue;
    m_anotherParam = config["another_param"] | String("default_string");
    m_boolParam = config["bool_param"] | true;
    
    // Always include version for migration support
    uint16_t configVersion = config["config_version"] | 1;
    
    return true;
}
```

## State Management Rules

### Component States
1. **UNINITIALIZED**: Component created but not configured
2. **INITIALIZING**: Currently running initialization
3. **READY**: Ready for execution and actions
4. **EXECUTING**: Currently running execute() method
5. **ERROR**: Error state - requires intervention
6. **COMPONENT_DISABLED**: Intentionally disabled

### State Transition Rules
```cpp
// Standard initialization pattern
setState(ComponentState::INITIALIZING);
// ... do initialization work ...
setState(ComponentState::READY);

// Standard execution pattern
setState(ComponentState::EXECUTING);
// ... do execution work ...
setState(ComponentState::READY);

// Error handling
setError("Specific error message");  // Automatically sets ERROR state
```

## Inter-Component Communication

### Action System
Components communicate through the action system rather than direct method calls:

```cpp
// Component defining actions
std::vector<ComponentAction> YourComponent::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    
    ComponentAction myAction;
    myAction.name = "do_something";
    myAction.description = "Perform some operation";
    myAction.requiresReady = true;
    myAction.timeoutMs = 5000;
    
    // Add parameters with validation
    ActionParameter param;
    param.name = "target_value";
    param.type = ActionParameterType::FLOAT;
    param.required = true;
    param.minValue = 0.0f;
    param.maxValue = 100.0f;
    myAction.parameters.push_back(param);
    
    actions.push_back(myAction);
    return actions;
}
```

### Calling Actions on Other Components
```cpp
// From within a component's execute() or action methods
JsonDocument actionParams;
actionParams["target_value"] = 75.0f;

ActionResult result = m_orchestrator->executeComponentAction(
    "target-component-id", 
    "action_name", 
    actionParams
);

if (result.success) {
    // Use result.data for response data
}
```

## Upstream Dependencies (Shared Components)

### Required Includes
```cpp
#include "BaseComponent.h"           // ALWAYS required
#include "../utils/Logger.h"         // For logging
#include "../core/Orchestrator.h"    // For inter-component communication
#include <Arduino.h>                 // For Arduino framework
```

### Hardware Libraries (Examples)
```cpp
#include <DHT.h>                     // Temperature/humidity sensors
#include <Wire.h>                    // I2C communication
#include <Servo.h>                   // Servo motor control
#include <Adafruit_TSL2561_U.h>     // Light sensor
#include <PubSubClient.h>            // MQTT communication
#include <AsyncWebServer.h>          // Web server functionality
```

### System Integration Points
```cpp
ConfigStorage& m_storage;            // Configuration persistence
Orchestrator* m_orchestrator;        // Inter-component communication
Logger                               // Centralized logging system
```

## Downstream Assumptions (What Your Component Provides)

### To the Orchestrator
1. **Execution Scheduling**: Component schedules itself via `setNextExecutionMs()`
2. **State Reporting**: Component maintains accurate state via `setState()`
3. **Data Generation**: Component produces structured data via `execute()`
4. **Error Reporting**: Component reports errors via `setError()`

### To Other Components
1. **Action Interface**: Component exposes actions via `getSupportedActions()`
2. **Configuration Access**: Component configuration accessible via REST API
3. **Status Monitoring**: Component state and statistics available
4. **Data Sharing**: Component data available through orchestrator

### To Storage System
1. **Configuration Persistence**: Component config saved/loaded automatically
2. **Schema Definition**: Component schema registered for validation
3. **Migration Support**: Component handles configuration version migrations

## Mock Mode Implementation

### Mock Mode Requirements
ALL components must support mock mode for development and testing:

```cpp
// In initialize()
if (m_gpioPin == 0 || m_enableMockMode) {
    log(Logger::INFO, "Running in mock mode");
    m_currentMode = YourMode::MOCK;
    return true;  // Skip hardware initialization
}

// In execute()
if (m_currentMode == YourMode::MOCK) {
    return generateMockData();
}
```

### Mock Data Guidelines
- Generate realistic data that changes over time
- Include all the same fields as real hardware
- Add identifying markers (e.g., `"mode": "mock"`)
- Simulate normal operational patterns

## Error Handling Patterns

### Error States
```cpp
// Use setError() for all error conditions
setError("Hardware initialization failed");  // Sets ERROR state automatically

// Check for error conditions
if (someCondition) {
    setError("Specific error description");
    return false;  // or return failed ExecutionResult
}
```

### Error Recovery
```cpp
// Components can recover from errors by calling clearError()
if (m_state == ComponentState::ERROR && canRecover()) {
    clearError();  // Returns to READY state
}
```

## Performance and Resource Management

### Memory Management
- Use JsonDocument for configuration and data (automatic memory management)
- Free hardware resources in cleanup()
- Avoid memory leaks in long-running components

### Execution Timing
- Use `millis()` for all timing operations
- Set reasonable execution intervals (avoid overloading system)
- Components should complete execute() quickly (< 1 second typically)

### Resource Sharing
- Components share I2C/SPI buses - coordinate access
- GPIO pins are exclusive - document pin usage
- Use inter-component communication for shared resources

## Testing and Development

### Mock Mode Development
1. Always implement and test mock mode first
2. Verify configuration persistence with mock data
3. Test action system with mock responses
4. Validate schema and configuration hydration

### Hardware Integration
1. Start with simplest hardware configuration
2. Add calibration and advanced features incrementally
3. Test error conditions and recovery
4. Verify timing and resource usage

### System Integration
1. Test component in isolation with orchestrator
2. Verify inter-component communication
3. Test configuration persistence across reboots
4. Monitor system performance and memory usage

## Example Implementation Workflow

1. **Copy template files**: `TemplateComponent.h` and `TemplateComponent.cpp`
2. **Replace "Template" with your component name** throughout
3. **Define component-specific enums and structs**
4. **Implement getDefaultSchema()** with your configuration parameters
5. **Implement applyConfig()** with default hydration pattern
6. **Implement getCurrentConfig()** to return current state
7. **Implement initialize()** following the standard pattern
8. **Implement execute()** with your component logic
9. **Implement action system** for inter-component communication
10. **Test in mock mode** before hardware integration
11. **Add to Orchestrator initialization** in `initializeDefaultComponents()`

## Common Pitfalls to Avoid

1. **Missing setNextExecutionMs()**: Component won't be scheduled
2. **Not calling updateExecutionStats()**: Execution count won't increment
3. **Forgetting mock mode**: Can't test without hardware
4. **Hard-coded configurations**: Use schema and default hydration
5. **Direct component references**: Use action system instead
6. **Blocking operations**: Keep execute() fast and non-blocking
7. **Missing error handling**: Always validate and report errors
8. **Inconsistent naming**: Follow established conventions
9. **Missing cleanup()**: Can cause resource leaks
10. **State management errors**: Follow INITIALIZING->READY->EXECUTING->READY pattern

## Integration with System Services

### Configuration Storage
- Automatic save/load via BaseComponent methods
- Schema-driven validation and defaults
- Version migration support for config updates

### Execution Orchestration
- Components registered with main Orchestrator
- Scheduled execution via `isReadyToExecute()` and timing
- Pause/resume support via execution loop API

### Web API Integration
- Component data automatically exposed via `/api/components/data`
- Configuration accessible via `/api/components/{id}/config`
- Actions callable via `/api/components/{id}/actions`

### MQTT Integration
- Component data automatically published via MqttBroadcastComponent
- Topics follow pattern: `/EspOrch/{ComponentID}`
- Data includes timestamp and component metadata

### Logging System
- Centralized logging via Logger class
- Log format: `[timestamp][level][ComponentType:id] message`
- Automatic component identification in logs

This architecture ensures scalable, maintainable, and testable IoT component development within the ESP32 orchestrator ecosystem.