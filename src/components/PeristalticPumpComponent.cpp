#include "PeristalticPumpComponent.h"

PeristalticPumpComponent::PeristalticPumpComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "PeristalticPump", name, storage, orchestrator) {
    log(Logger::DEBUG, "PeristalticPumpComponent created");
}

PeristalticPumpComponent::~PeristalticPumpComponent() {
    cleanup();
}

JsonDocument PeristalticPumpComponent::getDefaultSchema() const {
    JsonDocument schema;
    schema["type"] = "object";
    schema["title"] = "Peristaltic Pump Configuration";
    schema["required"].add("pin_no");
    schema["required"].add("mls_per_sec");
    
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // Pin number (required)
    JsonObject pinNo = properties["pin_no"].to<JsonObject>();
    pinNo["type"] = "integer";
    pinNo["minimum"] = 0;
    pinNo["maximum"] = 39;
    pinNo["default"] = 26;
    pinNo["description"] = "GPIO pin number for pump control";
    
    // Flow rate (required)
    JsonObject mlsPerSec = properties["mls_per_sec"].to<JsonObject>();
    mlsPerSec["type"] = "number";
    mlsPerSec["minimum"] = 0.1;
    mlsPerSec["maximum"] = 100.0;
    mlsPerSec["default"] = 40.0;
    mlsPerSec["description"] = "Flow rate in milliliters per second";
    
    // Board reference
    JsonObject boardRef = properties["board_ref"].to<JsonObject>();
    boardRef["type"] = "string";
    boardRef["default"] = "BOARD_1";
    boardRef["description"] = "Board reference identifier";
    
    // Liquid name
    JsonObject liquidName = properties["liquid_name"].to<JsonObject>();
    liquidName["type"] = "string";
    liquidName["default"] = "Unknown";
    liquidName["description"] = "Name of liquid being pumped";
    
    // Liquid concentration
    JsonObject liquidConc = properties["liquid_concentration"].to<JsonObject>();
    liquidConc["type"] = "number";
    liquidConc["minimum"] = 0.0;
    liquidConc["maximum"] = 100.0;
    liquidConc["default"] = 100.0;
    liquidConc["description"] = "Liquid concentration percentage (0-100%)";
    
    // Max runtime safety
    JsonObject maxRuntime = properties["max_runtime"].to<JsonObject>();
    maxRuntime["type"] = "integer";
    maxRuntime["minimum"] = 1000;
    maxRuntime["maximum"] = 300000;
    maxRuntime["default"] = 60000;
    maxRuntime["description"] = "Safety timeout in milliseconds";
    
    // Relay inversion
    JsonObject inverted = properties["relay_inverted"].to<JsonObject>();
    inverted["type"] = "boolean";
    inverted["default"] = true;
    inverted["description"] = "Inverted relay logic (LOW = active)";
    
    // Legacy compatibility fields
    JsonObject pumpPin = properties["pump_pin"].to<JsonObject>();
    pumpPin["type"] = "integer";
    pumpPin["default"] = 26;
    pumpPin["description"] = "Legacy: GPIO pin (use pin_no instead)";
    
    JsonObject flowRate = properties["ml_per_second"].to<JsonObject>();
    flowRate["type"] = "number";
    flowRate["default"] = 40.0;
    flowRate["description"] = "Legacy: Flow rate (use mls_per_sec instead)";
    
    log(Logger::DEBUG, String("Generated pump schema: pin=") + 26 + ", flow=40ml/s, liquid=Unknown");
    return schema;
}

bool PeristalticPumpComponent::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing peristaltic pump...");
    setState(ComponentState::INITIALIZING);
    
    // Load configuration (will use defaults if config is empty)
    if (!loadConfiguration(config)) {
        setError("Failed to load configuration");
        return false;
    }
    
    // Apply the configuration
    if (!applyConfiguration(getConfiguration())) {
        setError("Failed to apply configuration");
        return false;
    }
    
    // Initialize pump hardware
    if (!initializePump()) {
        setError("Failed to initialize pump hardware");
        return false;
    }
    
    setState(ComponentState::READY);
    log(Logger::INFO, String("Peristaltic pump initialized on pin ") + m_pumpPin + 
                      ", flow=" + m_mlPerSecond + "ml/s");
    
    return true;
}


