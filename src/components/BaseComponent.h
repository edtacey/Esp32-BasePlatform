/**
 * @file BaseComponent.h
 * @brief Base class for all IoT components with schema support
 * 
 * This class provides the foundation for all components with:
 * - Schema-driven configuration
 * - Default property definitions
 * - Persistent storage integration
 */

#ifndef BASE_COMPONENT_H
#define BASE_COMPONENT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../utils/Logger.h"
#include "../storage/ConfigStorage.h"

// Forward declaration
class Orchestrator;

/**
 * @brief Component execution states
 */
enum class ComponentState {
    UNINITIALIZED = 0,
    INITIALIZING = 1,
    READY = 2,
    EXECUTING = 3,
    ERROR = 4,
    COMPONENT_DISABLED = 5
};

/**
 * @brief Component execution result
 */
struct ExecutionResult {
    bool success = false;
    String message = "";
    JsonDocument data;
    uint32_t executionTimeMs = 0;
};

/**
 * @brief Action parameter types for validation
 */
enum class ActionParameterType {
    INTEGER,
    FLOAT,
    STRING, 
    BOOLEAN,
    ARRAY,
    OBJECT
};

/**
 * @brief Action parameter definition with validation constraints
 */
struct ActionParameter {
    String name;
    ActionParameterType type;
    bool required = true;
    JsonVariant defaultValue;
    float minValue = 0;
    float maxValue = 0;
    uint32_t maxLength = 0;
    String description = "";
};

/**
 * @brief Component action definition
 */
struct ComponentAction {
    String name;
    String description;
    std::vector<ActionParameter> parameters;
    uint32_t timeoutMs = 30000;  // Default 30s timeout
    bool requiresReady = true;   // Requires component to be in READY state
};

/**
 * @brief Action execution result
 */
struct ActionResult {
    bool success = false;
    String message = "";
    JsonDocument data;
    uint32_t executionTimeMs = 0;
    String actionName = "";
};

/**
 * @brief Base class for all IoT components
 * 
 * Provides schema-driven configuration, lifecycle management,
 * and integration with the orchestrator system.
 */
class BaseComponent {
protected:
    String m_componentId;
    String m_componentType;
    String m_componentName;
    ComponentState m_state = ComponentState::UNINITIALIZED;
    
    JsonDocument m_configuration;
    JsonDocument m_schema;
    JsonDocument m_lastData;
    String m_lastDataString;  // Alternative storage for debugging
    String m_lastError;
    
    uint32_t m_nextExecutionMs = 0;
    uint32_t m_lastExecutionMs = 0;
    uint32_t m_executionCount = 0;
    uint32_t m_errorCount = 0;
    
    // Storage reference
    ConfigStorage& m_storage;
    
