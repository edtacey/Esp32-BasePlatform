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

protected:
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
};

#endif // BASE_COMPONENT_H