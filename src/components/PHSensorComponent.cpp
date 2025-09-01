#include "PHSensorComponent.h"
#include "../utils/Logger.h"
#include "../core/Orchestrator.h"
#include <Arduino.h>

PHSensorComponent::PHSensorComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "PHSensor", name, storage, orchestrator) {
    
    // Initialize calibration points
    m_calibrationPoints[0].voltage = 0.0f;
    m_calibrationPoints[0].ph = 4.0f;
    m_calibrationPoints[0].isValid = false;
    m_calibrationPoints[0].timestampMs = 0;
    
    m_calibrationPoints[1].voltage = 0.0f;
    m_calibrationPoints[1].ph = 7.0f;
    m_calibrationPoints[1].isValid = false;
    m_calibrationPoints[1].timestampMs = 0;
    
    m_calibrationPoints[2].voltage = 0.0f;
    m_calibrationPoints[2].ph = 10.0f;
    m_calibrationPoints[2].isValid = false;
    m_calibrationPoints[2].timestampMs = 0;
    
    // Reserve space for readings array
    m_lastReads.reserve(m_sampleSize);
}

PHSensorComponent::~PHSensorComponent() {
    cleanup();
}

JsonDocument PHSensorComponent::getDefaultSchema() const {
    JsonDocument schema;
    
    // Schema metadata
    schema["version"] = 1;
    schema["type"] = "PHSensor";
    schema["description"] = "pH sensor with 3-point calibration and temperature compensation";
    
    // Configuration parameters
    JsonObject config = schema["config"].to<JsonObject>();
    config["gpio_pin"] = 36;
    config["temp_coefficient"] = -0.0198f;
    config["sample_size"] = 10;
    config["adc_voltage_ref"] = 3.3f;
    config["adc_resolution"] = 4096;
    config["reading_interval_ms"] = 1000;
    config["time_period_for_sampling"] = 10000;
    config["outlier_threshold"] = 2.0f;
    config["temperature_source_id"] = "";
    config["excite_voltage_component_id"] = "";
    config["excite_stabilize_ms"] = 500;
    
    // Calibration points
    JsonArray calibration = config["calibration_points"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        JsonObject point = calibration.add<JsonObject>();
        point["ph"] = (i == 0) ? 4.0f : (i == 1) ? 7.0f : 10.0f;
        point["voltage"] = 0.0f;
        point["valid"] = false;
        point["timestamp_ms"] = 0;
    }
    
    return schema;
}

bool PHSensorComponent::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing pH Sensor Component: " + m_componentName);
    
    if (!applyConfig(config)) {
        log(Logger::ERROR, "Failed to apply pH sensor configuration");
        return false;
    }
    
    if (!initializeSensor()) {
        log(Logger::ERROR, "Failed to initialize pH sensor hardware");
        return false;
    }
    
    logCalibrationStatus();
    
    // Set initial execution time to start sampling cycle
    setNextExecutionMs(millis() + m_readingIntervalMs);
    
    setState(ComponentState::READY);
    log(Logger::INFO, "pH Sensor initialized successfully on GPIO " + String(m_gpioPin));
    return true;
}

