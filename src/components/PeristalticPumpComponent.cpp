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
    schema["required"].add("pump_pin");
    
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // Pump control pin (required)
    JsonObject pumpPin = properties["pump_pin"].to<JsonObject>();
    pumpPin["type"] = "integer";
    pumpPin["minimum"] = 0;
    pumpPin["maximum"] = 39;
    pumpPin["default"] = 26;
    pumpPin["description"] = "GPIO pin for pump control relay";
    
    // Flow rate (optional)
    JsonObject flowRate = properties["ml_per_second"].to<JsonObject>();
    flowRate["type"] = "number";
    flowRate["minimum"] = 0.1;
    flowRate["maximum"] = 100.0;
    flowRate["default"] = 40.0;
    flowRate["description"] = "Pump flow rate in ml/s";
    
    // Max runtime safety (optional)
    JsonObject maxRuntime = properties["max_runtime"].to<JsonObject>();
    maxRuntime["type"] = "integer";
    maxRuntime["minimum"] = 1000;
    maxRuntime["maximum"] = 300000;
    maxRuntime["default"] = 60000;
    maxRuntime["description"] = "Safety timeout in milliseconds";
    
    // Relay inversion (optional)
    JsonObject inverted = properties["relay_inverted"].to<JsonObject>();
    inverted["type"] = "boolean";
    inverted["default"] = true;
    inverted["description"] = "Inverted relay logic (LOW = active)";
    
    log(Logger::DEBUG, "Generated pump schema with pin=26, flow=40ml/s, inverted=true");
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

bool PeristalticPumpComponent::applyConfiguration(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying pump configuration");
    
    // Extract pump pin
    if (config["pump_pin"].is<uint8_t>()) {
        m_pumpPin = config["pump_pin"].as<uint8_t>();
    }
    
    // Extract flow rate
    if (config["ml_per_second"].is<float>()) {
        m_mlPerSecond = config["ml_per_second"].as<float>();
    }
    
    // Extract max runtime
    if (config["max_runtime"].is<uint32_t>()) {
        m_maxRuntimeMs = config["max_runtime"].as<uint32_t>();
    }
    
    // Extract relay inversion
    if (config["relay_inverted"].is<bool>()) {
        m_relayInverted = config["relay_inverted"].as<bool>();
    }
    
    log(Logger::DEBUG, String("Config applied: pin=") + m_pumpPin + 
                       ", flow=" + m_mlPerSecond + "ml/s" +
                       ", timeout=" + m_maxRuntimeMs + "ms" +
                       ", inverted=" + (m_relayInverted ? "true" : "false"));
    
    return true;
}

bool PeristalticPumpComponent::initializePump() {
    log(Logger::DEBUG, String("Initializing pump on GPIO pin ") + m_pumpPin);
    
    // Configure pump control pin
    pinMode(m_pumpPin, OUTPUT);
    
    // Ensure pump is off initially
    setPumpRelay(false);
    
    // Reset state variables
    m_isPumping = false;
    m_pumpStartTime = 0;
    m_targetDurationMs = 0;
    m_currentDoseVolume = 0;
    m_continuousMode = false;
    
    log(Logger::INFO, String("Pump ready - GPIO") + m_pumpPin + 
                      " configured, relay logic " + (m_relayInverted ? "inverted" : "normal"));
    
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
    data["pin"] = m_pumpPin;
    data["is_pumping"] = m_isPumping;
    data["continuous_mode"] = m_continuousMode;
    data["current_dose_ml"] = m_currentDoseVolume;
    data["total_volume_ml"] = m_totalVolumePumped;
    data["total_runtime_ms"] = m_totalPumpTimeMs;
    data["dose_count"] = m_doseCount;
    data["flow_rate_ml_s"] = m_mlPerSecond;
    data["success"] = true;
    
    if (m_isPumping) {
        uint32_t elapsed = millis() - m_pumpStartTime;
        float volumePumped = (elapsed / 1000.0) * m_mlPerSecond;
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
    
    // Check for safety timeout
    if (elapsed >= m_maxRuntimeMs) {
        log(Logger::WARNING, "Pump safety timeout reached - stopping");
        stopPump();
        return;
    }
    
    // Check if dose is complete (non-continuous mode)
    if (!m_continuousMode && m_targetDurationMs > 0 && elapsed >= m_targetDurationMs) {
        float actualVolume = (elapsed / 1000.0) * m_mlPerSecond;
        m_totalVolumePumped += actualVolume;
        m_doseCount++;
        
        log(Logger::INFO, String("Dose complete: ") + actualVolume + "ml dispensed");
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
    
    float rate = (flow_rate > 0) ? flow_rate : m_mlPerSecond;
    m_targetDurationMs = calculatePumpTime(volume_ml, rate);
    
    if (m_targetDurationMs > m_maxRuntimeMs) {
        log(Logger::ERROR, "Dose duration exceeds max runtime safety limit");
        return false;
    }
    
    m_currentDoseVolume = volume_ml;
    m_continuousMode = false;
    startPump();
    
    log(Logger::INFO, String("Dosing ") + volume_ml + "ml at " + rate + "ml/s (" + m_targetDurationMs + "ms)");
    return true;
}

bool PeristalticPumpComponent::startContinuous() {
    if (m_isPumping) {
        log(Logger::WARNING, "Pump already running");
        return false;
    }
    
    m_continuousMode = true;
    m_targetDurationMs = 0;  // No target duration
    m_currentDoseVolume = 0;
    startPump();
    
    log(Logger::INFO, "Starting continuous pumping");
    return true;
}

bool PeristalticPumpComponent::stop() {
    if (!m_isPumping) {
        log(Logger::DEBUG, "Pump already stopped");
        return true;
    }
    
    // Calculate final volume if we were dosing
    if (m_currentDoseVolume > 0) {
        uint32_t elapsed = millis() - m_pumpStartTime;
        float actualVolume = (elapsed / 1000.0) * m_mlPerSecond;
        m_totalVolumePumped += actualVolume;
        
        if (!m_continuousMode) {
            m_doseCount++;
        }
    }
    
    stopPump();
    log(Logger::INFO, "Pump stopped by request");
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