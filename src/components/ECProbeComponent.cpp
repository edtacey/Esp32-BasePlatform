#include "ECProbeComponent.h"
#include "../utils/Logger.h"
#include "../core/Orchestrator.h"
#include <Arduino.h>

ECProbeComponent::ECProbeComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "ECProbe", name, storage, orchestrator) {
    
    // Initialize calibration points for EC probe
    m_calibrationPoints[0].voltage = 0.0f;
    m_calibrationPoints[0].ec_us_cm = 0.0f;
    m_calibrationPoints[0].isValid = false;
    m_calibrationPoints[0].timestampMs = 0;
    
    m_calibrationPoints[1].voltage = 0.0f;
    m_calibrationPoints[1].ec_us_cm = 84.0f;
    m_calibrationPoints[1].isValid = false;
    m_calibrationPoints[1].timestampMs = 0;
    
    m_calibrationPoints[2].voltage = 0.0f;
    m_calibrationPoints[2].ec_us_cm = 1413.0f;
    m_calibrationPoints[2].isValid = false;
    m_calibrationPoints[2].timestampMs = 0;
    
    // Reserve space for readings array - EC needs more samples for stability
    m_lastReads.reserve(m_sampleSize);
}

ECProbeComponent::~ECProbeComponent() {
    cleanup();
}

JsonDocument ECProbeComponent::getDefaultSchema() const {
    JsonDocument schema;
    
    // Schema metadata
    schema["version"] = 1;
    schema["type"] = "ECProbe";
    schema["description"] = "Electrical Conductivity probe with 3-point calibration and temperature compensation";
    
    // Configuration parameters
    JsonObject config = schema["config"].to<JsonObject>();
    config["gpio_pin"] = 35;
    config["temp_coefficient"] = 2.0f;
    config["sample_size"] = 15;
    config["adc_voltage_ref"] = 3.3f;
    config["adc_resolution"] = 4096;
    config["reading_interval_ms"] = 800;
    config["time_period_for_sampling"] = 15000;
    config["outlier_threshold"] = 2.5f;
    config["tds_conversion_factor"] = 0.64f;
    config["temperature_source_id"] = "";
    config["excite_voltage_component_id"] = "";
    config["excite_stabilize_ms"] = 1000;
    
    // Calibration points
    JsonArray calibration = config["calibration_points"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        JsonObject point = calibration.add<JsonObject>();
        point["ec_us_cm"] = (i == 0) ? 0.0f : (i == 1) ? 84.0f : 1413.0f;
        point["voltage"] = 0.0f;
        point["valid"] = false;
        point["timestamp_ms"] = 0;
    }
    
    return schema;
}

bool ECProbeComponent::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing EC Probe Component: " + m_componentName);
    
    if (!applyConfig(config)) {
        log(Logger::ERROR, "Failed to apply EC probe configuration");
        return false;
    }
    
    if (!initializeSensor()) {
        log(Logger::ERROR, "Failed to initialize EC probe hardware");
        return false;
    }
    
    logCalibrationStatus();
    
    // Set initial execution time to start sampling cycle
    setNextExecutionMs(millis() + m_readingIntervalMs);
    
    setState(ComponentState::READY);
    log(Logger::INFO, "EC Probe initialized successfully on GPIO " + String(m_gpioPin));
    return true;
}