ExecutionResult PHSensorComponent::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    uint32_t currentTime = millis();
    
    setState(ComponentState::EXECUTING);
    
    // Update sensor mode based on current state
    updateSensorMode();
    
    // Initialize sampling window if not active
    if (!m_samplingActive) {
        startSamplingWindow();
    }
    
    // Take readings during the sampling window
    if (m_samplingActive && !isSamplingWindowComplete()) {
        // Ensure excitation voltage is stabilized before taking readings
        if (isExcitationStabilized()) {
            // Check if it's time for a new reading
            if (currentTime - m_lastReadingMs >= m_readingIntervalMs) {
                // Take raw voltage reading
                float rawVoltage = readRawVoltage();
                
                if (rawVoltage >= 0) {
                    // Add to sample buffer
                    addReading(rawVoltage);
                    m_totalReadings++;
                    m_lastReadingMs = currentTime;
                    
                    log(Logger::DEBUG, "pH Sample " + String(m_lastReads.size()) + "/" + String(m_sampleSize) + 
                        ": " + String(rawVoltage, 4) + "V");
                } else {
                    m_errorCount++;
                    log(Logger::WARNING, "Failed to read pH sensor voltage");
                }
            }
        } else {
            log(Logger::DEBUG, "Waiting for excitation voltage to stabilize...");
        }
    }
    
    // Process samples when window is complete
    if (m_samplingActive && isSamplingWindowComplete()) {
        endSamplingWindow();
        
        // Remove outliers from sample set
        removeOutliers();
        
        // Calculate final averaged voltage with outlier removal
        m_currentVolts = calculateWeightedAverageWithOutlierRemoval();
        
        // Get temperature for compensation
        m_currentTemp = getTemperatureReading();
        
        // Calculate pH with temperature compensation
        m_currentPH = convertVoltageToPH(m_currentVolts, m_currentTemp);
        
        // Update statistics
        if (m_currentPH >= 0) {
            m_minRecordedPH = min(m_minRecordedPH, m_currentPH);
            m_maxRecordedPH = max(m_maxRecordedPH, m_currentPH);
        }
        
        log(Logger::INFO, "Sampling window complete: " + String(m_lastReads.size()) + " samples, " + 
            String(m_outliersRemoved) + " outliers removed, pH = " + String(m_currentPH, 2));
    }
    
    // Prepare output data
    JsonDocument data;
    data["timestamp"] = currentTime;
    data["gpio_pin"] = m_gpioPin;
    
    // State output fields
    data["current_mode"] = getModeString(m_currentMode);
    data["current_mode_int"] = static_cast<int>(m_currentMode);
    data["current_volts"] = m_currentVolts;
    data["current_temp"] = m_currentTemp;
    data["current_ph"] = m_currentPH;
    data["sample_size"] = m_sampleSize;
    
    // Sampling window status
    data["sampling_active"] = m_samplingActive;
    data["sampling_start_ms"] = m_samplingStartMs;
    data["sampling_end_ms"] = m_samplingEndMs;
    data["time_period_for_sampling"] = m_timePeriodForSampling;
    
    // Calibration status
    data["is_calibrated"] = isCalibrated();
    data["calibration_points_valid"] = 0;
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) {
            data["calibration_points_valid"] = data["calibration_points_valid"].as<int>() + 1;
        }
    }
    
    // Statistics
    data["total_readings"] = m_totalReadings;
    data["error_count"] = m_errorCount;
    data["outliers_removed"] = m_outliersRemoved;
    data["min_recorded_ph"] = m_minRecordedPH;
    data["max_recorded_ph"] = m_maxRecordedPH;
    data["buffer_full"] = m_bufferFull;
    
    // Last readings array (for debugging)
    JsonArray readings = data["last_readings"].to<JsonArray>();
    for (float reading : m_lastReads) {
        readings.add(reading);
    }
    
    data["success"] = true;
    
    // Calculate next execution time based on sampling window state
    uint32_t nextExecutionTime = calculateNextExecutionTime();
    setNextExecutionMs(nextExecutionTime);
    
    // CRITICAL: Update execution statistics
    updateExecutionStats();
    
    // Store last execution data for API/dashboard access
    String dataStr;
    serializeJson(data, dataStr);
    storeExecutionDataString(dataStr);  // Store as string for API access
    
    result.success = true;
    result.data = data;
    result.executionTimeMs = millis() - startTime;
    
    setState(ComponentState::READY);
    return result;
}

void PHSensorComponent::cleanup() {
    log(Logger::INFO, "Cleaning up pH sensor component");
    m_lastReads.clear();
}

JsonDocument PHSensorComponent::getCurrentConfig() const {
    JsonDocument config;
    
    config["version"] = 1;
    config["gpio_pin"] = m_gpioPin;
    config["temp_coefficient"] = m_tempCoefficient;
    config["sample_size"] = m_sampleSize;
    config["adc_voltage_ref"] = m_adcVoltageRef;
    config["adc_resolution"] = m_adcResolution;
    config["reading_interval_ms"] = m_readingIntervalMs;
    config["time_period_for_sampling"] = m_timePeriodForSampling;
    config["outlier_threshold"] = m_outlierThreshold;
    config["temperature_source_id"] = m_temperatureSourceId;
    config["excite_voltage_component_id"] = m_exciteVoltageComponentId;
    config["excite_stabilize_ms"] = m_exciteStabilizeMs;
    
    // Include calibration data
    JsonArray calibration = config["calibration_points"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        JsonObject point = calibration.add<JsonObject>();
        point["ph"] = m_calibrationPoints[i].ph;
        point["voltage"] = m_calibrationPoints[i].voltage;
        point["valid"] = m_calibrationPoints[i].isValid;
        point["timestamp_ms"] = m_calibrationPoints[i].timestampMs;
    }
    
    return config;
}

