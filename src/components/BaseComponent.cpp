/**
 * @file BaseComponent.cpp
 * @brief BaseComponent implementation
 */

#include "BaseComponent.h"
#include "../core/Orchestrator.h"
#include <LittleFS.h>

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
    log(Logger::INFO, "🔥 [BOOT-TRACE] ==================== LOADING CONFIG FOR " + m_componentId + " ====================");
    
    // STEP 0: Check if LittleFS is ready and mounted
    log(Logger::INFO, "🔥 [BOOT-TRACE] Checking LittleFS status...");
    if (!LittleFS.begin()) {
        log(Logger::ERROR, "🔥 [BOOT-TRACE] ❌ CRITICAL: LittleFS not mounted during component initialization!");
        return false;
    } else {
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        log(Logger::INFO, "🔥 [BOOT-TRACE] ✅ LittleFS is mounted and ready: " + String(usedBytes) + "/" + String(totalBytes) + " bytes used");
    }
    
    // STEP 1: Check if config file already exists BEFORE we do anything
    String configFilePath = "/config/components/" + m_componentId + ".json";
    log(Logger::INFO, "🔥 [BOOT-TRACE] Checking for existing config file: " + configFilePath);
    
    File existingFile = LittleFS.open(configFilePath, "r");
    if (existingFile) {
        String existingContent = existingFile.readString();
        existingFile.close();
        log(Logger::INFO, "🔥 [BOOT-TRACE] ✅ EXISTING CONFIG FILE FOUND! Content: " + existingContent);
        
        // Parse and extract remoteHost if present
        JsonDocument existingConfig;
        if (deserializeJson(existingConfig, existingContent) == DeserializationError::Ok) {
            if (existingConfig.containsKey("remoteHost")) {
                String existingRemoteHost = existingConfig["remoteHost"].as<String>();
                log(Logger::INFO, "🔥 [BOOT-TRACE] EXISTING FILE remoteHost: '" + existingRemoteHost + "'");
            }
        }
    } else {
        log(Logger::INFO, "🔥 [BOOT-TRACE] No existing config file found - this will be a fresh initialization");
    }
    
    // Log the incoming config parameter
    String incomingConfigStr;
    serializeJson(config, incomingConfigStr);
    log(Logger::INFO, "🔥 [BOOT-TRACE] Incoming config parameter: " + incomingConfigStr);
    
    // Get default schema from derived class
    m_schema = getDefaultSchema();
    
    if (m_schema.isNull() || m_schema.size() == 0) {
        setError("Failed to get default schema from component");
        return false;
    }
    
    log(Logger::INFO, "🔥 [BOOT-TRACE] Schema loaded successfully");
    
    // Step 1: Extract defaults from schema
    JsonDocument defaults = extractDefaultValues(m_schema);
    
    // Debug: log extracted defaults with remoteHost tracking
    String defaultsStr;
    serializeJson(defaults, defaultsStr);
    log(Logger::INFO, "🔥 [BOOT-TRACE] Extracted defaults: " + defaultsStr);
    
    // Track remoteHost specifically
    if (defaults.containsKey("remoteHost")) {
        String remoteHostDefault = defaults["remoteHost"].as<String>();
        log(Logger::INFO, "🔥 [BOOT-TRACE] Schema default remoteHost: '" + remoteHostDefault + "'");
    }
    
    if (defaults.isNull() || defaults.size() == 0) {
        setError("Failed to extract default values from schema");
        return false;
    }
    
    // Step 2: *** CRITICAL FIX *** - Only save defaults if no config file exists
    File checkFile = LittleFS.open(configFilePath, "r");
    bool configFileExists = (bool)checkFile;
    if (checkFile) checkFile.close();
    
    if (configFileExists) {
        log(Logger::INFO, "🔥 [BOOT-TRACE] ✅ SKIPPING defaults save - existing config file found, preserving it!");
    } else {
        log(Logger::INFO, "🔥 [BOOT-TRACE] No existing config file - saving defaults for first-time setup");
        if (!saveConfigurationToStorage(defaults)) {
            log(Logger::WARNING, "🔥 [BOOT-TRACE] Failed to save defaults - continuing anyway");
        } else {
            log(Logger::INFO, "🔥 [BOOT-TRACE] ✅ Defaults saved to storage for first-time setup");
        }
    }
    
    // Step 3: Attempt to reload from storage to validate save worked
    log(Logger::INFO, "🔥 [BOOT-TRACE] Reloading configuration from storage...");
    JsonDocument storedConfig = loadConfigurationFromStorage();
    
    // Track remoteHost in stored config
    if (!storedConfig.isNull() && storedConfig.containsKey("remoteHost")) {
        String storedRemoteHost = storedConfig["remoteHost"].as<String>();
        log(Logger::INFO, "🔥 [BOOT-TRACE] Stored config remoteHost: '" + storedRemoteHost + "'");
    } else {
        log(Logger::INFO, "🔥 [BOOT-TRACE] No remoteHost found in stored config or config is null");
    }
    
    // Step 4: Choose configuration source
    if (config.isNull() || config.size() == 0) {
        log(Logger::INFO, "🔥 [BOOT-TRACE] Incoming config is null/empty - choosing between stored and defaults");
        if (storedConfig.isNull() || storedConfig.size() == 0) {
            log(Logger::INFO, "🔥 [BOOT-TRACE] ❌ OVERRIDE DETECTED: Using schema defaults (stored config null/empty)");
            m_configuration = defaults;
        } else {
            log(Logger::INFO, "🔥 [BOOT-TRACE] ✅ Using stored configuration from LittleFS");
            m_configuration = storedConfig;
        }
    } else {
        // Track remoteHost in incoming config
        if (config.containsKey("remoteHost")) {
            String incomingRemoteHost = config["remoteHost"].as<String>();
            log(Logger::INFO, "🔥 [BOOT-TRACE] Incoming config remoteHost: '" + incomingRemoteHost + "'");
        }
        
        // Merge provided config with defaults
        log(Logger::INFO, "🔥 [BOOT-TRACE] ❌ OVERRIDE DETECTED: Merging incoming config with defaults (ignoring stored)");
        m_configuration = mergeConfiguration(defaults, config);
        
        // Save the merged config
        log(Logger::INFO, "🔥 [BOOT-TRACE] Saving merged configuration to storage");
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
    log(Logger::INFO, "🔥 [BOOT-TRACE] Final configuration: " + configStr);
    
    // Track final remoteHost value
    if (m_configuration.containsKey("remoteHost")) {
        String finalRemoteHost = m_configuration["remoteHost"].as<String>();
        log(Logger::INFO, "🔥 [BOOT-TRACE] FINAL remoteHost value: '" + finalRemoteHost + "'");
    }
    
    return true;
}

