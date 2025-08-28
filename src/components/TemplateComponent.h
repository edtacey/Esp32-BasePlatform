/**
 * @file TemplateComponent.h
 * @brief Template component demonstrating ESP32 IoT Orchestrator architecture
 * 
 * This template shows how to implement a compliant IoT capability component
 * following the established patterns for sensors, actuators, and orchestrators.
 * 
 * COPY THIS FILE AND REPLACE "Template" WITH YOUR COMPONENT NAME
 */

#pragma once

#include "BaseComponent.h"
// Add any hardware-specific includes here
// Examples:
// #include <DHT.h>           // For temperature sensors
// #include <Wire.h>          // For I2C devices
// #include <Servo.h>         // For servo motors
// #include <PubSubClient.h>  // For MQTT components

/**
 * @brief Component operational modes (customize for your component)
 * 
 * Define the different operational states your component can be in.
 * This is optional but recommended for complex components.
 */
enum class TemplateMode {
    INACTIVE = 0,    // Component not operating
    NORMAL = 1,      // Normal operation mode
    CALIBRATE = 2,   // Calibration mode
    MOCK = 3        // Mock mode for testing without hardware
};

/**
 * @brief Template component demonstrating IoT capability implementation
 * 
 * This template shows the complete implementation pattern for:
 * - SENSORS: Components that read data from environment/hardware
 * - ACTUATORS: Components that control physical devices
 * - ORCHESTRATORS: Components that coordinate other components
 * 
 * KEY ARCHITECTURAL REQUIREMENTS:
 * 1. All components inherit from BaseComponent
 * 2. Must implement 4 pure virtual methods + 4 configuration methods
 * 3. Use schema-driven configuration with defaults
 * 4. Follow naming conventions and state management patterns
 * 5. Implement action system for inter-component communication
 * 6. Call updateExecutionStats() and setNextExecutionMs() properly
 */
class TemplateComponent : public BaseComponent {
public:
    /**
     * @brief Constructor following standard pattern
     * @param id Unique component identifier (e.g., "temp-sensor-1", "pump-controller-1")
     * @param name Human-readable component name (e.g., "Living Room Temperature")
     * @param storage Reference to ConfigStorage instance (provided by orchestrator)
     * @param orchestrator Reference to orchestrator for inter-component communication
     */
    TemplateComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Destructor - calls cleanup()
     */
    ~TemplateComponent() override;

    // =====================================================================
    // REQUIRED: BaseComponent pure virtual methods (MUST implement all 4)
    // =====================================================================
    
    /**
     * @brief Get default schema with all configuration parameters
     * 
     * CRITICAL: This defines your component's configuration structure.
     * Use JSON Schema format with default values for all parameters.
     * The schema drives automatic configuration persistence and validation.
     * 
     * @return JsonDocument with complete JSON Schema including defaults
     */
    JsonDocument getDefaultSchema() const override;
    
    /**
     * @brief Initialize component with configuration
     * 
     * STANDARD PATTERN:
     * 1. Set state to INITIALIZING
     * 2. Call loadConfiguration(config) - handles defaults and persistence
     * 3. Apply configuration via applyConfig(getConfiguration())
     * 4. Initialize hardware/resources
     * 5. Set initial execution time with setNextExecutionMs()
     * 6. Set state to READY
     * 
     * @param config Configuration document (may be empty for defaults)
     * @return true if initialization successful
     */
    bool initialize(const JsonDocument& config) override;
    
    /**
     * @brief Execute component's main function
     * 
     * STANDARD PATTERN:
     * 1. Set state to EXECUTING
     * 2. Perform main component logic (read sensor, control actuator, etc.)
     * 3. Create result data as JsonDocument
     * 4. Call updateExecutionStats() to track execution count
     * 5. Call setNextExecutionMs() to schedule next execution
     * 6. Set state back to READY
     * 7. Return ExecutionResult with success status and data
     * 
     * @return Execution result with data and status
     */
    ExecutionResult execute() override;
    
    /**
     * @brief Cleanup resources when component is destroyed
     * 
     * Free any allocated hardware resources, close connections, etc.
     */
    void cleanup() override;

protected:
    // =====================================================================
    // REQUIRED: Configuration management methods (MUST implement both)
    // =====================================================================
    
    /**
     * @brief Get current component configuration as JSON
     * 
     * Return JsonDocument containing ALL current component settings.
     * This enables configuration persistence and API access.
     * Include all member variables that define component behavior.
     * 
     * @return JsonDocument containing all current settings
     */
    JsonDocument getCurrentConfig() const override;
    
    /**
     * @brief Apply configuration to component variables
     * 
     * CRITICAL PATTERN: Use default hydration with | operator:
     * m_myVariable = config["myVariable"] | defaultValue;
     * 
     * This ensures components work even with empty configurations.
     * 
     * @param config Configuration to apply (may be empty for defaults)
     * @return true if configuration applied successfully
     */
    bool applyConfig(const JsonDocument& config) override;

    // =====================================================================
    // REQUIRED: Action system methods (MUST implement both)
    // =====================================================================
    
    /**
     * @brief Get supported actions for this component
     * 
     * Define actions that other components can call on this component.
     * Actions enable inter-component communication and external control.
     * 
     * @return Vector of ComponentAction definitions with parameter validation
     */
    std::vector<ComponentAction> getSupportedActions() const override;
    
    /**
     * @brief Execute component action with validated parameters
     * 
     * Implement the actual logic for each action defined in getSupportedActions().
     * Parameters are pre-validated by BaseComponent.
     * 
     * @param actionName Name of action to execute
     * @param parameters Validated JSON parameters
     * @return ActionResult with execution status and data
     */
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;

private:
    // =====================================================================
    // COMPONENT-SPECIFIC: Hardware and configuration variables
    // =====================================================================
    
    // Hardware configuration (customize for your component type)
    uint8_t m_gpioPin = 0;                    // GPIO pin (0 = mock mode)
    uint32_t m_samplingIntervalMs = 5000;     // How often to execute (milliseconds)
    
    // Component-specific configuration
    String m_deviceName = "Template Device";   // User-friendly device name
    bool m_enableMockMode = true;             // Enable mock data generation
    float m_configParameter1 = 1.0f;         // Example numeric parameter
    String m_configParameter2 = "default";   // Example string parameter
    
    // Runtime state variables
    TemplateMode m_currentMode = TemplateMode::INACTIVE;
    float m_lastReading = 0.0f;               // Last sensor reading (for sensors)
    bool m_actuatorState = false;             // Current actuator state (for actuators)
    uint32_t m_lastOperationTime = 0;         // Timestamp of last operation
    
    // Statistics (track component performance)
    uint32_t m_successfulOperations = 0;
    uint32_t m_failedOperations = 0;
    
    // =====================================================================
    // COMPONENT-SPECIFIC: Private helper methods
    // =====================================================================
    
    /**
     * @brief Initialize hardware resources
     * Set up GPIO pins, I2C, sensors, actuators, etc.
     * @return true if hardware initialized successfully
     */
    bool initializeHardware();
    
    /**
     * @brief Perform main component operation
     * - For SENSORS: Read data from hardware
     * - For ACTUATORS: Update physical state
     * - For ORCHESTRATORS: Coordinate other components
     * @return true if operation successful
     */
    bool performMainOperation();
    
    /**
     * @brief Generate mock data (when in mock mode)
     * @return Mock data appropriate for component type
     */
    JsonDocument generateMockData();
    
    /**
     * @brief Validate component-specific constraints
     * @return true if current state is valid
     */
    bool validateComponentState();
};