bool PHSensorComponent::applyConfig(const JsonDocument& config) {
    // Apply basic configuration
    m_gpioPin = config["gpio_pin"] | m_gpioPin;
    m_tempCoefficient = config["temp_coefficient"] | m_tempCoefficient;
    m_sampleSize = config["sample_size"] | m_sampleSize;
    m_adcVoltageRef = config["adc_voltage_ref"] | m_adcVoltageRef;
    m_adcResolution = config["adc_resolution"] | m_adcResolution;
    m_readingIntervalMs = config["reading_interval_ms"] | m_readingIntervalMs;
    m_timePeriodForSampling = config["time_period_for_sampling"] | m_timePeriodForSampling;
    m_outlierThreshold = config["outlier_threshold"] | m_outlierThreshold;
    m_temperatureSourceId = config["temperature_source_id"] | m_temperatureSourceId;
    m_exciteVoltageComponentId = config["excite_voltage_component_id"] | m_exciteVoltageComponentId;
    m_exciteStabilizeMs = config["excite_stabilize_ms"] | m_exciteStabilizeMs;
    
    // Validate sample size
    if (m_sampleSize < 1) m_sampleSize = 1;
    if (m_sampleSize > 100) m_sampleSize = 100;
    
    // Resize readings buffer if needed
    if (m_lastReads.capacity() != m_sampleSize) {
        m_lastReads.clear();
        m_lastReads.reserve(m_sampleSize);
        m_readIndex = 0;
        m_bufferFull = false;
    }
    
    // Apply calibration data
    updateCalibrationFromConfig(config);
    
    return true;
}

std::vector<ComponentAction> PHSensorComponent::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    
    // Calibrate action - 3-point calibration
    ComponentAction calibrateAction;
    calibrateAction.name = "calibrate";
    calibrateAction.description = "Perform 3-point pH calibration";
    calibrateAction.timeoutMs = 30000;
    calibrateAction.requiresReady = true;
    
    ActionParameter ph4Param;
    ph4Param.name = "ph4_voltage";
    ph4Param.type = ActionParameterType::FLOAT;
    ph4Param.required = true;
    ph4Param.minValue = 0.0f;
    ph4Param.maxValue = 5.0f;
    calibrateAction.parameters.push_back(ph4Param);
    
    ActionParameter ph7Param;
    ph7Param.name = "ph7_voltage";
    ph7Param.type = ActionParameterType::FLOAT;
    ph7Param.required = true;
    ph7Param.minValue = 0.0f;
    ph7Param.maxValue = 5.0f;
    calibrateAction.parameters.push_back(ph7Param);
    
    ActionParameter ph10Param;
    ph10Param.name = "ph10_voltage";
    ph10Param.type = ActionParameterType::FLOAT;
    ph10Param.required = true;
    ph10Param.minValue = 0.0f;
    ph10Param.maxValue = 5.0f;
    calibrateAction.parameters.push_back(ph10Param);
    
    actions.push_back(calibrateAction);
    
    // Calibrate Point action - single point calibration
    ComponentAction calibratePointAction;
    calibratePointAction.name = "calibrate_point";
    calibratePointAction.description = "Calibrate single pH point";
    calibratePointAction.timeoutMs = 10000;
    calibratePointAction.requiresReady = true;
    
    ActionParameter phValueParam;
    phValueParam.name = "ph_value";
    phValueParam.type = ActionParameterType::FLOAT;
    phValueParam.required = true;
    phValueParam.minValue = 0.0f;
    phValueParam.maxValue = 14.0f;
    calibratePointAction.parameters.push_back(phValueParam);
    
    ActionParameter voltageParam;
    voltageParam.name = "voltage";
    voltageParam.type = ActionParameterType::FLOAT;
    voltageParam.required = true;
    voltageParam.minValue = 0.0f;
    voltageParam.maxValue = 5.0f;
    calibratePointAction.parameters.push_back(voltageParam);
    
    actions.push_back(calibratePointAction);
    
    // Clear Calibration action
    ComponentAction clearCalAction;
    clearCalAction.name = "clear_calibration";
    clearCalAction.description = "Clear all calibration data";
    clearCalAction.timeoutMs = 5000;
    clearCalAction.requiresReady = true;
    actions.push_back(clearCalAction);
    
    return actions;
}