bool BaseComponent::validateConfiguration(const JsonDocument& config) {
    // With default hydration architecture, no fields are truly "required"
    // since applyConfig() provides defaults for all missing values
    // 
    // Future validation logic could include:
    // - Range validation (e.g., port numbers 1-65535)
    // - Format validation (e.g., valid IP addresses)
    // - Logical validation (e.g., start_time < end_time)
    
    log(Logger::DEBUG, "Configuration validation passed (using default hydration)");
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
    uint32_t currentTime = millis();
    bool stateReady = (m_state == ComponentState::READY);
    bool timeReady = (currentTime >= m_nextExecutionMs);
    bool neverExecuted = (m_executionCount == 0);
    
    // Force execution if never executed and component is ready
    if (stateReady && neverExecuted) {
        return true;
    }
    
    return stateReady && timeReady;
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
    stats["execution_count"] = m_executionCount;
    stats["error_count"] = m_errorCount;
    stats["last_execution_ms"] = m_lastExecutionMs;
    stats["nextExecutionMs"] = m_nextExecutionMs;
    stats["uptime"] = millis();
    
    return stats;
}

JsonDocument BaseComponent::getCoreData() const {
    // Default implementation returns last execution data
    // Derived classes should override this to return only essential sensor values
    return m_lastData;
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
    log(Logger::INFO, "🔵 [SAVE] Saving configuration to storage for: " + m_componentId);
    
    // Log what we're saving
    String configStr;
    serializeJson(config, configStr);
    log(Logger::INFO, "🔵 [SAVE] Configuration content: " + configStr);
    
    // Use ConfigStorage to save component configuration
    bool success = m_storage.saveComponentConfig(m_componentId, config);
    
    if (success) {
        log(Logger::INFO, "✅ [SAVE] Successfully saved configuration to storage: " + m_componentId);
    } else {
        log(Logger::ERROR, "❌ [SAVE] Failed to save configuration to storage: " + m_componentId);
    }
    
    return success;
}