JsonDocument PeristalticPumpComponent::getCurrentConfig() const {
    JsonDocument config;
    
    // New field names
    config["pin_no"] = m_pinNo;
    config["mls_per_sec"] = m_mlsPerSec;
    config["board_ref"] = m_boardRef;
    config["liquid_name"] = m_liquidName;
    config["liquid_concentration"] = m_liquidConcentration;
    config["max_runtime"] = m_maxRuntimeMs;
    config["relay_inverted"] = m_relayInverted;
    
    // Legacy compatibility fields
    config["pump_pin"] = m_pumpPin;
    config["ml_per_second"] = m_mlPerSecond;
    
    // Version tracking
    config["config_version"] = 2;
    
    return config;
}

bool PeristalticPumpComponent::applyConfig(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying pump configuration with default hydration");
    
    // Handle version migration
    uint16_t configVersion = config["config_version"] | 1;
    
    if (configVersion < 2) {
        // Migrate from version 1 to version 2
        log(Logger::INFO, "Migrating pump configuration from v1 to v2");
        
        // Map legacy fields to new fields
        m_pinNo = config["pump_pin"] | config["pin_no"] | 26;
        m_mlsPerSec = config["ml_per_second"] | config["mls_per_sec"] | 40.0f;
        
        // Legacy fields for compatibility
        m_pumpPin = m_pinNo;
        m_mlPerSecond = m_mlsPerSec;
    } else {
        // Version 2+ configuration
        m_pinNo = config["pin_no"] | 26;
        m_mlsPerSec = config["mls_per_sec"] | 40.0f;
        
        // Sync legacy fields
        m_pumpPin = m_pinNo;
        m_mlPerSecond = m_mlsPerSec;
    }
    
    // Apply common fields with defaults
    m_boardRef = config["board_ref"] | "BOARD_1";
    m_liquidName = config["liquid_name"] | "Unknown";
    m_liquidConcentration = config["liquid_concentration"] | 100.0f;
    m_maxRuntimeMs = config["max_runtime"] | 60000;
    m_relayInverted = config["relay_inverted"] | true;
    
    // Validate concentration range
    if (m_liquidConcentration < 0.0f) m_liquidConcentration = 0.0f;
    if (m_liquidConcentration > 100.0f) m_liquidConcentration = 100.0f;
    
    log(Logger::DEBUG, String("Config applied: pin=") + m_pinNo + 
                       ", flow=" + m_mlsPerSec + "ml/s" +
                       ", board=" + m_boardRef +
                       ", liquid=" + m_liquidName + " @" + m_liquidConcentration + "%" +
                       ", timeout=" + m_maxRuntimeMs + "ms" +
                       ", inverted=" + (m_relayInverted ? "true" : "false"));
    
    return true;
}

bool PeristalticPumpComponent::applyConfiguration(const JsonDocument& config) {
    // Legacy method - now delegates to new architecture
    return applyConfig(config);
}

bool PeristalticPumpComponent::initializePump() {
    log(Logger::DEBUG, String("Initializing pump on GPIO pin ") + m_pinNo);
    
    // Configure pump control pin (use new field)
    pinMode(m_pinNo, OUTPUT);
    
    // Ensure pump is off initially
    setPumpRelay(false);
    
    // Reset state machine fields
    m_dispenseMode = DispenseMode::IDLE;
    m_dispenseTarget = 0.0;
    m_currentVolume = 0.0;
    m_dispenseStartMs = 0;
    m_dispenseEndMs = 0;
    
    // Reset runtime state
    m_isPumping = false;
    m_pumpStartTime = 0;
    m_targetDurationMs = 0;
    m_currentDoseVolume = 0;
    m_continuousMode = false;
    
    // Sync legacy fields
    m_pumpPin = m_pinNo;
    m_mlPerSecond = m_mlsPerSec;
    
    log(Logger::INFO, String("Pump ready - GPIO") + m_pinNo + 
                      " configured for " + m_liquidName + " @" + m_liquidConcentration + "%" +
                      ", board=" + m_boardRef +
                      ", relay " + (m_relayInverted ? "inverted" : "normal"));
    
    return true;
}

