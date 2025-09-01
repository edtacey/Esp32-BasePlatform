/**
 * @file TemplateComponent.cpp
 * @brief Template component implementation demonstrating ESP32 IoT architecture patterns
 */

#include "TemplateComponent.h"
#include "../utils/Logger.h"
#include "../core/Orchestrator.h"
#include <Arduino.h>

// =====================================================================
// CONSTRUCTOR & DESTRUCTOR
// =====================================================================

TemplateComponent::TemplateComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "Template", name, storage, orchestrator) {
    log(Logger::DEBUG, "TemplateComponent created");
}

TemplateComponent::~TemplateComponent() {
    cleanup();
}

// =====================================================================
// REQUIRED: Pure virtual method implementations
// =====================================================================

JsonDocument TemplateComponent::getDefaultSchema() const {
    JsonDocument schema;
    
    // Schema metadata (required)
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["title"] = "Template IoT Component";
    schema["description"] = "Template component for demonstrating architecture patterns";
    schema["type"] = "object";
    schema["version"] = 1;
    
    // Configuration properties with defaults
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // GPIO pin configuration
    JsonObject pinProp = properties["gpio_pin"].to<JsonObject>();
    pinProp["type"] = "integer";
    pinProp["minimum"] = 0;      // 0 = mock mode
    pinProp["maximum"] = 39;     // ESP32 GPIO range
    pinProp["default"] = 0;      // Default to mock mode
    pinProp["description"] = "GPIO pin for component (0 = mock mode)";
    
    // Sampling interval
    JsonObject intervalProp = properties["sampling_interval_ms"].to<JsonObject>();
    intervalProp["type"] = "integer";
    intervalProp["minimum"] = 100;
    intervalProp["maximum"] = 3600000;  // 1 hour max
    intervalProp["default"] = 5000;     // 5 seconds default
    intervalProp["description"] = "How often to execute component operation (ms)";
    
    // Device name
    JsonObject nameProp = properties["device_name"].to<JsonObject>();
    nameProp["type"] = "string";
    nameProp["maxLength"] = 64;
    nameProp["default"] = "Template Device";
    nameProp["description"] = "Human-readable device name";
    
    // Mock mode enable
    JsonObject mockProp = properties["enable_mock_mode"].to<JsonObject>();
    mockProp["type"] = "boolean";
    mockProp["default"] = true;
    mockProp["description"] = "Enable mock data generation for testing";
    
    // Example numeric parameter
    JsonObject param1Prop = properties["config_parameter_1"].to<JsonObject>();
    param1Prop["type"] = "number";
    param1Prop["minimum"] = 0.0;
    param1Prop["maximum"] = 100.0;
    param1Prop["default"] = 1.0;
    param1Prop["description"] = "Example numeric configuration parameter";
    
    // Example string parameter
    JsonObject param2Prop = properties["config_parameter_2"].to<JsonObject>();
    param2Prop["type"] = "string";
    JsonArray enumValues = param2Prop["enum"].to<JsonArray>();
    enumValues.add("option1");
    enumValues.add("option2"); 
    enumValues.add("default");
    param2Prop["default"] = "default";
    param2Prop["description"] = "Example string configuration parameter";
    
    log(Logger::DEBUG, "Generated default schema with pin=0 (mock), interval=5000ms");
    return schema;
}

bool TemplateComponent::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing Template Component: " + m_componentName);
    setState(ComponentState::INITIALIZING);
    
    // STEP 1: Load configuration (handles defaults and persistence)
    if (!loadConfiguration(config)) {
        setError("Failed to load configuration");
        return false;
    }
    
    // STEP 2: Apply configuration to member variables
    if (!applyConfig(getConfiguration())) {
        setError("Failed to apply configuration");
        return false;
    }
    
    // STEP 3: Initialize hardware resources
    if (!initializeHardware()) {
        setError("Failed to initialize hardware");
        return false;
    }
    
    // STEP 4: CRITICAL - Set initial execution time to enable scheduling
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    
    // STEP 5: Set state to READY
    setState(ComponentState::READY);
    log(Logger::INFO, "Template component initialized successfully on GPIO " + String(m_gpioPin));
    
    return true;
}