ExecutionResult ECProbeComponent::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    uint32_t currentTime = millis();
    
    setState(ComponentState::EXECUTING);
    
    // Update sensor mode based on current state
    updateSensorMode();
    
    // For mock mode, use simplified direct reading
    if (m_currentMode == ECProbeMode::MOCK) {
        // Take a direct reading for mock mode
        float rawVoltage = readRawVoltage();
        if (rawVoltage >= 0) {
            m_currentVolts = rawVoltage;
            m_currentTemp = getTemperatureReading();
            m_currentEC = convertVoltageToEC(m_currentVolts, m_currentTemp);
            m_currentTDS = convertECtoTDS(m_currentEC);
            m_totalReadings++;
            
            log(Logger::DEBUG, "EC Mock reading: " + String(rawVoltage, 4) + "V -> " + 
                String(m_currentEC, 1) + " µS/cm, " + String(m_currentTDS, 1) + " ppm");
        }
    } else {
        // Initialize sampling window if not active
        if (!m_samplingActive) {
            startSamplingWindow();
        }
    }
    
    // Take readings during the sampling window (only for non-mock mode)
    if (m_currentMode != ECProbeMode::MOCK && m_samplingActive && !isSamplingWindowComplete()) {
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
                
                log(Logger::DEBUG, "EC Sample " + String(m_lastReads.size()) + "/" + String(m_sampleSize) + 
                    ": " + String(rawVoltage, 4) + "V");
            } else {
                m_errorCount++;
                log(Logger::WARNING, "Failed to read EC probe voltage");
            }
        } else {
            log(Logger::DEBUG, "Waiting for EC excitation voltage to stabilize...");
        }
    }
    
    // Process samples when window is complete (only for non-mock mode)
    if (m_currentMode != ECProbeMode::MOCK && m_samplingActive && isSamplingWindowComplete()) {
        endSamplingWindow();
        
        // Remove outliers from sample set
        removeOutliers();
        
        // Calculate final averaged voltage with outlier removal
        m_currentVolts = calculateWeightedAverageWithOutlierRemoval();
        
        // Get temperature for compensation
        m_currentTemp = getTemperatureReading();
        
        // Calculate EC with temperature compensation
        m_currentEC = convertVoltageToEC(m_currentVolts, m_currentTemp);
        
        // Calculate TDS from EC
        m_currentTDS = convertECtoTDS(m_currentEC);
        
        // Update statistics
        if (m_currentEC >= 0) {
            m_minRecordedEC = min(m_minRecordedEC, m_currentEC);
            m_maxRecordedEC = max(m_maxRecordedEC, m_currentEC);
        }
        
        log(Logger::INFO, "EC sampling complete: " + String(m_lastReads.size()) + " samples, " + 
            String(m_outliersRemoved) + " outliers removed, EC = " + String(m_currentEC, 1) + " µS/cm, TDS = " + 
            String(m_currentTDS, 1) + " ppm");
    }
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
    data["current_ec"] = m_currentEC;
    data["current_tds"] = m_currentTDS;
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
    data["min_recorded_ec"] = m_minRecordedEC;
    data["max_recorded_ec"] = m_maxRecordedEC;
    data["buffer_full"] = m_bufferFull;
    data["tds_conversion_factor"] = m_tdsConversionFactor;
    
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
    storeExecutionDataString(dataStr);
    
    result.success = true;
    result.data = data;
    result.executionTimeMs = millis() - startTime;
    
    setState(ComponentState::READY);
    return result;
}

void ECProbeComponent::cleanup() {
    log(Logger::INFO, "Cleaning up EC probe component");
    m_lastReads.clear();
}

JsonDocument ECProbeComponent::getCurrentConfig() const {
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
    config["tds_conversion_factor"] = m_tdsConversionFactor;
    config["temperature_source_id"] = m_temperatureSourceId;
    config["excite_voltage_component_id"] = m_exciteVoltageComponentId;
    config["excite_stabilize_ms"] = m_exciteStabilizeMs;
    
    // Include calibration data
    JsonArray calibration = config["calibration_points"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        JsonObject point = calibration.add<JsonObject>();
        point["ec_us_cm"] = m_calibrationPoints[i].ec_us_cm;
        point["voltage"] = m_calibrationPoints[i].voltage;
        point["valid"] = m_calibrationPoints[i].isValid;
        point["timestamp_ms"] = m_calibrationPoints[i].timestampMs;
    }
    
    return config;
}