ExecutionResult PeristalticPumpComponent::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    
    setState(ComponentState::EXECUTING);
    
    // Update pump state (check timeouts, stop if needed)
    updatePumpState();
    
    // Prepare output data
    JsonDocument data;
    data["timestamp"] = millis();
    data["pin"] = m_pinNo;
    data["is_pumping"] = m_isPumping;
    data["continuous_mode"] = m_continuousMode;
    data["current_dose_ml"] = m_currentDoseVolume;
    data["total_volume_ml"] = m_totalVolumePumped;
    data["total_runtime_ms"] = m_totalPumpTimeMs;
    data["dose_count"] = m_doseCount;
    data["flow_rate_ml_s"] = m_mlsPerSec;
    
    // State machine output fields
    data["dispense_mode"] = static_cast<int>(m_dispenseMode);
    data["dispense_target"] = m_dispenseTarget;
    data["current_volume"] = m_currentVolume;
    data["dispense_start_ms"] = m_dispenseStartMs;
    data["dispense_end_ms"] = m_dispenseEndMs;
    
    // Liquid properties
    data["liquid_name"] = m_liquidName;
    data["liquid_concentration"] = m_liquidConcentration;
    data["board_ref"] = m_boardRef;
    
    data["success"] = true;
    
    if (m_isPumping) {
        uint32_t elapsed = millis() - m_pumpStartTime;
        float volumePumped = (elapsed / 1000.0) * m_mlsPerSec;
        data["current_volume_ml"] = volumePumped;
        data["elapsed_ms"] = elapsed;
        
        if (m_targetDurationMs > 0) {
            float progress = (float)elapsed / m_targetDurationMs;
            data["dose_progress"] = (progress > 1.0) ? 1.0 : progress;
        }
    } else {
        data["current_volume_ml"] = 0;
        data["elapsed_ms"] = 0;
        data["dose_progress"] = 0;
    }
    
    // Update execution interval based on pump state
    if (m_isPumping) {
        setNextExecutionMs(millis() + 1000);  // Check every second when pumping
    } else {
        setNextExecutionMs(millis() + 30000); // Check every 30 seconds when idle
    }
    
    result.success = true;
    result.data = data;
    result.executionTimeMs = millis() - startTime;
    
    setState(ComponentState::READY);
    return result;
}

void PeristalticPumpComponent::updatePumpState() {
    if (!m_isPumping) return;
    
    uint32_t currentTime = millis();
    uint32_t elapsed = currentTime - m_pumpStartTime;
    
    // Update current volume in real-time
    m_currentVolume = (elapsed / 1000.0) * m_mlsPerSec;
    
    // Check for safety timeout
    if (elapsed >= m_maxRuntimeMs) {
        log(Logger::WARNING, "Pump safety timeout reached - stopping");
        // Update state machine before stopping
        m_dispenseMode = DispenseMode::IDLE;
        m_dispenseEndMs = currentTime;
        stopPump();
        return;
    }
    
    // Check if dose is complete (DOSE mode)
    if (m_dispenseMode == DispenseMode::DOSE && m_targetDurationMs > 0 && elapsed >= m_targetDurationMs) {
        // Final volume calculation
        m_currentVolume = (elapsed / 1000.0) * m_mlsPerSec;
        m_totalVolumePumped += m_currentVolume;
        m_doseCount++;
        
        // Update state machine
        m_dispenseMode = DispenseMode::IDLE;
        m_dispenseEndMs = currentTime;
        
        log(Logger::INFO, String("Dose complete: ") + m_currentVolume + "ml of " + m_liquidName + " dispensed");
        stopPump();
    }
}

bool PeristalticPumpComponent::dose(float volume_ml, float flow_rate) {
    if (m_isPumping) {
        log(Logger::WARNING, "Cannot dose - pump already running");
        return false;
    }
    
    if (volume_ml <= 0) {
        log(Logger::ERROR, "Invalid dose volume");
        return false;
    }
    
    float rate = (flow_rate > 0) ? flow_rate : m_mlsPerSec;
    m_targetDurationMs = calculatePumpTime(volume_ml, rate);
    
    if (m_targetDurationMs > m_maxRuntimeMs) {
        log(Logger::ERROR, "Dose duration exceeds max runtime safety limit");
        return false;
    }
    
    // Set state machine fields for DOSE mode
    m_dispenseMode = DispenseMode::DOSE;
    m_dispenseTarget = volume_ml;
    m_currentVolume = 0.0;
    m_dispenseStartMs = millis();
    m_dispenseEndMs = m_dispenseStartMs + m_targetDurationMs;
    
    // Legacy fields for backward compatibility
    m_currentDoseVolume = volume_ml;
    m_continuousMode = false;
    
    startPump();
    
    log(Logger::INFO, String("Dosing ") + volume_ml + "ml of " + m_liquidName + " at " + rate + "ml/s (" + m_targetDurationMs + "ms)");
    return true;
}

bool PeristalticPumpComponent::startContinuous() {
    if (m_isPumping) {
        log(Logger::WARNING, "Pump already running");
        return false;
    }
    
    // Set state machine fields for CONTINUOUS mode
    m_dispenseMode = DispenseMode::CONTINUOUS;
    m_dispenseTarget = 0.0;  // No target for continuous
    m_currentVolume = 0.0;
    m_dispenseStartMs = millis();
    m_dispenseEndMs = 0;     // No end time for continuous
    
    // Legacy fields for backward compatibility
    m_continuousMode = true;
    m_targetDurationMs = 0;  // No target duration
    m_currentDoseVolume = 0;
    
    startPump();
    
    log(Logger::INFO, "Starting continuous pumping of " + m_liquidName);
    return true;
}

