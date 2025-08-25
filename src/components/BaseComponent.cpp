/**
 * @file BaseComponent.cpp
 * @brief BaseComponent implementation
 */

#include "BaseComponent.h"
#include "../core/Orchestrator.h"

BaseComponent::BaseComponent(const String& id, const String& type, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : m_componentId(id)
    , m_componentType(type) 
    , m_componentName(name)
    , m_storage(storage)
    , m_orchestrator(orchestrator)
{
    log(Logger::DEBUG, "BaseComponent created: " + id + " (" + type + ")");
}

bool BaseComponent::loadConfiguration(const JsonDocument& config) {
    log(Logger::INFO, "Loading configuration for " + m_componentId);
    
    // Get default schema from derived class
    m_schema = getDefaultSchema();
    
    if (m_schema.isNull() || m_schema.size() == 0) {
        setError("Failed to get default schema from component");
        return false;
    }
    
    // Step 1: Extract defaults from schema
    JsonDocument defaults = extractDefaultValues(m_schema);
    
    // Debug: log extracted defaults
    String defaultsStr;
    serializeJson(defaults, defaultsStr);
    log(Logger::DEBUG, "Extracted defaults: " + defaultsStr);
    
    if (defaults.isNull() || defaults.size() == 0) {
        setError("Failed to extract default values from schema");
        return false;
    }
    
    // Step 2: Save defaults to storage for persistence validation
    log(Logger::INFO, "Saving default configuration to validate persistence");
    if (!saveConfigurationToStorage(defaults)) {
        log(Logger::WARNING, "Failed to save defaults - continuing anyway");
    }
    
    // Step 3: Attempt to reload from storage to validate save worked
    log(Logger::INFO, "Reloading configuration to validate persistence");
    JsonDocument storedConfig = loadConfigurationFromStorage();
    
    // Step 4: Choose configuration source
    if (config.isNull() || config.size() == 0) {
        if (storedConfig.isNull() || storedConfig.size() == 0) {
            log(Logger::INFO, "No stored config found - using extracted defaults");
            m_configuration = defaults;
        } else {
            log(Logger::INFO, "Using validated stored configuration");
            m_configuration = storedConfig;
        }
    } else {
        // Merge provided config with defaults
        log(Logger::INFO, "Merging provided configuration with defaults");
        m_configuration = mergeConfiguration(defaults, config);
        
        // Save the merged config
        saveConfigurationToStorage(m_configuration);
    }
    
    // Step 5: Validate final configuration
    if (!validateConfiguration(m_configuration)) {
        setError("Final configuration validation failed");
        return false;
    }
    
    // Step 6: Log successful configuration with values
    String configStr;
    serializeJson(m_configuration, configStr);
    log(Logger::INFO, "Configuration validated successfully: " + configStr);
    
    return true;
}

bool BaseComponent::validateConfiguration(const JsonDocument& config) {
    // Basic validation - check for required fields from schema
    if (m_schema["required"].is<JsonArray>()) {
        JsonArray required = m_schema["required"];
        for (JsonVariant requiredField : required) {
            String fieldName = requiredField.as<String>();
            if (config[fieldName].isNull()) {
                setError("Missing required field: " + fieldName);
                return false;
            }
        }
    }
    
    // Additional validation can be added here
    // For now, we do basic validation
    
    return true;
}

void BaseComponent::setState(ComponentState newState) {
    if (m_state != newState) {
        ComponentState oldState = m_state;
        m_state = newState;
        
        log(Logger::INFO, "State changed: " + getStateString(oldState) + " -> " + getStateString());
    }
}

String BaseComponent::getStateString() const {
    return getStateString(m_state);
}

String BaseComponent::getStateString(ComponentState state) const {
    switch (state) {
        case ComponentState::UNINITIALIZED: return "UNINITIALIZED";
        case ComponentState::INITIALIZING:  return "INITIALIZING";
        case ComponentState::READY:         return "READY";
        case ComponentState::EXECUTING:     return "EXECUTING";
        case ComponentState::ERROR:         return "ERROR";
        case ComponentState::COMPONENT_DISABLED: return "DISABLED";
        default:                            return "UNKNOWN";
    }
}

bool BaseComponent::isReadyToExecute() const {
    return (m_state == ComponentState::READY) && 
           (millis() >= m_nextExecutionMs);
}

void BaseComponent::clearError() {
    m_lastError = "";
    if (m_state == ComponentState::ERROR) {
        setState(ComponentState::READY);
    }
}

JsonDocument BaseComponent::getStatistics() const {
    JsonDocument stats;
    
    stats["componentId"] = m_componentId;
    stats["componentType"] = m_componentType;
    stats["state"] = getStateString();
    stats["executionCount"] = m_executionCount;
    stats["errorCount"] = m_errorCount;
    stats["lastExecutionMs"] = m_lastExecutionMs;
    stats["nextExecutionMs"] = m_nextExecutionMs;
    stats["uptime"] = millis();
    
    return stats;
}

void BaseComponent::setError(const String& error) {
    m_lastError = error;
    m_errorCount++;
    setState(ComponentState::ERROR);
    log(Logger::ERROR, error);
}

void BaseComponent::log(Logger::Level level, const String& message) const {
    String logComponent = m_componentType + ":" + m_componentId;
    switch (level) {
        case Logger::DEBUG:
            Logger::debug(logComponent.c_str(), message);
            break;
        case Logger::INFO:
            Logger::info(logComponent.c_str(), message);
            break;
        case Logger::WARNING:
            Logger::warning(logComponent.c_str(), message);
            break;
        case Logger::ERROR:
            Logger::error(logComponent.c_str(), message);
            break;
        case Logger::CRITICAL:
            Logger::critical(logComponent.c_str(), message);
            break;
    }
}

JsonDocument BaseComponent::mergeConfiguration(const JsonDocument& defaults, const JsonDocument& overrides) {
    JsonDocument merged;
    
    // Start with defaults
    merged.set(defaults);
    
    // Apply overrides - create a temporary writable copy
    JsonDocument overridesCopy;
    overridesCopy.set(overrides);
    
    if (overridesCopy.is<JsonObject>()) {
        JsonObject overridesObj = overridesCopy.as<JsonObject>();
        for (JsonPair pair : overridesObj) {
            merged[pair.key()] = pair.value();
        }
    }
    
    return merged;
}

JsonDocument BaseComponent::extractDefaultValues(const JsonDocument& schema) {
    JsonDocument defaults;
    
    // Create a writable copy to work with
    JsonDocument schemaCopy;
    schemaCopy.set(schema);
    
    // Check if schema has properties
    if (!schemaCopy["properties"].is<JsonObject>()) {
        log(Logger::WARNING, "Schema has no properties section");
        return defaults;
    }
    
    JsonObject properties = schemaCopy["properties"].as<JsonObject>();
    
    log(Logger::DEBUG, String("Schema has ") + properties.size() + " properties");
    
    // Extract default value from each property
    for (JsonPair prop : properties) {
        String propName = prop.key().c_str();
        JsonObject propDef = prop.value().as<JsonObject>();
        
        // Check if this property has a default value
        if (!propDef["default"].isNull()) {
            defaults[propName] = propDef["default"];
            log(Logger::DEBUG, String("Extracted default: ") + propName + " = " + propDef["default"].as<String>());
        }
    }
    
    return defaults;
}

bool BaseComponent::saveConfigurationToStorage(const JsonDocument& config) {
    log(Logger::INFO, "Saving configuration to storage");
    
    // Use ConfigStorage to save component configuration
    bool success = m_storage.saveComponentConfig(m_componentId, config);
    
    if (success) {
        log(Logger::DEBUG, "Successfully saved configuration to storage");
    } else {
        log(Logger::ERROR, "Failed to save configuration to storage");
    }
    
    return success;
}

JsonDocument BaseComponent::loadConfigurationFromStorage() {
    JsonDocument config;
    
    log(Logger::INFO, "Loading configuration from storage");
    
    // Use ConfigStorage to load component configuration
    bool success = m_storage.loadComponentConfig(m_componentId, config);
    
    if (success) {
        log(Logger::DEBUG, "Successfully loaded configuration from storage");
    } else {
        log(Logger::DEBUG, "No stored configuration found (using defaults)");
    }
    
    return config;
}

void BaseComponent::updateExecutionStats() {
    m_lastExecutionMs = millis();
    m_executionCount++;
}

bool BaseComponent::requestScheduleUpdate(const String& componentId, uint32_t timeToWakeUp) {
    if (!m_orchestrator) {
        log(Logger::WARNING, "Cannot request schedule update - no orchestrator reference");
        return false;
    }
    
    log(Logger::DEBUG, String("Requesting schedule update for ") + componentId + " to wake at " + timeToWakeUp + "ms");
    return m_orchestrator->updateNextCheck(componentId, timeToWakeUp);
}