bool ECProbeComponent::applyConfig(const JsonDocument& config) {
    // Apply basic configuration
    m_gpioPin = config["gpio_pin"] | m_gpioPin;
    m_tempCoefficient = config["temp_coefficient"] | m_tempCoefficient;
    m_sampleSize = config["sample_size"] | m_sampleSize;
    m_adcVoltageRef = config["adc_voltage_ref"] | m_adcVoltageRef;
    m_adcResolution = config["adc_resolution"] | m_adcResolution;
    m_readingIntervalMs = config["reading_interval_ms"] | m_readingIntervalMs;
    m_timePeriodForSampling = config["time_period_for_sampling"] | m_timePeriodForSampling;
    m_outlierThreshold = config["outlier_threshold"] | m_outlierThreshold;
    m_tdsConversionFactor = config["tds_conversion_factor"] | m_tdsConversionFactor;
    m_temperatureSourceId = config["temperature_source_id"] | m_temperatureSourceId;
    m_exciteVoltageComponentId = config["excite_voltage_component_id"] | m_exciteVoltageComponentId;
    m_exciteStabilizeMs = config["excite_stabilize_ms"] | m_exciteStabilizeMs;
    
    // Validate sample size
    if (m_sampleSize < 1) m_sampleSize = 1;
    if (m_sampleSize > 100) m_sampleSize = 100;
    
    // Validate TDS conversion factor
    if (m_tdsConversionFactor <= 0) m_tdsConversionFactor = 0.64f;
    
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

std::vector<ComponentAction> ECProbeComponent::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    
    // Calibrate action - 3-point calibration
    ComponentAction calibrateAction;
    calibrateAction.name = "calibrate";
    calibrateAction.description = "Perform 3-point EC calibration (dry, low, high)";
    calibrateAction.timeoutMs = 30000;
    calibrateAction.requiresReady = true;
    
    ActionParameter dryParam;
    dryParam.name = "dry_voltage";
    dryParam.type = ActionParameterType::FLOAT;
    dryParam.required = true;
    dryParam.minValue = 0.0f;
    dryParam.maxValue = 5.0f;
    calibrateAction.parameters.push_back(dryParam);
    
    ActionParameter lowEcParam;
    lowEcParam.name = "low_ec_voltage";
    lowEcParam.type = ActionParameterType::FLOAT;
    lowEcParam.required = true;
    lowEcParam.minValue = 0.0f;
    lowEcParam.maxValue = 5.0f;
    calibrateAction.parameters.push_back(lowEcParam);
    
    ActionParameter highEcParam;
    highEcParam.name = "high_ec_voltage";
    highEcParam.type = ActionParameterType::FLOAT;
    highEcParam.required = true;
    highEcParam.minValue = 0.0f;
    highEcParam.maxValue = 5.0f;
    calibrateAction.parameters.push_back(highEcParam);
    
    ActionParameter lowEcValueParam;
    lowEcValueParam.name = "low_ec_value";
    lowEcValueParam.type = ActionParameterType::FLOAT;
    lowEcValueParam.required = false;
    // lowEcValueParam.defaultValue = 84.0f;
    lowEcValueParam.minValue = 1.0f;
    lowEcValueParam.maxValue = 10000.0f;
    calibrateAction.parameters.push_back(lowEcValueParam);
    
    ActionParameter highEcValueParam;
    highEcValueParam.name = "high_ec_value";
    highEcValueParam.type = ActionParameterType::FLOAT;
    highEcValueParam.required = false;
    // highEcValueParam.defaultValue = 1413.0f;
    highEcValueParam.minValue = 100.0f;
    highEcValueParam.maxValue = 50000.0f;
    calibrateAction.parameters.push_back(highEcValueParam);
    
    actions.push_back(calibrateAction);
    
    // Calibrate Point action - single point calibration
    ComponentAction calibratePointAction;
    calibratePointAction.name = "calibrate_point";
    calibratePointAction.description = "Calibrate single EC point";
    calibratePointAction.timeoutMs = 10000;
    calibratePointAction.requiresReady = true;
    
    ActionParameter ecValueParam;
    ecValueParam.name = "ec_value";
    ecValueParam.type = ActionParameterType::FLOAT;
    ecValueParam.required = true;
    ecValueParam.minValue = 0.0f;
    ecValueParam.maxValue = 50000.0f;
    calibratePointAction.parameters.push_back(ecValueParam);
    
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
    clearCalAction.description = "Clear all EC calibration data";
    clearCalAction.timeoutMs = 5000;
    clearCalAction.requiresReady = true;
    actions.push_back(clearCalAction);
    
    return actions;
}