bool PeristalticPumpComponent::stop() {
    if (!m_isPumping) {
        log(Logger::DEBUG, "Pump already stopped");
        return true;
    }
    
    // Calculate final volume and update state machine
    uint32_t elapsed = millis() - m_pumpStartTime;
    float actualVolume = (elapsed / 1000.0) * m_mlsPerSec;
    
    // Update current volume in state machine
    m_currentVolume = actualVolume;
    
    // Update total statistics
    m_totalVolumePumped += actualVolume;
    if (m_dispenseMode == DispenseMode::DOSE) {
        m_doseCount++;
    }
    
    // Reset state machine to IDLE
    m_dispenseMode = DispenseMode::IDLE;
    m_dispenseTarget = 0.0;
    m_dispenseEndMs = millis();  // Record actual end time
    
    // Reset legacy fields
    m_continuousMode = false;
    m_currentDoseVolume = 0;
    
    stopPump();
    log(Logger::INFO, "Pump stopped - dispensed " + String(actualVolume, 2) + "ml of " + m_liquidName);
    return true;
}

void PeristalticPumpComponent::startPump() {
    setPumpRelay(true);
    m_isPumping = true;
    m_pumpStartTime = millis();
    
    log(Logger::DEBUG, "Pump started");
}

void PeristalticPumpComponent::stopPump() {
    setPumpRelay(false);
    m_isPumping = false;
    m_continuousMode = false;
    
    // Update runtime statistics
    if (m_pumpStartTime > 0) {
        m_totalPumpTimeMs += millis() - m_pumpStartTime;
    }
    
    m_pumpStartTime = 0;
    m_targetDurationMs = 0;
    m_currentDoseVolume = 0;
    
    log(Logger::DEBUG, "Pump stopped");
}

void PeristalticPumpComponent::setPumpRelay(bool active) {
    bool pinState = m_relayInverted ? !active : active;
    digitalWrite(m_pumpPin, pinState ? HIGH : LOW);
    
    log(Logger::DEBUG, String("Relay ") + (active ? "ON" : "OFF") + 
                       " (GPIO" + m_pumpPin + "=" + (pinState ? "HIGH" : "LOW") + ")");
}

uint32_t PeristalticPumpComponent::calculatePumpTime(float volume_ml, float flow_rate) {
    if (flow_rate <= 0) return 0;
    return (uint32_t)((volume_ml / flow_rate) * 1000.0);  // Convert to milliseconds
}

void PeristalticPumpComponent::cleanup() {
    log(Logger::DEBUG, "Cleaning up peristaltic pump");
    
    // Ensure pump is stopped
    if (m_isPumping) {
        stopPump();
    }
    
    // Reset pin to input to avoid any potential issues
    if (m_pumpPin != 255) {
        pinMode(m_pumpPin, INPUT);
    }
}

// === Action System Implementation ===

std::vector<ComponentAction> PeristalticPumpComponent::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    
    // Dose Action
    ComponentAction doseAction;
    doseAction.name = "dose";
    doseAction.description = "Dispense a specific volume of liquid";
    doseAction.timeoutMs = 300000;  // 5 minutes max
    doseAction.requiresReady = true;
    
    ActionParameter volumeParam;
    volumeParam.name = "volume_ml";
    volumeParam.type = ActionParameterType::FLOAT;
    volumeParam.required = true;
    volumeParam.minValue = 0.1;
    volumeParam.maxValue = 1000.0;
    volumeParam.description = "Volume to dispense in milliliters";
    doseAction.parameters.push_back(volumeParam);
    
    ActionParameter flowRateParam;
    flowRateParam.name = "flow_rate";
    flowRateParam.type = ActionParameterType::FLOAT;
    flowRateParam.required = false;
    flowRateParam.minValue = 0.1;
    flowRateParam.maxValue = 100.0;
    // flowRateParam.defaultValue = 0;  // Use component default
    flowRateParam.description = "Optional flow rate override (ml/s)";
    doseAction.parameters.push_back(flowRateParam);
    
    ActionParameter timeoutParam;
    timeoutParam.name = "timeout_s";
    timeoutParam.type = ActionParameterType::INTEGER;
    timeoutParam.required = false;
    timeoutParam.minValue = 1;
    timeoutParam.maxValue = 300;
    // timeoutParam.defaultValue = 60;
    timeoutParam.description = "Maximum time to wait for completion (seconds)";
    doseAction.parameters.push_back(timeoutParam);
    
    actions.push_back(doseAction);
    
    // Start Continuous Action
    ComponentAction continuousAction;
    continuousAction.name = "start_continuous";
    continuousAction.description = "Start continuous pumping until stopped";
    continuousAction.timeoutMs = 5000;  // Quick start operation
    continuousAction.requiresReady = true;
    actions.push_back(continuousAction);
    
    // Stop Action
    ComponentAction stopAction;
    stopAction.name = "stop";
    stopAction.description = "Stop pump operation immediately";
    stopAction.timeoutMs = 5000;
    stopAction.requiresReady = false;  // Can stop even if not ready
    actions.push_back(stopAction);
    
    // Get Status Action
    ComponentAction statusAction;
    statusAction.name = "get_status";
    statusAction.description = "Get current pump status and statistics";
    statusAction.timeoutMs = 1000;
    statusAction.requiresReady = false;  // Can check status anytime
    actions.push_back(statusAction);
    
    return actions;
}