ExecutionResult TemplateComponent::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    
    // STEP 1: Set executing state
    setState(ComponentState::EXECUTING);
    
    // STEP 2: Perform main component operation
    bool operationSuccess = performMainOperation();
    
    // STEP 3: Create result data
    JsonDocument data;
    data["timestamp"] = startTime;
    data["component_id"] = m_componentId;
    data["gpio_pin"] = m_gpioPin;
    data["mode"] = static_cast<int>(m_currentMode);
    
    if (operationSuccess) {
        // Add component-specific data
        if (m_enableMockMode || m_gpioPin == 0) {
            data = generateMockData();
        } else {
            // Add real hardware data here
            data["last_reading"] = m_lastReading;
            data["actuator_state"] = m_actuatorState;
        }
        
        data["operation"] = "success";
        result.success = true;
        result.message = "Operation completed successfully";
        
        m_successfulOperations++;
    } else {
        data["operation"] = "failed";
        data["error"] = m_lastError;
        result.success = false;
        result.message = "Operation failed: " + m_lastError;
        
        m_failedOperations++;
    }
    
    // STEP 4: Store execution data and update stats
    result.data = data;
    result.executionTimeMs = millis() - startTime;
    updateExecutionStats();  // CRITICAL: Updates m_executionCount and m_lastExecutionMs
    
    // STEP 5: Schedule next execution
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    
    // STEP 6: Return to READY state
    setState(ComponentState::READY);
    
    return result;
}

void TemplateComponent::cleanup() {
    log(Logger::DEBUG, "Cleaning up template component resources");
    
    // Free any allocated hardware resources
    // Examples:
    // - Close I2C connections
    // - Detach servo motors
    // - Disconnect from external services
    // - Free allocated memory
    
    m_currentMode = TemplateMode::INACTIVE;
}

// =====================================================================
// REQUIRED: Configuration management implementation
// =====================================================================

JsonDocument TemplateComponent::getCurrentConfig() const {
    JsonDocument config;
    
    // Return ALL current configuration values
    // This enables configuration persistence and API access
    config["gpio_pin"] = m_gpioPin;
    config["sampling_interval_ms"] = m_samplingIntervalMs;
    config["device_name"] = m_deviceName;
    config["enable_mock_mode"] = m_enableMockMode;
    config["config_parameter_1"] = m_configParameter1;
    config["config_parameter_2"] = m_configParameter2;
    config["config_version"] = 1;  // For future migration support
    
    return config;
}

bool TemplateComponent::applyConfig(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying template component configuration with default hydration");
    
    // CRITICAL PATTERN: Use | operator for default hydration
    // This ensures components work with empty/partial configurations
    m_gpioPin = config["gpio_pin"] | 0;
    m_samplingIntervalMs = config["sampling_interval_ms"] | 5000;
    m_deviceName = config["device_name"] | String("Template Device");
    m_enableMockMode = config["enable_mock_mode"] | true;
    m_configParameter1 = config["config_parameter_1"] | 1.0f;
    m_configParameter2 = config["config_parameter_2"] | String("default");
    
    // Handle configuration version migration
    uint16_t configVersion = config["config_version"] | 1;
    if (configVersion < 1) {
        log(Logger::INFO, "Migrating template configuration to version 1");
        // Future migration logic would go here
    }
    
    // Update operational mode based on configuration
    if (m_gpioPin == 0 || m_enableMockMode) {
        m_currentMode = TemplateMode::MOCK;
    } else {
        m_currentMode = TemplateMode::NORMAL;
    }
    
    log(Logger::DEBUG, String("Config applied: pin=") + m_gpioPin + 
                       ", interval=" + m_samplingIntervalMs + "ms" +
                       ", mode=" + static_cast<int>(m_currentMode));
    
    return true;
}

// =====================================================================
// REQUIRED: Action system implementation
// =====================================================================