ActionResult ECProbeComponent::performAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.success = false;
    
    if (actionName == "calibrate") {
        float dry_voltage = parameters["dry_voltage"];
        float low_ec_voltage = parameters["low_ec_voltage"];
        float high_ec_voltage = parameters["high_ec_voltage"];
        float low_ec_value = parameters["low_ec_value"] | 84.0f;
        float high_ec_value = parameters["high_ec_value"] | 1413.0f;
        
        result.success = calibrate(dry_voltage, low_ec_voltage, high_ec_voltage, low_ec_value, high_ec_value);
        result.message = result.success ? 
            "3-point EC calibration completed successfully" :
            "EC calibration failed - check voltage values";
            
    } else if (actionName == "calibrate_point") {
        float ec_value = parameters["ec_value"];
        float voltage = parameters["voltage"];
        
        result.success = calibratePoint(ec_value, voltage);
        result.message = result.success ?
            "EC calibration point updated for " + String(ec_value, 1) + " µS/cm" :
            "Failed to update EC calibration point";
            
    } else if (actionName == "clear_calibration") {
        result.success = clearCalibration();
        result.message = result.success ?
            "EC calibration data cleared" :
            "Failed to clear EC calibration";
    } else {
        result.message = "Unknown action: " + actionName;
    }
    
    return result;
}

bool ECProbeComponent::calibrate(float dry_voltage, float low_ec_voltage, float high_ec_voltage, 
                                 float low_ec_value, float high_ec_value) {
    log(Logger::INFO, "Performing 3-point EC calibration");
    
    uint32_t timestamp = millis();
    
    // Set calibration points
    m_calibrationPoints[0].voltage = dry_voltage;
    m_calibrationPoints[0].ec_us_cm = 0.0f;
    m_calibrationPoints[0].isValid = true;
    m_calibrationPoints[0].timestampMs = timestamp;
    
    m_calibrationPoints[1].voltage = low_ec_voltage;
    m_calibrationPoints[1].ec_us_cm = low_ec_value;
    m_calibrationPoints[1].isValid = true;
    m_calibrationPoints[1].timestampMs = timestamp;
    
    m_calibrationPoints[2].voltage = high_ec_voltage;
    m_calibrationPoints[2].ec_us_cm = high_ec_value;
    m_calibrationPoints[2].isValid = true;
    m_calibrationPoints[2].timestampMs = timestamp;
    
    m_lastCalibrationMs = timestamp;
    m_calibrationValid = validateCalibrationPoints();
    
    if (m_calibrationValid) {
        log(Logger::INFO, "3-point EC calibration successful");
        logCalibrationStatus();
        
        // Save configuration
        saveConfigurationToStorage(getCurrentConfig());
        return true;
    } else {
        log(Logger::ERROR, "EC calibration validation failed");
        return false;
    }
}

bool ECProbeComponent::calibratePoint(float ec_value, float voltage) {
    // Find the closest calibration point
    int pointIndex = -1;
    float minDiff = 100000.0f;  // Max EC difference
    
    for (int i = 0; i < 3; i++) {
        float diff = abs(ec_value - m_calibrationPoints[i].ec_us_cm);
        if (diff < minDiff) {
            minDiff = diff;
            pointIndex = i;
        }
    }
    
    if (pointIndex >= 0) {
        // Allow broader matching for EC values
        float tolerancePercent = 0.5f;  // 50% tolerance
        float tolerance = m_calibrationPoints[pointIndex].ec_us_cm * tolerancePercent;
        
        if (minDiff <= tolerance || pointIndex == 0) {  // Always allow dry calibration
            m_calibrationPoints[pointIndex].voltage = voltage;
            m_calibrationPoints[pointIndex].ec_us_cm = ec_value;  // Update with actual value
            m_calibrationPoints[pointIndex].isValid = true;
            m_calibrationPoints[pointIndex].timestampMs = millis();
            
            m_calibrationValid = validateCalibrationPoints();
            
            log(Logger::INFO, "Updated EC calibration point: " + String(ec_value, 1) + " µS/cm = " + 
                String(voltage, 3) + "V");
            
            // Save configuration
            saveConfigurationToStorage(getCurrentConfig());
            return true;
        }
    }
    
    log(Logger::ERROR, "No suitable calibration point found for EC " + String(ec_value, 1) + " µS/cm");
    return false;
}

