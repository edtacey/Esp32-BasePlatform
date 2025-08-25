/**
 * @file BaseComponent.cpp
 * @brief BaseComponent implementation
 */

#include "BaseComponent.h"

BaseComponent::BaseComponent(const String& id, const String& type, const String& name)
    : m_componentId(id)
    , m_componentType(type) 
    , m_componentName(name)
{
    log(Logger::DEBUG, "BaseComponent created: " + id + " (" + type + ")");
}

bool BaseComponent::loadConfiguration(const JsonDocument& config) {
    log(Logger::DEBUG, "Loading configuration for " + m_componentId);
    
    // Get default schema from derived class
    m_schema = getDefaultSchema();
    
    if (m_schema.isNull() || m_schema.size() == 0) {
        setError("Failed to get default schema from component");
        return false;
    }
    
    // If no config provided, use defaults from schema
    if (config.isNull() || config.size() == 0) {
        log(Logger::INFO, "Using default configuration (no override provided)");
        m_configuration = m_schema["properties"];
    } else {
        // Merge provided config with defaults
        log(Logger::INFO, "Merging provided configuration with defaults");
        m_configuration = mergeConfiguration(m_schema["properties"], config);
    }
    
    // Validate the final configuration
    if (!validateConfiguration(m_configuration)) {
        setError("Configuration validation failed");
        return false;
    }
    
    log(Logger::INFO, "Configuration loaded successfully");
    return true;
}

bool BaseComponent::validateConfiguration(const JsonDocument& config) {
    // Basic validation - check for required fields from schema
    if (m_schema["required"].is<JsonArray>()) {
        JsonArray required = m_schema["required"];
        for (JsonVariant requiredField : required) {
            String fieldName = requiredField.as<String>();
            if (!config[fieldName].is<JsonVariant>()) {
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

void BaseComponent::updateExecutionStats() {
    m_lastExecutionMs = millis();
    m_executionCount++;
}