ActionResult PHSensorComponent::performAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.success = false;
    
    if (actionName == "calibrate") {
        float ph4_voltage = parameters["ph4_voltage"];
        float ph7_voltage = parameters["ph7_voltage"];
        float ph10_voltage = parameters["ph10_voltage"];
        
        result.success = calibrate(ph4_voltage, ph7_voltage, ph10_voltage);
        result.message = result.success ? 
            "3-point calibration completed successfully" :
            "Calibration failed - check voltage values";
            
    } else if (actionName == "calibrate_point") {
        float ph_value = parameters["ph_value"];
        float voltage = parameters["voltage"];
        
        result.success = calibratePoint(ph_value, voltage);
        result.message = result.success ?
            "Calibration point updated for pH " + String(ph_value, 1) :
            "Failed to update calibration point";
            
    } else if (actionName == "clear_calibration") {
        result.success = clearCalibration();
        result.message = result.success ?
            "Calibration data cleared" :
            "Failed to clear calibration";
    } else {
        result.message = "Unknown action: " + actionName;
    }
    
    return result;
}

JsonDocument PHSensorComponent::getCoreData() const {
    JsonDocument coreData;
    
    // For pH sensors, core data is pH value, temperature, and success status
    if (!m_lastData.isNull() && m_lastData.size() > 0) {
        // Extract only the essential sensor values
        if (!m_lastData["current ph"].isNull()) {
            coreData["ph"] = m_lastData["current ph"];
        }
        if (!m_lastData["current temp"].isNull()) {
            coreData["temperature"] = m_lastData["current temp"];
        }
        if (!m_lastData["current volts"].isNull()) {
            coreData["voltage"] = m_lastData["current volts"];
        }
        if (!m_lastData["success"].isNull()) {
            coreData["success"] = m_lastData["success"];
        }
        if (!m_lastData["timestamp"].isNull()) {
            coreData["timestamp"] = m_lastData["timestamp"];
        }
        if (!m_lastData["is calibrated"].isNull()) {
            coreData["calibrated"] = m_lastData["is calibrated"];
        }
        
        log(Logger::DEBUG, String("[CORE-DATA] PHSensor ") + m_componentId + " returning " + 
            String(coreData.size()) + " core fields");
    } else {
        // No data available
        coreData["status"] = "No data available";
        log(Logger::DEBUG, String("[CORE-DATA] PHSensor ") + m_componentId + " has no data");
    }
    
    return coreData;
}

bool PHSensorComponent::calibrate(float ph4_voltage, float ph7_voltage, float ph10_voltage) {
    log(Logger::INFO, "Performing 3-point pH calibration");
    
    uint32_t timestamp = millis();
    
    // Set calibration points
    m_calibrationPoints[0].voltage = ph4_voltage;
    m_calibrationPoints[0].ph = 4.0f;
    m_calibrationPoints[0].isValid = true;
    m_calibrationPoints[0].timestampMs = timestamp;
    
    m_calibrationPoints[1].voltage = ph7_voltage;
    m_calibrationPoints[1].ph = 7.0f;
    m_calibrationPoints[1].isValid = true;
    m_calibrationPoints[1].timestampMs = timestamp;
    
    m_calibrationPoints[2].voltage = ph10_voltage;
    m_calibrationPoints[2].ph = 10.0f;
    m_calibrationPoints[2].isValid = true;
    m_calibrationPoints[2].timestampMs = timestamp;
    
    m_lastCalibrationMs = timestamp;
    m_calibrationValid = validateCalibrationPoints();
    
    if (m_calibrationValid) {
        log(Logger::INFO, "3-point calibration successful");
        logCalibrationStatus();
        
        // Save configuration
        saveConfigurationToStorage(getCurrentConfig());
        return true;
    } else {
        log(Logger::ERROR, "Calibration validation failed");
        return false;
    }
}