bool ECProbeComponent::clearCalibration() {
    log(Logger::INFO, "Clearing EC calibration data");
    
    for (int i = 0; i < 3; i++) {
        m_calibrationPoints[i].voltage = 0.0f;
        m_calibrationPoints[i].isValid = false;
        m_calibrationPoints[i].timestampMs = 0;
        // Keep original EC values for reference
    }
    
    m_calibrationValid = false;
    m_lastCalibrationMs = 0;
    
    // Save configuration
    saveConfigurationToStorage(getCurrentConfig());
    return true;
}

float ECProbeComponent::readRawVoltage() {
    if (m_gpioPin == 0) {
        // Mock mode - return simulated EC voltage
        static uint32_t mockCounter = 0;
        mockCounter++;
        
        // Simulate 400 µS/cm EC reading with some noise
        float baseVoltage = 1.2f;  // Typical mid-range EC voltage
        float noise = (sin(mockCounter * 0.15f) * 0.08f) + ((mockCounter % 11) * 0.015f);
        return baseVoltage + noise;
    }
    
    int adcValue = analogRead(m_gpioPin);
    if (adcValue < 0) return -1.0f;
    
    // Convert ADC value to voltage
    float voltage = (float)adcValue * m_adcVoltageRef / m_adcResolution;
    return voltage;
}

float ECProbeComponent::getCurrentEC(float temperature_c) {
    float voltage = calculateWeightedAverage();
    return convertVoltageToEC(voltage, temperature_c);
}

float ECProbeComponent::getCurrentTDS(float temperature_c) {
    float ec = getCurrentEC(temperature_c);
    return convertECtoTDS(ec);
}

bool ECProbeComponent::isCalibrated() const {
    int validPoints = 0;
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) validPoints++;
    }
    return validPoints >= 2;  // Need at least 2 points for linear interpolation
}

bool ECProbeComponent::initializeSensor() {
    // Set ADC attenuation for ESP32 (0-3.3V range)
    analogSetAttenuation(ADC_11db);
    
    // Initialize readings buffer
    m_lastReads.clear();
    m_readIndex = 0;
    m_bufferFull = false;
    
    return true;
}

float ECProbeComponent::calculateWeightedAverage() const {
    if (m_lastReads.empty()) return 0.0f;
    
    float sum = 0.0f;
    int count = m_bufferFull ? m_sampleSize : m_lastReads.size();
    
    for (int i = 0; i < count; i++) {
        sum += m_lastReads[i];
    }
    
    return sum / count;
}

float ECProbeComponent::calculateWeightedAverageWithOutlierRemoval() {
    if (m_lastReads.empty()) return 0.0f;
    
    // If only one reading, return it directly
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
        log(Logger::WARNING, "All EC samples were outliers - using original data");
        cleanedReads = m_lastReads;
    }
    
    float sum = 0.0f;
    for (float value : cleanedReads) {
        sum += value;
    }
    
    return sum / cleanedReads.size();
}

void ECProbeComponent::addReading(float voltage) {
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

bool ECProbeComponent::isOutlier(float value, const std::vector<float>& data) const {
    if (data.size() < 3) return false;  // Need at least 3 samples
    
    float mean = calculateMean(data);
    float stdDev = calculateStandardDeviation(data);
    
    if (stdDev == 0) return false;  // No variation
    
    float zScore = abs(value - mean) / stdDev;
    return zScore > m_outlierThreshold;
}

void ECProbeComponent::removeOutliers() {
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
                " EC outliers (threshold: " + String(m_outlierThreshold, 1) + " σ)");
        }
    }
}

float ECProbeComponent::calculateStandardDeviation(const std::vector<float>& data) const {
    if (data.size() < 2) return 0.0f;
    
    float mean = calculateMean(data);
    float sumSquaredDiffs = 0.0f;
    
    for (float value : data) {
        float diff = value - mean;
        sumSquaredDiffs += diff * diff;
    }
    
    return sqrt(sumSquaredDiffs / (data.size() - 1));  // Sample standard deviation
}