JsonDocument BaseComponent::loadConfigurationFromStorage() {
    JsonDocument config;
    
    log(Logger::INFO, "🔷 [LOAD] Loading configuration from storage for: " + m_componentId);
    
    // Check if config exists first
    if (m_storage.hasComponentConfig(m_componentId)) {
        log(Logger::INFO, "🔷 [LOAD] Found stored config file for: " + m_componentId);
    } else {
        log(Logger::WARNING, "⚠️ [LOAD] No stored config file exists for: " + m_componentId);
    }
    
    // Use ConfigStorage to load component configuration
    bool success = m_storage.loadComponentConfig(m_componentId, config);
    
    if (success) {
        String loadedStr;
        serializeJson(config, loadedStr);
        log(Logger::INFO, "✅ [LOAD] Successfully loaded config: " + loadedStr);
    } else {
        log(Logger::WARNING, "⚠️ [LOAD] Failed to load config or file doesn't exist for: " + m_componentId);
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

// === Enhanced Configuration Persistence Implementation ===

bool BaseComponent::saveCurrentConfiguration() {
    log(Logger::INFO, "🔥 [SAVE-TRACE] Starting saveCurrentConfiguration for: " + m_componentId);
    
    try {
        // Ask child class for its complete current state
        log(Logger::INFO, "🔥 [SAVE-TRACE] Calling getCurrentConfig()...");
        JsonDocument currentConfig = getCurrentConfig();
        
        // Track remoteHost in current config before saving
        if (currentConfig.containsKey("remoteHost")) {
            String currentRemoteHost = currentConfig["remoteHost"].as<String>();
            log(Logger::INFO, "🔥 [SAVE-TRACE] getCurrentConfig() returned remoteHost: '" + currentRemoteHost + "'");
        } else {
            log(Logger::INFO, "🔥 [SAVE-TRACE] getCurrentConfig() returned NO remoteHost field");
        }
        
        if (currentConfig.isNull() || currentConfig.size() == 0) {
            log(Logger::WARNING, "⚠️ Child class returned empty configuration");
            return false;
        }
        
        // Add component metadata for persistence
        currentConfig["component_type"] = m_componentType;
        currentConfig["component_name"] = m_componentName;
        
        // Enhanced logging of the configuration being saved
        String configStr;
        serializeJson(currentConfig, configStr);
        log(Logger::INFO, "📝 Saving config to storage: " + configStr);
        
        // Base class handles the storage operation
        log(Logger::INFO, "🔥 [SAVE-TRACE] Calling m_storage.saveComponentConfig()...");
        bool success = m_storage.saveComponentConfig(m_componentId, currentConfig);
        
        if (success) {
            log(Logger::INFO, "🔥 [SAVE-TRACE] ✅ Configuration saved successfully to LittleFS");
        } else {
            log(Logger::ERROR, "🔥 [SAVE-TRACE] ❌ Failed to save configuration to LittleFS");
        }
        
        return success;
        
    } catch (...) {
        log(Logger::ERROR, "Exception during configuration save");
        return false;
    }
}

bool BaseComponent::loadStoredConfiguration() {
    log(Logger::INFO, "Loading configuration from persistent storage");
    
    try {
        JsonDocument storedConfig;
        
        // Base class handles the storage operation
        bool hasStoredConfig = m_storage.loadComponentConfig(m_componentId, storedConfig);
        
        if (hasStoredConfig) {
            // Debug log what was loaded
            String configStr;
            serializeJson(storedConfig, configStr);
            log(Logger::INFO, "🔄 Loaded stored config from persistence: " + configStr);
            
            // Ask child class to apply the loaded configuration
            bool applied = applyConfig(storedConfig);
            
            if (applied) {
                log(Logger::INFO, "✅ Stored configuration applied successfully");
                
                // Get current config to verify what was actually applied
                JsonDocument appliedConfig = getCurrentConfig();
                String appliedStr;
                serializeJson(appliedConfig, appliedStr);
                log(Logger::INFO, "🔍 Applied config verification: " + appliedStr);
                
                // Only save if the applied config is significantly different from stored
                // This prevents overriding stored configs with defaults
                if (appliedConfig.size() > storedConfig.size()) {
                    log(Logger::INFO, "💾 Saving expanded configuration (added missing defaults)");
                    saveCurrentConfiguration();
                } else {
                    log(Logger::INFO, "✅ Configuration matches stored - no save needed");
                }
                
            } else {
                log(Logger::ERROR, "❌ Failed to apply stored configuration");
                return false;
            }
            
        } else {
            log(Logger::INFO, "🆕 No stored configuration found, applying defaults");
            
            // No stored config - ask child to apply defaults
            JsonDocument emptyConfig;
            bool applied = applyConfig(emptyConfig);
            
            if (applied) {
                // Get the applied defaults for verification
                JsonDocument defaultConfig = getCurrentConfig();
                String defaultStr;
                serializeJson(defaultConfig, defaultStr);
                log(Logger::INFO, "🔍 Applied default config: " + defaultStr);
                
                // Save the default configuration for future use
                log(Logger::INFO, "💾 Default configuration applied, saving to storage");
                saveCurrentConfiguration();
            } else {
                log(Logger::ERROR, "❌ Failed to apply default configuration");
                return false;
            }
        }
        
        return true;
        
    } catch (...) {
        log(Logger::ERROR, "Exception during configuration load");
        return false;
    }
}

// === Component Action System Implementation ===

ActionResult BaseComponent::executeAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.actionName = actionName;
    uint32_t startTime = millis();
    
    log(Logger::INFO, String("Executing action: ") + actionName);
    
    // Get supported actions from child class
    std::vector<ComponentAction> actions = getSupportedActions();
    ComponentAction* targetAction = nullptr;
    
    // Find the requested action
    for (auto& action : actions) {
        if (action.name == actionName) {
            targetAction = &action;
            break;
        }
    }
    
    if (!targetAction) {
        result.success = false;
        result.message = "Action not supported: " + actionName;
        log(Logger::ERROR, result.message);
        return result;
    }
    
    // Check component state requirements
    if (targetAction->requiresReady && m_state != ComponentState::READY) {
        result.success = false;
        result.message = String("Component not ready for action. Current state: ") + getStateString();
        log(Logger::WARNING, result.message);
        return result;
    }
    
    // Validate parameters
    if (!validateActionParameters(*targetAction, parameters)) {
        result.success = false;
        result.message = "Invalid parameters for action: " + actionName;
        log(Logger::ERROR, result.message);
        return result;
    }
    
    // Set component state to executing
    ComponentState originalState = m_state;
    setState(ComponentState::EXECUTING);
    
    try {
        // Execute the action in the child class
        result = performAction(actionName, parameters);
        
        result.executionTimeMs = millis() - startTime;
        
        if (result.success) {
            log(Logger::INFO, String("Action completed successfully: ") + actionName + 
                             " (" + result.executionTimeMs + "ms)");
        } else {
            log(Logger::ERROR, String("Action failed: ") + actionName + " - " + result.message);
        }
        
    } catch (...) {
        result.success = false;
        result.message = "Exception during action execution";
        result.executionTimeMs = millis() - startTime;
        log(Logger::ERROR, result.message);
    }
    
    // Restore component state
    setState(originalState);
    
    return result;
}

bool BaseComponent::validateActionParameters(const ComponentAction& action, const JsonDocument& parameters) {
    log(Logger::DEBUG, String("Validating parameters for action: ") + action.name);
    
    // Check each required parameter
    for (const ActionParameter& param : action.parameters) {
        if (param.required && !parameters[param.name].is<JsonVariant>()) {
            log(Logger::ERROR, String("Missing required parameter: ") + param.name);
            return false;
        }
        
        if (parameters[param.name].isNull() && param.required) {
            log(Logger::ERROR, String("Required parameter is null: ") + param.name);
            return false;
        }
        
        if (!parameters[param.name].isNull()) {
            // Validate parameter type and constraints
            if (!validateParameterValue(param, parameters[param.name])) {
                return false;
            }
        }
    }
    
    log(Logger::DEBUG, "Parameter validation successful");
    return true;
}

bool BaseComponent::validateParameterValue(const ActionParameter& param, JsonVariantConst value) {
    switch (param.type) {
        case ActionParameterType::INTEGER:
            if (!value.is<int>()) {
                log(Logger::ERROR, String("Parameter ") + param.name + " must be an integer");
                return false;
            }
            if (param.minValue != param.maxValue) {
                int intVal = value.as<int>();
                if (intVal < param.minValue || intVal > param.maxValue) {
                    log(Logger::ERROR, String("Parameter ") + param.name + " out of range: " + 
                                       intVal + " (allowed: " + param.minValue + "-" + param.maxValue + ")");
                    return false;
                }
            }
            break;
            
        case ActionParameterType::FLOAT:
            if (!value.is<float>()) {
                log(Logger::ERROR, String("Parameter ") + param.name + " must be a float");
                return false;
            }
            if (param.minValue != param.maxValue) {
                float floatVal = value.as<float>();
                if (floatVal < param.minValue || floatVal > param.maxValue) {
                    log(Logger::ERROR, String("Parameter ") + param.name + " out of range: " + 
                                       floatVal + " (allowed: " + param.minValue + "-" + param.maxValue + ")");
                    return false;
                }
            }
            break;
            
        case ActionParameterType::STRING:
            if (!value.is<const char*>()) {
                log(Logger::ERROR, String("Parameter ") + param.name + " must be a string");
                return false;
            }
            if (param.maxLength > 0) {
                String strVal = value.as<String>();
                if (strVal.length() > param.maxLength) {
                    log(Logger::ERROR, String("Parameter ") + param.name + " too long: " + 
                                       strVal.length() + " chars (max: " + param.maxLength + ")");
                    return false;
                }
            }
            break;
            
        case ActionParameterType::BOOLEAN:
            if (!value.is<bool>()) {
                log(Logger::ERROR, String("Parameter ") + param.name + " must be a boolean");
                return false;
            }
            break;
            
        case ActionParameterType::ARRAY:
            if (!value.is<JsonArray>()) {
                log(Logger::ERROR, String("Parameter ") + param.name + " must be an array");
                return false;
            }
            break;
            
        case ActionParameterType::OBJECT:
            if (!value.is<JsonObject>()) {
                log(Logger::ERROR, String("Parameter ") + param.name + " must be an object");
                return false;
            }
            break;
    }
    
    return true;
}

JsonDocument BaseComponent::fetchRemoteData(const String& url, uint32_t timeoutMs) {
    JsonDocument result;
    
    // Delegate to orchestrator if available
    if (m_orchestrator) {
        log(Logger::DEBUG, "Delegating fetchRemoteData to Orchestrator: " + url);
        return m_orchestrator->fetchRemoteData(url, timeoutMs);
    }
    
    // Fallback if no orchestrator
    result["error"] = "No orchestrator available for remote fetch";
    log(Logger::ERROR, "fetchRemoteData called but no orchestrator available");
    result["success"] = false;
    
    return result;
}