ActionResult PeristalticPumpComponent::performAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.actionName = actionName;
    result.success = false;
    
    log(Logger::INFO, String("Performing pump action: ") + actionName);
    
    if (actionName == "dose") {
        float volume = parameters["volume_ml"].as<float>();
        float flowRate = parameters["flow_rate"] | 0.0f;  // Use default if not provided
        uint32_t timeoutS = parameters["timeout_s"] | 60;
        
        log(Logger::INFO, String("Starting dose: ") + volume + "ml" +
                         (flowRate > 0 ? String(", flow: ") + flowRate + "ml/s" : "") +
                         ", timeout: " + timeoutS + "s");
        
        bool started = dose(volume, flowRate);
        if (started) {
            // Wait for dose completion or timeout
            uint32_t startTime = millis();
            uint32_t timeoutMs = timeoutS * 1000;
            
            while (m_isPumping && (millis() - startTime) < timeoutMs) {
                delay(100);  // Check every 100ms
                updatePumpState();  // Update internal state
            }
            
            if (m_isPumping) {
                // Timeout occurred
                stop();
                result.success = false;
                result.message = "Dose operation timed out";
                log(Logger::ERROR, "Dose timeout - stopping pump");
            } else {
                // Completed successfully
                result.success = true;
                result.message = String("Successfully dispensed ") + volume + "ml";
                result.data["volume_dispensed"] = volume;
                result.data["actual_duration_ms"] = millis() - startTime;
                log(Logger::INFO, result.message);
            }
        } else {
            result.success = false;
            result.message = "Failed to start dose operation";
            log(Logger::ERROR, result.message);
        }
        
    } else if (actionName == "start_continuous") {
        bool started = startContinuous();
        result.success = started;
        result.message = started ? "Continuous pumping started" : "Failed to start continuous pumping";
        if (started) {
            result.data["mode"] = "continuous";
        }
        log(Logger::INFO, result.message);
        
    } else if (actionName == "stop") {
        bool stopped = stop();
        result.success = stopped;
        result.message = stopped ? "Pump stopped successfully" : "Failed to stop pump";
        result.data["was_pumping"] = !stopped;  // If stop failed, it was still pumping
        log(Logger::INFO, result.message);
        
    } else if (actionName == "get_status") {
        result.success = true;
        result.message = "Status retrieved successfully";
        
        result.data["is_pumping"] = m_isPumping;
        result.data["continuous_mode"] = m_continuousMode;
        result.data["current_dose_ml"] = m_currentDoseVolume;
        result.data["total_volume_ml"] = m_totalVolumePumped;
        result.data["total_runtime_ms"] = m_totalPumpTimeMs;
        result.data["dose_count"] = m_doseCount;
        result.data["flow_rate_ml_s"] = m_mlPerSecond;
        result.data["pin"] = m_pumpPin;
        result.data["relay_inverted"] = m_relayInverted;
        
        if (m_isPumping) {
            uint32_t elapsed = millis() - m_pumpStartTime;
            float currentVolume = (elapsed / 1000.0) * m_mlPerSecond;
            result.data["elapsed_ms"] = elapsed;
            result.data["current_volume_ml"] = currentVolume;
        }
        
        log(Logger::DEBUG, "Status retrieved successfully");
        
    } else {
        result.success = false;
        result.message = "Unknown action: " + actionName;
        log(Logger::ERROR, result.message);
    }
    
    return result;
}