std::vector<ComponentAction> TemplateComponent::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    
    // Example Action 1: Get Status
    ComponentAction statusAction;
    statusAction.name = "get_status";
    statusAction.description = "Get current component status and readings";
    statusAction.timeoutMs = 3000;
    statusAction.requiresReady = false;  // Can run even when not READY
    // No parameters required for this action
    actions.push_back(statusAction);
    
    // Example Action 2: Set Configuration Parameter
    ComponentAction configAction;
    configAction.name = "set_parameter";
    configAction.description = "Update a configuration parameter";
    configAction.timeoutMs = 5000;
    configAction.requiresReady = true;  // Requires READY state
    
    // Define parameters with validation
    ActionParameter paramName;
    paramName.name = "parameter_name";
    paramName.type = ActionParameterType::STRING;
    paramName.required = true;
    paramName.maxLength = 32;
    paramName.description = "Name of parameter to set";
    configAction.parameters.push_back(paramName);
    
    ActionParameter paramValue;
    paramValue.name = "parameter_value";
    paramValue.type = ActionParameterType::FLOAT;
    paramValue.required = true;
    paramValue.minValue = 0.0f;
    paramValue.maxValue = 100.0f;
    paramValue.description = "New parameter value";
    configAction.parameters.push_back(paramValue);
    
    actions.push_back(configAction);
    
    // Add more actions as needed for your component
    
    return actions;
}

ActionResult TemplateComponent::performAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.actionName = actionName;
    uint32_t startTime = millis();
    
    if (actionName == "get_status") {
        // Return current component status
        JsonDocument statusData;
        statusData["component_id"] = m_componentId;
        statusData["state"] = getStateString();
        statusData["mode"] = static_cast<int>(m_currentMode);
        statusData["last_reading"] = m_lastReading;
        statusData["successful_operations"] = m_successfulOperations;
        statusData["failed_operations"] = m_failedOperations;
        statusData["uptime_ms"] = millis();
        
        result.success = true;
        result.message = "Status retrieved successfully";
        result.data = statusData;
        
    } else if (actionName == "set_parameter") {
        // Update configuration parameter
        String paramName = parameters["parameter_name"];
        float paramValue = parameters["parameter_value"];
        
        if (paramName == "config_parameter_1") {
            m_configParameter1 = paramValue;
            
            // Save updated configuration
            if (saveCurrentConfiguration()) {
                result.success = true;
                result.message = "Parameter updated and saved";
                result.data["old_value"] = m_configParameter1;
                result.data["new_value"] = paramValue;
            } else {
                result.success = false;
                result.message = "Parameter updated but failed to save";
            }
        } else {
            result.success = false;
            result.message = "Unknown parameter: " + paramName;
        }
        
    } else {
        result.success = false;
        result.message = "Unknown action: " + actionName;
    }
    
    result.executionTimeMs = millis() - startTime;
    return result;
}

// =====================================================================
// PRIVATE: Component-specific implementation methods
// =====================================================================

bool TemplateComponent::initializeHardware() {
    log(Logger::INFO, "Initializing template component hardware");
    
    if (m_gpioPin == 0 || m_enableMockMode) {
        log(Logger::INFO, "Running in mock mode - no hardware initialization needed");
        m_currentMode = TemplateMode::MOCK;
        return true;
    }
    
    // HARDWARE INITIALIZATION EXAMPLE:
    // For sensors:
    //   - Configure GPIO pins for analog/digital input
    //   - Initialize I2C/SPI communication
    //   - Configure sensor-specific settings
    //
    // For actuators:
    //   - Configure GPIO pins for output
    //   - Initialize PWM/servo libraries
    //   - Set initial actuator positions
    //
    // For orchestrators:
    //   - Initialize communication interfaces
    //   - Set up timers and scheduling
    //   - Initialize component references
    
    // Example GPIO setup
    pinMode(m_gpioPin, INPUT);  // For sensors
    // pinMode(m_gpioPin, OUTPUT); // For actuators
    
    m_currentMode = TemplateMode::NORMAL;
    log(Logger::INFO, "Hardware initialized successfully on GPIO " + String(m_gpioPin));
    
    return true;
}