bool PHSensorComponent::calibratePoint(float ph_value, float voltage) {
    // Find the closest calibration point
    int pointIndex = -1;
    float minDiff = 15.0f;  // Max pH difference
    
    for (int i = 0; i < 3; i++) {
        float diff = abs(ph_value - m_calibrationPoints[i].ph);
        if (diff < minDiff) {
            minDiff = diff;
            pointIndex = i;
        }
    }
    
    if (pointIndex >= 0 && minDiff < 1.0f) {  // Within 1 pH unit
        m_calibrationPoints[pointIndex].voltage = voltage;
        m_calibrationPoints[pointIndex].isValid = true;
        m_calibrationPoints[pointIndex].timestampMs = millis();
        
        m_calibrationValid = validateCalibrationPoints();
        
        log(Logger::INFO, "Updated calibration point: pH " + String(ph_value, 1) + " = " + String(voltage, 3) + "V");
        
        // Save configuration
        saveConfigurationToStorage(getCurrentConfig());
        return true;
    }
    
    log(Logger::ERROR, "No suitable calibration point found for pH " + String(ph_value, 1));
    return false;
}

bool PHSensorComponent::clearCalibration() {
    log(Logger::INFO, "Clearing pH calibration data");
    
    for (int i = 0; i < 3; i++) {
        m_calibrationPoints[i].voltage = 0.0f;
        m_calibrationPoints[i].isValid = false;
        m_calibrationPoints[i].timestampMs = 0;
    }
    
    m_calibrationValid = false;
    m_lastCalibrationMs = 0;
    
    // Save configuration
    saveConfigurationToStorage(getCurrentConfig());
    return true;
}

float PHSensorComponent::readRawVoltage() {
    if (m_gpioPin == 0) {
        // Mock mode - return simulated pH voltage (around pH 7.0)
        static uint32_t mockCounter = 0;
        mockCounter++;
        
        // Simulate pH 7.0 reading with some noise
        float baseVoltage = 1.65f;  // Typical neutral pH voltage
        float noise = (sin(mockCounter * 0.1f) * 0.05f) + ((mockCounter % 7) * 0.01f);
        return baseVoltage + noise;
    }
    
    int adcValue = analogRead(m_gpioPin);
    if (adcValue < 0) return -1.0f;
    
    // Convert ADC value to voltage
    float voltage = (float)adcValue * m_adcVoltageRef / m_adcResolution;
    return voltage;
}

float PHSensorComponent::getCurrentPH(float temperature_c) {
    float voltage = calculateWeightedAverage();
    return convertVoltageToPH(voltage, temperature_c);
}

bool PHSensorComponent::isCalibrated() const {
    int validPoints = 0;
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) validPoints++;
    }
    return validPoints >= 2;  // Need at least 2 points for linear interpolation
}

bool PHSensorComponent::initializeSensor() {
    // Set ADC attenuation for ESP32 (0-3.3V range)
    analogSetAttenuation(ADC_11db);
    
    // Initialize readings buffer
    m_lastReads.clear();
    m_readIndex = 0;
    m_bufferFull = false;
    
    return true;
}

float PHSensorComponent::calculateWeightedAverage() const {
    if (m_lastReads.empty()) return 0.0f;
    
    float sum = 0.0f;
    int count = m_bufferFull ? m_sampleSize : m_lastReads.size();
    
    for (int i = 0; i < count; i++) {
        sum += m_lastReads[i];
    }
    
    return sum / count;
}

void PHSensorComponent::addReading(float voltage) {
    if (m_lastReads.size() < m_sampleSize) {
        m_lastReads.push_back(voltage);
    } else {
        m_lastReads[m_readIndex] = voltage;
        m_readIndex = (m_readIndex + 1) % m_sampleSize;
        if (!m_bufferFull && m_readIndex == 0) {
            m_bufferFull = true;
        }
    }
}

float PHSensorComponent::convertVoltageToPH(float voltage, float temperature_c) const {
    // In mock mode, provide simulated pH values without calibration
    if (m_currentMode == PHSensorMode::MOCK) {
        // Simulate pH 7.0 ± variations based on voltage
        // Mock voltage is around 1.65V for neutral pH
        float basePH = 7.0f;
        float voltageDeviation = voltage - 1.65f;
        float ph = basePH - (voltageDeviation * 10.0f); // ~10 pH units per volt
        
        // Apply temperature compensation
        float tempCompensation = m_tempCoefficient * (temperature_c - 25.0f);
        ph += tempCompensation;
        
        // Clamp to valid pH range
        if (ph < 0.0f) ph = 0.0f;
        if (ph > 14.0f) ph = 14.0f;
        
        return ph;
    }
    
    if (!isCalibrated()) return -1.0f;
    
    // Basic linear interpolation between calibration points
    float ph = interpolatePH(voltage);
    
    if (ph < 0) return -1.0f;
    
    // Apply temperature compensation
    // pH change = temp_coefficient * (current_temp - 25°C)
    float tempCompensation = m_tempCoefficient * (temperature_c - 25.0f);
    ph += tempCompensation;
    
    // Clamp to valid pH range
    if (ph < 0.0f) ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    
    return ph;
}