float ECProbeComponent::calculateMean(const std::vector<float>& data) const {
    if (data.empty()) return 0.0f;
    
    float sum = 0.0f;
    for (float value : data) {
        sum += value;
    }
    
    return sum / data.size();
}

void ECProbeComponent::startSamplingWindow() {
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
    
    log(Logger::INFO, "Starting EC sampling window: " + String(m_timePeriodForSampling) + "ms, " + 
        String(m_sampleSize) + " samples max");
}

void ECProbeComponent::endSamplingWindow() {
    m_samplingActive = false;
    uint32_t actualDuration = millis() - m_samplingStartMs;
    
    // Disable excitation voltage if configured
    if (m_exciteVoltageComponentId.length() > 0) {
        controlExcitationVoltage(false);
    }
    
    log(Logger::INFO, "EC sampling window ended: " + String(actualDuration) + "ms duration, " + 
        String(m_lastReads.size()) + " samples collected");
}

bool ECProbeComponent::isSamplingWindowComplete() const {
    if (!m_samplingActive) return false;
    
    uint32_t currentTime = millis();
    bool timeExpired = currentTime >= m_samplingEndMs;
    bool bufferFull = m_lastReads.size() >= m_sampleSize;
    
    return timeExpired || bufferFull;
}

uint32_t ECProbeComponent::calculateNextExecutionTime() const {
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

float ECProbeComponent::convertVoltageToEC(float voltage, float temperature_c) const {
    // In mock mode, provide simulated EC values without calibration
    if (m_currentMode == ECProbeMode::MOCK) {
        // Simulate EC around 400 µS/cm with variations based on voltage
        float baseEC = 400.0f;
        float voltageDeviation = voltage - 1.2f;
        float ec = baseEC + (voltageDeviation * 500.0f);
        
        // Apply temperature compensation
        float tempCompensation = 1.0f + (m_tempCoefficient / 100.0f) * (temperature_c - 25.0f);
        ec = ec / tempCompensation;
        
        // Ensure reasonable range
        if (ec < 0.0f) ec = 0.0f;
        if (ec > 5000.0f) ec = 5000.0f;
        
        return ec;
    }
    
    if (!isCalibrated()) return -1.0f;
    
    // Basic linear interpolation between calibration points
    float ec = interpolateEC(voltage);
    
    if (ec < 0) return -1.0f;
    
    // Apply temperature compensation
    // EC increases by ~2% per degree C above 25°C
    float tempCompensation = 1.0f + (m_tempCoefficient / 100.0f) * (temperature_c - 25.0f);
    ec = ec / tempCompensation;  // Normalize to 25°C
    
    // Ensure non-negative
    if (ec < 0.0f) ec = 0.0f;
    
    return ec;
}

float ECProbeComponent::convertECtoTDS(float ec_us_cm) const {
    if (ec_us_cm < 0) return -1.0f;
    
    // Convert EC (µS/cm) to TDS (ppm) using conversion factor
    return ec_us_cm * m_tdsConversionFactor;
}

float ECProbeComponent::getTemperatureReading() {
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

bool ECProbeComponent::updateCalibrationFromConfig(const JsonDocument& config) {
    if (!config.containsKey("calibration_points")) return true;
    
    JsonArrayConst calibration = config["calibration_points"];
    for (size_t i = 0; i < calibration.size() && i < 3; i++) {
        JsonObjectConst point = calibration[i];
        m_calibrationPoints[i].ec_us_cm = point["ec_us_cm"] | m_calibrationPoints[i].ec_us_cm;
        m_calibrationPoints[i].voltage = point["voltage"] | m_calibrationPoints[i].voltage;
        m_calibrationPoints[i].isValid = point["valid"] | m_calibrationPoints[i].isValid;
        m_calibrationPoints[i].timestampMs = point["timestamp_ms"] | m_calibrationPoints[i].timestampMs;
    }
    
    m_calibrationValid = validateCalibrationPoints();
    return true;
}

void ECProbeComponent::logCalibrationStatus() {
    int validPoints = 0;
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) {
            validPoints++;
            String pointType = (i == 0) ? "Dry" : (i == 1) ? "Low EC" : "High EC";
            log(Logger::INFO, "EC Calibration Point " + pointType + ": " + 
                String(m_calibrationPoints[i].ec_us_cm, 1) + " µS/cm = " + 
                String(m_calibrationPoints[i].voltage, 3) + "V");
        }
    }
    
    log(Logger::INFO, "EC probe calibration: " + String(validPoints) + "/3 points valid, " +
        (isCalibrated() ? "READY" : "NEEDS CALIBRATION"));
}