bool TemplateComponent::performMainOperation() {
    log(Logger::DEBUG, "Performing main template operation");
    
    // Validate component state before operation
    if (!validateComponentState()) {
        return false;
    }
    
    bool success = false;
    
    switch (m_currentMode) {
        case TemplateMode::MOCK:
            // Mock mode operation
            log(Logger::DEBUG, "Generating mock data");
            m_lastReading = random(0, 100);  // Example mock reading
            m_lastOperationTime = millis();
            success = true;
            break;
            
        case TemplateMode::NORMAL:
            // REAL HARDWARE OPERATION EXAMPLES:
            
            // For SENSORS:
            //   - Read analog/digital values from GPIO
            //   - Communicate with I2C/SPI devices
            //   - Process and calibrate raw readings
            //   - Store results in m_lastReading
            
            // Example sensor reading:
            // int rawValue = analogRead(m_gpioPin);
            // m_lastReading = (rawValue * 3.3f) / 4095.0f;  // Convert to voltage
            
            // For ACTUATORS:
            //   - Write to GPIO pins
            //   - Control servo positions
            //   - Send PWM signals
            //   - Update m_actuatorState
            
            // Example actuator control:
            // digitalWrite(m_gpioPin, m_actuatorState ? HIGH : LOW);
            
            // For ORCHESTRATORS:
            //   - Read data from other components
            //   - Make decisions based on sensor data
            //   - Send commands to actuator components
            //   - Update coordination state
            
            // Example inter-component communication:
            // JsonDocument actionParams;
            // actionParams["target_value"] = m_lastReading;
            // ActionResult result = m_orchestrator->executeComponentAction("actuator-1", "set_value", actionParams);
            
            m_lastReading = 42.0f;  // Placeholder for real operation
            m_lastOperationTime = millis();
            success = true;
            break;
            
        case TemplateMode::CALIBRATE:
            // Calibration mode operation
            log(Logger::INFO, "Performing calibration operation");
            success = true;  // Implement calibration logic
            break;
            
        default:
            log(Logger::ERROR, "Invalid operation mode");
            success = false;
            break;
    }
    
    if (!success) {
        setError("Main operation failed");
    }
    
    return success;
}

JsonDocument TemplateComponent::generateMockData() {
    JsonDocument data;
    
    // Generate realistic mock data for your component type
    uint32_t currentTime = millis();
    
    // Common fields for all components
    data["timestamp"] = currentTime;
    data["component_id"] = m_componentId;
    data["mode"] = "mock";
    
    // SENSOR MOCK DATA EXAMPLE:
    data["sensor_value"] = m_lastReading + (random(-10, 10) / 10.0f);  // Add some noise
    data["unit"] = "units";
    data["quality"] = "good";
    
    // ACTUATOR MOCK DATA EXAMPLE:
    data["actuator_position"] = random(0, 180);  // Servo position
    data["actuator_state"] = m_actuatorState;
    data["power_consumption"] = random(50, 150);  // mW
    
    // ORCHESTRATOR MOCK DATA EXAMPLE:
    data["managed_components"] = 5;
    data["active_schedules"] = 3;
    data["system_health"] = "optimal";
    
    // Component-specific mock data
    data["config_parameter_1"] = m_configParameter1;
    data["successful_operations"] = m_successfulOperations;
    data["failed_operations"] = m_failedOperations;
    
    return data;
}

bool TemplateComponent::validateComponentState() {
    // Perform component-specific validation
    
    // Example validations:
    if (m_samplingIntervalMs < 100) {
        setError("Sampling interval too short");
        return false;
    }
    
    if (m_gpioPin > 39) {
        setError("Invalid GPIO pin number");
        return false;
    }
    
    // Add more validation as needed for your component
    
    return true;
}

// =====================================================================
// ARCHITECTURE COMPLIANCE VERIFICATION
// =====================================================================

/*
 * COMPLIANCE CHECKLIST FOR NEW COMPONENTS:
 * 
 * ✅ Inherits from BaseComponent
 * ✅ Implements all 4 pure virtual methods: getDefaultSchema(), initialize(), execute(), cleanup()
 * ✅ Implements all 4 configuration methods: getCurrentConfig(), applyConfig(), getSupportedActions(), performAction()
 * ✅ Uses schema-driven configuration with defaults
 * ✅ Calls setNextExecutionMs() in initialize() to enable scheduling
 * ✅ Calls updateExecutionStats() in execute() to track executions
 * ✅ Follows state management pattern (INITIALIZING -> READY -> EXECUTING -> READY)
 * ✅ Uses proper error handling with setError()
 * ✅ Implements action system for inter-component communication
 * ✅ Uses consistent logging with component:id format
 * ✅ Follows naming conventions (CamelCase classes, snake_case configs)
 * ✅ Supports mock mode for testing without hardware
 * ✅ Provides configuration persistence via getCurrentConfig()
 * ✅ Uses default hydration pattern in applyConfig()
 */