float PHSensorComponent::getTemperatureReading() {
    if (m_temperatureSourceId.length() > 0 && m_orchestrator) {
        BaseComponent* tempComponent = getTemperatureComponent();
        if (tempComponent) {
            // Try to get temperature from component
            JsonDocument tempData;
            // This would need to be implemented based on your temperature component's interface
            // For now, return default
        }
    }
    
    return 25.0f;  // Default temperature
}

bool PHSensorComponent::updateCalibrationFromConfig(const JsonDocument& config) {
    if (config["calibration_points"].isNull()) return true;
    
    JsonArrayConst calibration = config["calibration_points"];
    for (size_t i = 0; i < calibration.size() && i < 3; i++) {
        JsonObjectConst point = calibration[i];
        m_calibrationPoints[i].ph = point["ph"] | m_calibrationPoints[i].ph;
        m_calibrationPoints[i].voltage = point["voltage"] | m_calibrationPoints[i].voltage;
        m_calibrationPoints[i].isValid = point["valid"] | m_calibrationPoints[i].isValid;
        m_calibrationPoints[i].timestampMs = point["timestamp_ms"] | m_calibrationPoints[i].timestampMs;
    }
    
    m_calibrationValid = validateCalibrationPoints();
    return true;
}

void PHSensorComponent::logCalibrationStatus() {
    int validPoints = 0;
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) {
            validPoints++;
            log(Logger::INFO, "Calibration Point " + String(i+1) + ": pH " + 
                String(m_calibrationPoints[i].ph, 1) + " = " + 
                String(m_calibrationPoints[i].voltage, 3) + "V");
        }
    }
    
    log(Logger::INFO, "pH sensor calibration: " + String(validPoints) + "/3 points valid, " +
        (isCalibrated() ? "READY" : "NEEDS CALIBRATION"));
}

bool PHSensorComponent::validateCalibrationPoints() const {
    int validPoints = 0;
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) validPoints++;
    }
    return validPoints >= 2;
}

float PHSensorComponent::interpolatePH(float voltage) const {
    // Find two valid calibration points for interpolation
    std::vector<std::pair<float, float>> validPoints;  // voltage, pH pairs
    
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) {
            validPoints.push_back({m_calibrationPoints[i].voltage, m_calibrationPoints[i].ph});
        }
    }
    
    if (validPoints.size() < 2) return -1.0f;
    
    // Sort by voltage
    std::sort(validPoints.begin(), validPoints.end());
    
    // Simple linear interpolation between closest points
    if (voltage <= validPoints[0].first) {
        // Extrapolate below lowest point
        if (validPoints.size() >= 2) {
            float slope = (validPoints[1].second - validPoints[0].second) / 
                         (validPoints[1].first - validPoints[0].first);
            return validPoints[0].second + slope * (voltage - validPoints[0].first);
        }
        return validPoints[0].second;
    }
    
    if (voltage >= validPoints.back().first) {
        // Extrapolate above highest point
        if (validPoints.size() >= 2) {
            size_t n = validPoints.size();
            float slope = (validPoints[n-1].second - validPoints[n-2].second) / 
                         (validPoints[n-1].first - validPoints[n-2].first);
            return validPoints[n-1].second + slope * (voltage - validPoints[n-1].first);
        }
        return validPoints.back().second;
    }
    
    // Interpolate between points
    for (size_t i = 0; i < validPoints.size() - 1; i++) {
        if (voltage >= validPoints[i].first && voltage <= validPoints[i+1].first) {
            float slope = (validPoints[i+1].second - validPoints[i].second) / 
                         (validPoints[i+1].first - validPoints[i].first);
            return validPoints[i].second + slope * (voltage - validPoints[i].first);
        }
    }
    
    return -1.0f;
}