    // Orchestrator reference for schedule notifications (optional)
    Orchestrator* m_orchestrator;

public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param type Component type string
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for schedule notifications
     */
    BaseComponent(const String& id, const String& type, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~BaseComponent() = default;

    // === Pure Virtual Methods (must be implemented by derived classes) ===
    
    /**
     * @brief Get the default schema for this component type
     * 
     * This method MUST be implemented by derived classes to define
     * the baseline schema with default values.
     * 
     * @return JsonDocument containing the default schema
     */
    virtual JsonDocument getDefaultSchema() const = 0;
    
    /**
     * @brief Initialize the component with configuration
     * @param config Configuration document (may be empty for defaults)
     * @return true if initialization successful
     */
    virtual bool initialize(const JsonDocument& config) = 0;
    
    /**
     * @brief Execute the component's main function
     * @return Execution result with data and status
     */
    virtual ExecutionResult execute() = 0;
    
    /**
     * @brief Cleanup resources when component is destroyed
     */
    virtual void cleanup() = 0;

    // === Configuration and Schema Management ===
    
    /**
     * @brief Load configuration from schema (with fallback to defaults)
     * @param config Optional configuration override
     * @return true if configuration loaded successfully
     */
    bool loadConfiguration(const JsonDocument& config = JsonDocument());
    
    /**
     * @brief Get current configuration
     * @return Current configuration document
     */
    const JsonDocument& getConfiguration() const { return m_configuration; }
    
    /**
     * @brief Get component schema
     * @return Schema document
     */
    const JsonDocument& getSchema() const { return m_schema; }
    
    /**
     * @brief Validate configuration against schema
     * @param config Configuration to validate
     * @return true if valid
     */
    bool validateConfiguration(const JsonDocument& config);

    // === State Management ===
    
    /**
     * @brief Get component state
     * @return Current component state
     */
    ComponentState getState() const { return m_state; }
    
    /**
     * @brief Set component state
     * @param newState New state to set
     */
    void setState(ComponentState newState);
    
    /**
     * @brief Get state as string
     * @return State as human-readable string
     */
    String getStateString() const;

    // === Component Information ===
    
    /**
     * @brief Get component ID
     * @return Component identifier
     */
    const String& getId() const { return m_componentId; }
    
    /**
     * @brief Get component type
     * @return Component type string
     */
    const String& getType() const { return m_componentType; }
    
    /**
     * @brief Get component name
     * @return Component name
     */
    const String& getName() const { return m_componentName; }

    // === Execution Management ===
    
    /**
     * @brief Get next scheduled execution time
     * @return Timestamp in milliseconds
     */
    uint32_t getNextExecutionMs() const { return m_nextExecutionMs; }
    
    /**
     * @brief Set next execution time
     * @param ms Timestamp in milliseconds
     */
    void setNextExecutionMs(uint32_t ms) { m_nextExecutionMs = ms; }
    
    /**
     * @brief Check if component is ready to execute
     * @return true if ready to execute
     */
    bool isReadyToExecute() const;
    
    /**
     * @brief Request orchestrator to update another component's schedule
     * @param componentId ID of component to update
     * @param timeToWakeUp New execution time in milliseconds
     * @return true if request was successful
     */
    bool requestScheduleUpdate(const String& componentId, uint32_t timeToWakeUp);

    // === Error Management ===
    
    /**
     * @brief Get last error message
     * @return Error message string
     */
    const String& getLastError() const { return m_lastError; }
    
    /**
     * @brief Clear error state
     */
    void clearError();
    
    /**
     * @brief Get error count
     * @return Number of errors encountered
     */
    uint32_t getErrorCount() const { return m_errorCount; }

    // === Statistics ===
    
    /**
     * @brief Get execution statistics
     * @return Statistics as JSON document
     */
    JsonDocument getStatistics() const;
    
    /**
     * @brief Get last execution data
     * @return Last execution data as JSON document
     */
    const JsonDocument& getLastExecutionData() const { return m_lastData; }
    
    /**
     * @brief Get core sensor data (for lightweight dashboard display)
     * 
     * Components should override this to return only essential sensor values
     * without diagnostic/debug information. Default implementation returns
     * the full last execution data.
     * 
     * Example core data for a temperature sensor:
     * {
     *   "temperature": 23.5,
     *   "humidity": 65.2,
     *   "timestamp": 12345
     * }
     * 
     * @return Core sensor data as JSON document
     */
    virtual JsonDocument getCoreData() const;
    
    /**
     * @brief Store execution data as string (for debugging)
     * @param data JSON string to store
     */
    void storeExecutionDataString(const String& data) { m_lastDataString = data; }
    
    /**
     * @brief Get last execution data as string
     * @return Last execution data as JSON string
     */
    const String& getLastExecutionDataString() const { return m_lastDataString; }
    
    /**
     * @brief Fetch remote data via orchestrator HTTP service
     * @param url Full URL to fetch from
     * @param timeoutMs HTTP timeout (default: 5000ms)  
     * @return JsonDocument with response or error info
     */
    JsonDocument fetchRemoteData(const String& url, uint32_t timeoutMs = 5000);
    
    // === Enhanced Configuration Persistence ===
    
    /**
     * @brief Save current component configuration to persistent storage
     * Base class orchestrates: calls child's getCurrentConfig() then saves to LittleFS
     * @return true if saved successfully
     */
    bool saveCurrentConfiguration();
    
    /**
     * @brief Load configuration from persistent storage and apply
     * Base class orchestrates: loads from LittleFS then calls child's applyConfig()
     * @return true if loaded and applied successfully
     */
    bool loadStoredConfiguration();
    
    /**
     * @brief Get current component configuration as JSON (public accessor)
     * @return JsonDocument containing all current settings
     */
    JsonDocument getConfigurationAsJson() const { return getCurrentConfig(); }
    
    /**
     * @brief Apply configuration to component variables (public accessor)
     * @param config Configuration to apply (may be empty for defaults)
     * @return true if configuration applied successfully
     */
    bool applyConfigurationFromJson(const JsonDocument& config) { return applyConfig(config); }

    // === Component Action System ===
    
    /**
     * @brief Get list of available actions for this component
     * @return Vector of available actions with parameter definitions
     */
    std::vector<ComponentAction> getAvailableActions() const { return getSupportedActions(); }
    
    /**
     * @brief Execute a component action with validated parameters
     * @param actionName Name of the action to execute
     * @param parameters JSON object containing action parameters
     * @return Action execution result with success status and data
     */
    ActionResult executeAction(const String& actionName, const JsonDocument& parameters);
    
    /**
     * @brief Validate action parameters against action definition
     * @param action Action definition containing parameter constraints
     * @param parameters JSON object with parameter values to validate
     * @return true if all parameters are valid
     */
    bool validateActionParameters(const ComponentAction& action, const JsonDocument& parameters);

protected:
    // === Configuration Management (Child classes MUST implement) ===
    
    /**
     * @brief Get current component configuration as JSON
     * Child classes must return their complete current state
     * @return JsonDocument containing all current settings
     */
    virtual JsonDocument getCurrentConfig() const = 0;
    
    /**
     * @brief Apply configuration to component variables
     * Child classes must populate all variables from config or defaults
     * @param config Configuration to apply (may be empty for defaults)
     * @return true if configuration applied successfully
     */
    virtual bool applyConfig(const JsonDocument& config) = 0;
    
    // === Action System (Child classes MUST implement) ===
    
    /**
     * @brief Get supported actions for this component type
     * Child classes must return their action definitions with parameter validation
     * @return Vector of ComponentAction definitions
     */
    virtual std::vector<ComponentAction> getSupportedActions() const = 0;
    
    /**
     * @brief Execute a specific action with validated parameters
     * Child classes implement the actual action logic
     * @param actionName Name of the action to execute
     * @param parameters Validated JSON parameters
     * @return ActionResult with execution status and data
     */
    virtual ActionResult performAction(const String& actionName, const JsonDocument& parameters) = 0;
    
    /**
     * @brief Set error message and update state
     * @param error Error message
     */
    void setError(const String& error);
    
    /**
     * @brief Log component message
     * @param level Log level
     * @param message Message to log
     */
    void log(Logger::Level level, const String& message) const;
    
    /**
     * @brief Merge default values with provided configuration
     * @param defaults Default configuration values
     * @param overrides Configuration overrides
     * @return Merged configuration
     */
    JsonDocument mergeConfiguration(const JsonDocument& defaults, const JsonDocument& overrides);
    
    /**
     * @brief Extract default values from schema properties
     * @param schema JSON schema containing properties with default values
     * @return Configuration with default values extracted
     */
    JsonDocument extractDefaultValues(const JsonDocument& schema);
    
    /**
     * @brief Save configuration to persistent storage
     * @param config Configuration to save
     * @return True if saved successfully
     */
    bool saveConfigurationToStorage(const JsonDocument& config);
    
    /**
     * @brief Load configuration from persistent storage
     * @return Configuration document (empty if not found)
     */
    JsonDocument loadConfigurationFromStorage();
    
    /**
     * @brief Get state as string for any state
     * @param state State to convert to string
     * @return State as human-readable string
     */
    String getStateString(ComponentState state) const;
    
    /**
     * @brief Update last execution timestamp and increment counter
     */
    void updateExecutionStats();
    
    /**
     * @brief Validate individual parameter value against constraints
     * @param param Parameter definition with validation constraints
     * @param value Parameter value to validate
     * @return true if parameter value is valid
     */
    bool validateParameterValue(const ActionParameter& param, JsonVariantConst value);
};

#endif // BASE_COMPONENT_H