bool ECProbeComponent::validateCalibrationPoints() const {
    int validPoints = 0;
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) validPoints++;
    }
    return validPoints >= 2;
}

float ECProbeComponent::interpolateEC(float voltage) const {
    // Find valid calibration points for interpolation
    std::vector<std::pair<float, float>> validPoints;  // voltage, EC pairs
    
    for (int i = 0; i < 3; i++) {
        if (m_calibrationPoints[i].isValid) {
            validPoints.push_back({m_calibrationPoints[i].voltage, m_calibrationPoints[i].ec_us_cm});
        }
    }
    
    if (validPoints.size() < 2) return -1.0f;
    
    // Sort by voltage
    std::sort(validPoints.begin(), validPoints.end());
    
    // Simple linear interpolation between closest points
    if (voltage <= validPoints[0].first) {
        // Extrapolate below lowest point (or clamp to zero for dry)
        if (validPoints[0].second == 0.0f) return 0.0f;  // Dry calibration point
        
        if (validPoints.size() >= 2) {
            float slope = (validPoints[1].second - validPoints[0].second) / 
                         (validPoints[1].first - validPoints[0].first);
            float result = validPoints[0].second + slope * (voltage - validPoints[0].first);
            return max(0.0f, result);  // Don't go negative
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

BaseComponent* ECProbeComponent::getTemperatureComponent() {
    if (m_orchestrator && m_temperatureSourceId.length() > 0) {
        // Implementation would depend on your orchestrator's component lookup method
        // return m_orchestrator->getComponent(m_temperatureSourceId);
    }
    return nullptr;
}

bool ECProbeComponent::controlExcitationVoltage(bool enable) {
    if (m_exciteVoltageComponentId.length() == 0) {
        return true; // No excitation component configured
    }
    
    BaseComponent* exciteComponent = getExciteVoltageComponent();
    if (!exciteComponent) {
        log(Logger::WARNING, "EC excitation voltage component not found: " + m_exciteVoltageComponentId);
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
            log(Logger::DEBUG, "EC excitation voltage enabled");
        } else {
            log(Logger::DEBUG, "EC excitation voltage disabled");
        }
        return true;
    } else {
        log(Logger::ERROR, "Failed to control EC excitation voltage: " + result.message);
        return false;
    }
}

bool ECProbeComponent::isExcitationStabilized() const {
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

void ECProbeComponent::updateSensorMode() {
    if (m_gpioPin == 0) {
        m_currentMode = ECProbeMode::MOCK;
    } else if (!m_samplingActive) {
        m_currentMode = ECProbeMode::SLEEPING;
    } else {
        // Determine if we're in calibration or normal mode
        // This could be enhanced to detect calibration mode based on actions or state
        m_currentMode = ECProbeMode::READ_NORMAL;
    }
}

String ECProbeComponent::getModeString(ECProbeMode mode) const {
    switch (mode) {
        case ECProbeMode::SLEEPING: return "SLEEPING";
        case ECProbeMode::READ_NORMAL: return "READ_NORMAL";
        case ECProbeMode::READ_CALIBRATE: return "READ_CALIBRATE";
        case ECProbeMode::MOCK: return "MOCK";
        default: return "UNKNOWN";
    }
}

BaseComponent* ECProbeComponent::getExciteVoltageComponent() {
    if (m_orchestrator && m_exciteVoltageComponentId.length() > 0) {
        // Implementation would depend on your orchestrator's component lookup method
        // return m_orchestrator->getComponent(m_exciteVoltageComponentId);
    }
    return nullptr;
}