BaseComponent* PHSensorComponent::getTemperatureComponent() {
    if (m_orchestrator && m_temperatureSourceId.length() > 0) {
        // Implementation would depend on your orchestrator's component lookup method
        // return m_orchestrator->getComponent(m_temperatureSourceId);
    }
    return nullptr;
}

bool PHSensorComponent::controlExcitationVoltage(bool enable) {
    if (m_exciteVoltageComponentId.length() == 0) {
        return true; // No excitation component configured
    }
    
    BaseComponent* exciteComponent = getExciteVoltageComponent();
    if (!exciteComponent) {
        log(Logger::WARNING, "Excitation voltage component not found: " + m_exciteVoltageComponentId);
        return false;
    }
    
    // Create action parameters
    JsonDocument params;
    params["state"] = enable;
    
    // Execute the action to control excitation voltage
    ActionResult result = exciteComponent->executeAction("set_output", params);
    
    if (result.success) {
        m_exciteVoltageOn = enable;
        if (enable) {
            m_exciteOnMs = millis();
            log(Logger::DEBUG, "pH excitation voltage enabled");
        } else {
            log(Logger::DEBUG, "pH excitation voltage disabled");
        }
        return true;
    } else {
        log(Logger::ERROR, "Failed to control pH excitation voltage: " + result.message);
        return false;
    }
}

bool PHSensorComponent::isExcitationStabilized() const {
    if (m_exciteVoltageComponentId.length() == 0) {
        return true; // No excitation voltage required
    }
    
    if (!m_exciteVoltageOn) {
        return false; // Excitation not enabled
    }
    
    // Check if stabilization time has passed
    uint32_t elapsed = millis() - m_exciteOnMs;
    return elapsed >= m_exciteStabilizeMs;
}

void PHSensorComponent::updateSensorMode() {
    if (m_gpioPin == 0) {
        m_currentMode = PHSensorMode::MOCK;
    } else if (!m_samplingActive) {
        m_currentMode = PHSensorMode::SLEEPING;
    } else {
        // Determine if we're in calibration or normal mode
        // This could be enhanced to detect calibration mode based on actions or state
        m_currentMode = PHSensorMode::READ_NORMAL;
    }
}

String PHSensorComponent::getModeString(PHSensorMode mode) const {
    switch (mode) {
        case PHSensorMode::SLEEPING: return "SLEEPING";
        case PHSensorMode::READ_NORMAL: return "READ_NORMAL";
        case PHSensorMode::READ_CALIBRATE: return "READ_CALIBRATE";
        case PHSensorMode::MOCK: return "MOCK";
        default: return "UNKNOWN";
    }
}

BaseComponent* PHSensorComponent::getExciteVoltageComponent() {
    if (m_orchestrator && m_exciteVoltageComponentId.length() > 0) {
        // Implementation would depend on your orchestrator's component lookup method
        // return m_orchestrator->getComponent(m_exciteVoltageComponentId);
    }
    return nullptr;
}

float PHSensorComponent::calculateWeightedAverageWithOutlierRemoval() {
    if (m_lastReads.empty()) return 0.0f;
    
    // If only one reading, return it directly (no outlier removal needed)
    if (m_lastReads.size() == 1) {
        return m_lastReads[0];
    }
    
    // Create a copy for outlier removal without affecting original data
    std::vector<float> cleanedReads = m_lastReads;
    
    // Remove outliers using Z-score method
    if (cleanedReads.size() >= 3) {  // Need at least 3 samples for outlier detection
        float mean = calculateMean(cleanedReads);
        float stdDev = calculateStandardDeviation(cleanedReads);
        
        if (stdDev > 0) {
            cleanedReads.erase(
                std::remove_if(cleanedReads.begin(), cleanedReads.end(),
                    [this, mean, stdDev](float value) {
                        float zScore = abs(value - mean) / stdDev;
                        return zScore > m_outlierThreshold;
                    }
                ), cleanedReads.end()
            );
        }
    }
    
    // Calculate weighted average of cleaned data
    if (cleanedReads.empty()) {
        log(Logger::WARNING, "All samples were outliers - using original data");
        cleanedReads = m_lastReads;
    }
    
    float sum = 0.0f;
    for (float value : cleanedReads) {
        sum += value;
    }
    
    return sum / cleanedReads.size();
}

bool PHSensorComponent::isOutlier(float value, const std::vector<float>& data) const {
    if (data.size() < 3) return false;  // Need at least 3 samples
    
    float mean = calculateMean(data);
    float stdDev = calculateStandardDeviation(data);
    
    if (stdDev == 0) return false;  // No variation
    
    float zScore = abs(value - mean) / stdDev;
    return zScore > m_outlierThreshold;
}

void PHSensorComponent::removeOutliers() {
    if (m_lastReads.size() < 3) return;  // Need at least 3 samples
    
    size_t originalSize = m_lastReads.size();
    float mean = calculateMean(m_lastReads);
    float stdDev = calculateStandardDeviation(m_lastReads);
    
    if (stdDev > 0) {
        m_lastReads.erase(
            std::remove_if(m_lastReads.begin(), m_lastReads.end(),
                [this, mean, stdDev](float value) {
                    float zScore = abs(value - mean) / stdDev;
                    if (zScore > m_outlierThreshold) {
                        m_outliersRemoved++;
                        return true;
                    }
                    return false;
                }
            ), m_lastReads.end()
        );
        
        if (originalSize != m_lastReads.size()) {
            log(Logger::INFO, "Removed " + String(originalSize - m_lastReads.size()) + 
                " outliers (threshold: " + String(m_outlierThreshold, 1) + " σ)");
        }
    }
}

float PHSensorComponent::calculateStandardDeviation(const std::vector<float>& data) const {
    if (data.size() < 2) return 0.0f;
    
    float mean = calculateMean(data);
    float sumSquaredDiffs = 0.0f;
    
    for (float value : data) {
        float diff = value - mean;
        sumSquaredDiffs += diff * diff;
    }
    
    return sqrt(sumSquaredDiffs / (data.size() - 1));  // Sample standard deviation
}

float PHSensorComponent::calculateMean(const std::vector<float>& data) const {
    if (data.empty()) return 0.0f;
    
    float sum = 0.0f;
    for (float value : data) {
        sum += value;
    }
    
    return sum / data.size();
}

void PHSensorComponent::startSamplingWindow() {
    m_samplingStartMs = millis();
    m_samplingEndMs = m_samplingStartMs + m_timePeriodForSampling;
    m_samplingActive = true;
    m_outliersRemoved = 0;
    
    // Clear previous readings to start fresh
    m_lastReads.clear();
    m_readIndex = 0;
    m_bufferFull = false;
    
    // Enable excitation voltage if configured
    if (m_exciteVoltageComponentId.length() > 0) {
        controlExcitationVoltage(true);
    }
    
    log(Logger::INFO, "Starting pH sampling window: " + String(m_timePeriodForSampling) + "ms, " + 
        String(m_sampleSize) + " samples max");
}

void PHSensorComponent::endSamplingWindow() {
    m_samplingActive = false;
    uint32_t actualDuration = millis() - m_samplingStartMs;
    
    // Disable excitation voltage if configured
    if (m_exciteVoltageComponentId.length() > 0) {
        controlExcitationVoltage(false);
    }
    
    log(Logger::INFO, "pH sampling window ended: " + String(actualDuration) + "ms duration, " + 
        String(m_lastReads.size()) + " samples collected");
}

bool PHSensorComponent::isSamplingWindowComplete() const {
    if (!m_samplingActive) return false;
    
    uint32_t currentTime = millis();
    bool timeExpired = currentTime >= m_samplingEndMs;
    bool bufferFull = m_lastReads.size() >= m_sampleSize;
    
    return timeExpired || bufferFull;
}

uint32_t PHSensorComponent::calculateNextExecutionTime() const {
    uint32_t currentTime = millis();
    
    if (m_samplingActive) {
        // During sampling: check frequently for new readings
        return currentTime + min(m_readingIntervalMs, 500U);  // At least every 500ms
    } else {
        // After sampling window completes: wait for end window + 100ms resource allocation
        // Then start next sampling cycle
        uint32_t nextSamplingStart = m_samplingEndMs + 100;  // 100ms resource allocation buffer
        
        // If we're past the buffer time, start immediately
        if (currentTime >= nextSamplingStart) {
            return currentTime + 100;  // Small delay to prevent tight loop
        } else {
            return nextSamplingStart;
        }
    }
}