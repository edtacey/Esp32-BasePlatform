/**
 * @file TSL2561Component.cpp
 * @brief TSL2561Component implementation
 */

#include "TSL2561Component.h"

TSL2561Component::TSL2561Component(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "TSL2561", name, storage, orchestrator)
{
    log(Logger::DEBUG, "TSL2561Component created");
}

TSL2561Component::~TSL2561Component() {
    cleanup();
}

JsonDocument TSL2561Component::getDefaultSchema() const {
    JsonDocument schema;
    
    // Schema metadata
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["title"] = "TSL2561 Digital Light Sensor";
    schema["description"] = "TSL2561 light sensor configuration schema";
    schema["type"] = "object";
    
    // Required fields
    JsonArray required = schema["required"].to<JsonArray>();
    required.add("i2cAddress");
    required.add("sdaPin");
    required.add("sclPin");
    required.add("samplingIntervalMs");
    
    // Properties with defaults
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // I2C Address (default: 0x39)
    JsonObject addrProp = properties["i2cAddress"].to<JsonObject>();
    addrProp["type"] = "integer";
    addrProp["minimum"] = 0x29;  // TSL2561_ADDR_LOW
    addrProp["maximum"] = 0x49;  // TSL2561_ADDR_HIGH
    addrProp["default"] = 0x39;  // TSL2561_ADDR_FLOAT
    addrProp["description"] = "I2C address of TSL2561 sensor (0x29, 0x39, or 0x49)";
    
    // SDA Pin (default: 21)
    JsonObject sdaProp = properties["sdaPin"].to<JsonObject>();
    sdaProp["type"] = "integer";
    sdaProp["minimum"] = 0;
    sdaProp["maximum"] = 39;
    sdaProp["default"] = 21;
    sdaProp["description"] = "GPIO pin for I2C SDA line";
    
    // SCL Pin (default: 22)
    JsonObject sclProp = properties["sclPin"].to<JsonObject>();
    sclProp["type"] = "integer";
    sclProp["minimum"] = 0;
    sclProp["maximum"] = 39;
    sclProp["default"] = 22;
    sclProp["description"] = "GPIO pin for I2C SCL line";
    
    // I2C Frequency (default: 100000)
    JsonObject freqProp = properties["i2cFrequency"].to<JsonObject>();
    freqProp["type"] = "integer";
    freqProp["minimum"] = 100000;   // 100kHz
    freqProp["maximum"] = 400000;   // 400kHz
    freqProp["default"] = 100000;
    freqProp["description"] = "I2C bus frequency in Hz";
    
    // Sampling interval (default: 2000ms)
    JsonObject intervalProp = properties["samplingIntervalMs"].to<JsonObject>();
    intervalProp["type"] = "integer";
    intervalProp["minimum"] = 500;     // 500ms minimum
    intervalProp["maximum"] = 3600000; // 1 hour max
    intervalProp["default"] = 2000;
    intervalProp["description"] = "Sampling interval in milliseconds";
    
    // Gain setting (default: "1x")
    JsonObject gainProp = properties["gain"].to<JsonObject>();
    gainProp["type"] = "string";
    JsonArray gainEnum = gainProp["enum"].to<JsonArray>();
    gainEnum.add("1x");
    gainEnum.add("16x");
    gainProp["default"] = "1x";
    gainProp["description"] = "Sensor gain setting (1x or 16x)";
    
    // Integration time (default: "402ms")
    JsonObject integrationProp = properties["integrationTime"].to<JsonObject>();
    integrationProp["type"] = "string";
    JsonArray integrationEnum = integrationProp["enum"].to<JsonArray>();
    integrationEnum.add("13ms");
    integrationEnum.add("101ms");
    integrationEnum.add("402ms");
    integrationProp["default"] = "402ms";
    integrationProp["description"] = "Integration time (13ms, 101ms, or 402ms)";
    
    // Sensor name/label
    JsonObject nameProp = properties["sensorName"].to<JsonObject>();
    nameProp["type"] = "string";
    nameProp["maxLength"] = 64;
    nameProp["default"] = "TSL2561 Light Sensor";
    nameProp["description"] = "Human-readable sensor name";
    
    log(Logger::DEBUG, "Generated default schema with I2C SDA=21, SCL=22, addr=0x39");
    return schema;
}

bool TSL2561Component::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing TSL2561 sensor...");
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
    
    // Initialize I2C bus
    if (!initializeI2C()) {
        setError("Failed to initialize I2C bus");
        return false;
    }
    
    // Initialize sensor
    if (!initializeSensor()) {
        setError("Failed to initialize TSL2561 sensor");
        return false;
    }
    
    // Set initial execution time
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    
    setState(ComponentState::READY);
    log(Logger::INFO, String("TSL2561 initialized - I2C addr=0x") + String(m_i2cAddress, HEX) +
                      ", SDA=" + m_sdaPin + ", SCL=" + m_sclPin +
                      ", interval=" + m_samplingIntervalMs + "ms");
    
    return true;
}

ExecutionResult TSL2561Component::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    
    log(Logger::DEBUG, "Executing TSL2561 reading...");
    setState(ComponentState::EXECUTING);
    
    // Perform the sensor reading
    bool success = performReading();
    
    // Prepare result data
    JsonDocument data;
    data["timestamp"] = millis();
    data["i2cAddress"] = m_i2cAddress;
    data["success"] = success;
    
    if (success) {
        data["lux"] = m_lastLux;
        data["broadband"] = m_lastBroadband;
        data["infrared"] = m_lastInfrared;
        data["gain"] = gainToString(m_gain);
        data["integrationTime"] = integrationToString(m_integration);
        
        result.success = true;
        result.message = "Reading successful";
        
        log(Logger::INFO, String("Lux=") + m_lastLux + 
                          ", BB=" + m_lastBroadband + 
                          ", IR=" + m_lastInfrared);
    } else {
        data["error"] = "Sensor reading failed";
        result.success = false;
        result.message = "Sensor reading failed";
        
        log(Logger::WARNING, "Sensor reading failed");
    }
    
    // Update statistics
    result.executionTimeMs = millis() - startTime;
    result.data = data;
    
    // Schedule next execution
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    
    // Update component stats
    updateExecutionStats();
    
    setState(ComponentState::READY);
    return result;
}

void TSL2561Component::cleanup() {
    log(Logger::DEBUG, "Cleaning up TSL2561 sensor");
    
    if (m_tsl) {
        delete m_tsl;
        m_tsl = nullptr;
    }
    
    m_sensorInitialized = false;
    m_i2cInitialized = false;
}

float TSL2561Component::readLux() {
    if (!m_sensorInitialized || !m_tsl) {
        return NAN;
    }
    
    sensors_event_t event;
    m_tsl->getEvent(&event);
    
    if (event.light) {
        return event.light;
    } else {
        return NAN;
    }
}

bool TSL2561Component::readRawValues(uint16_t& broadband, uint16_t& infrared) {
    if (!m_sensorInitialized || !m_tsl) {
        return false;
    }
    
    m_tsl->getLuminosity(&broadband, &infrared);
    return true;
}

JsonDocument TSL2561Component::getLastReadings() const {
    JsonDocument readings;
    
    readings["lux"] = m_lastLux;
    readings["broadband"] = m_lastBroadband;
    readings["infrared"] = m_lastInfrared;
    readings["timestamp"] = m_lastReadingTime;
    readings["valid"] = !isnan(m_lastLux);
    readings["gain"] = gainToString(m_gain);
    readings["integrationTime"] = integrationToString(m_integration);
    
    return readings;
}

bool TSL2561Component::isSensorHealthy() const {
    // Consider sensor healthy if we've had successful readings recently
    if (m_successfulReadings == 0) return false;
    
    uint32_t totalReadings = m_successfulReadings + m_failedReadings;
    if (totalReadings == 0) return false;
    
    float successRate = (float)m_successfulReadings / totalReadings;
    return successRate >= 0.8f;  // 80% success rate threshold
}

JsonDocument TSL2561Component::getSensorInfo() const {
    JsonDocument info;
    
    info["sensorType"] = "TSL2561";
    info["manufacturer"] = "AMS";
    info["description"] = "Digital Light Sensor";
    info["i2cAddress"] = m_i2cAddress;
    info["sensorId"] = m_sensorId;
    info["initialized"] = m_sensorInitialized;
    
    if (m_sensorInitialized) {
        info["currentGain"] = gainToString(m_gain);
        info["currentIntegration"] = integrationToString(m_integration);
    }
    
    return info;
}

JsonDocument TSL2561Component::getReadingStats() const {
    JsonDocument stats;
    
    stats["successfulReadings"] = m_successfulReadings;
    stats["failedReadings"] = m_failedReadings;
    stats["totalReadings"] = m_successfulReadings + m_failedReadings;
    
    if (m_successfulReadings + m_failedReadings > 0) {
        float successRate = (float)m_successfulReadings / (m_successfulReadings + m_failedReadings);
        stats["successRate"] = successRate;
    } else {
        stats["successRate"] = 0.0f;
    }
    
    stats["lastReadingTime"] = m_lastReadingTime;
    stats["sensorHealthy"] = isSensorHealthy();
    
    return stats;
}

bool TSL2561Component::setGain(tsl2561Gain_t gain) {
    if (!m_sensorInitialized || !m_tsl) {
        return false;
    }
    
    m_tsl->setGain(gain);
    m_gain = gain;
    
    log(Logger::INFO, "Gain set to " + gainToString(gain));
    return true;
}

bool TSL2561Component::setIntegrationTime(tsl2561IntegrationTime_t integration) {
    if (!m_sensorInitialized || !m_tsl) {
        return false;
    }
    
    m_tsl->setIntegrationTime(integration);
    m_integration = integration;
    
    log(Logger::INFO, "Integration time set to " + integrationToString(integration));
    return true;
}

// Private methods

bool TSL2561Component::applyConfiguration(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying TSL2561 configuration");
    
    // Extract I2C address
    if (config["i2cAddress"].is<uint8_t>()) {
        m_i2cAddress = config["i2cAddress"].as<uint8_t>();
    }
    
    // Extract I2C pins
    if (config["sdaPin"].is<uint8_t>()) {
        m_sdaPin = config["sdaPin"].as<uint8_t>();
    }
    if (config["sclPin"].is<uint8_t>()) {
        m_sclPin = config["sclPin"].as<uint8_t>();
    }
    
    // Extract I2C frequency
    if (config["i2cFrequency"].is<uint32_t>()) {
        m_i2cFreq = config["i2cFrequency"].as<uint32_t>();
    }
    
    // Extract sampling interval
    if (config["samplingIntervalMs"].is<uint32_t>()) {
        m_samplingIntervalMs = config["samplingIntervalMs"].as<uint32_t>();
    }
    
    // Extract gain setting
    if (config["gain"].is<String>()) {
        String gainStr = config["gain"].as<String>();
        m_gain = parseGain(gainStr);
    }
    
    // Extract integration time
    if (config["integrationTime"].is<String>()) {
        String integrationStr = config["integrationTime"].as<String>();
        m_integration = parseIntegrationTime(integrationStr);
    }
    
    log(Logger::DEBUG, String("Config applied: addr=0x") + String(m_i2cAddress, HEX) +
                       ", SDA=" + m_sdaPin + ", SCL=" + m_sclPin +
                       ", freq=" + m_i2cFreq + "Hz" +
                       ", interval=" + m_samplingIntervalMs + "ms");
    
    return true;
}

bool TSL2561Component::initializeI2C() {
    log(Logger::DEBUG, String("Initializing I2C bus - SDA=") + m_sdaPin + 
                       ", SCL=" + m_sclPin + ", freq=" + m_i2cFreq + "Hz");
    
    Wire.begin(m_sdaPin, m_sclPin, m_i2cFreq);
    
    // Test I2C communication by scanning for device
    Wire.beginTransmission(m_i2cAddress);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        log(Logger::INFO, String("I2C device found at address 0x") + String(m_i2cAddress, HEX));
        m_i2cInitialized = true;
        return true;
    } else {
        log(Logger::WARNING, String("No I2C device found at address 0x") + String(m_i2cAddress, HEX) +
                            " (error=" + error + ")");
        return false;  // Don't fail completely - sensor might still work
    }
}

bool TSL2561Component::initializeSensor() {
    log(Logger::DEBUG, String("Initializing TSL2561 sensor at I2C address 0x") + String(m_i2cAddress, HEX));
    
    // Clean up existing sensor if any
    if (m_tsl) {
        delete m_tsl;
        m_tsl = nullptr;
    }
    
    // Create new sensor instance
    m_tsl = new Adafruit_TSL2561_Unified((tsl2561IntegrationTime_t)m_i2cAddress, 12345);
    if (!m_tsl) {
        log(Logger::ERROR, "Failed to create TSL2561 sensor instance");
        return false;
    }
    
    // Initialize the sensor
    if (!m_tsl->begin()) {
        log(Logger::ERROR, "TSL2561 sensor initialization failed - check wiring!");
        delete m_tsl;
        m_tsl = nullptr;
        return false;
    }
    
    // Configure sensor
    m_tsl->enableAutoRange(true);           // Auto-gain
    m_tsl->setIntegrationTime(m_integration);
    
    // Get sensor details
    sensor_t sensor;
    m_tsl->getSensor(&sensor);
    m_sensorId = sensor.sensor_id;
    
    log(Logger::INFO, String("TSL2561 sensor initialized successfully"));
    log(Logger::INFO, String("  Sensor ID: ") + m_sensorId);
    log(Logger::INFO, String("  Max Value: ") + sensor.max_value + " lux");
    log(Logger::INFO, String("  Min Value: ") + sensor.min_value + " lux");
    log(Logger::INFO, String("  Resolution: ") + sensor.resolution + " lux");
    
    m_sensorInitialized = true;
    return true;
}

bool TSL2561Component::performReading() {
    if (!m_sensorInitialized || !m_tsl) {
        m_failedReadings++;
        return false;
    }
    
    // Read lux value
    float lux = readLux();
    
    // Read raw values
    uint16_t broadband, infrared;
    bool rawSuccess = readRawValues(broadband, infrared);
    
    // Validate readings
    if (isnan(lux) || !rawSuccess || !validateReadings(lux, broadband, infrared)) {
        m_failedReadings++;
        return false;
    }
    
    // Store readings
    m_lastLux = lux;
    m_lastBroadband = broadband;
    m_lastInfrared = infrared;
    m_lastReadingTime = millis();
    m_successfulReadings++;
    
    return true;
}

bool TSL2561Component::validateReadings(float lux, uint16_t broadband, uint16_t infrared) const {
    // Check for NaN lux value
    if (isnan(lux)) {
        return false;
    }
    
    // Check for reasonable lux range (0 to 40,000+ lux)
    if (lux < 0 || lux > 100000) {
        return false;
    }
    
    // Check if raw values are reasonable
    // (saturated readings would be 0xFFFF)
    if (broadband == 0xFFFF && infrared == 0xFFFF) {
        return false;  // Sensor saturated
    }
    
    return true;
}

// Helper methods for string conversion

String TSL2561Component::gainToString(tsl2561Gain_t gain) const {
    switch (gain) {
        case TSL2561_GAIN_1X:  return "1x";
        case TSL2561_GAIN_16X: return "16x";
        default:               return "unknown";
    }
}

String TSL2561Component::integrationToString(tsl2561IntegrationTime_t integration) const {
    switch (integration) {
        case TSL2561_INTEGRATIONTIME_13MS:  return "13ms";
        case TSL2561_INTEGRATIONTIME_101MS: return "101ms";
        case TSL2561_INTEGRATIONTIME_402MS: return "402ms";
        default:                            return "unknown";
    }
}

tsl2561Gain_t TSL2561Component::parseGain(const String& gainStr) const {
    if (gainStr == "1x") return TSL2561_GAIN_1X;
    if (gainStr == "16x") return TSL2561_GAIN_16X;
    return TSL2561_GAIN_1X;  // Default
}

tsl2561IntegrationTime_t TSL2561Component::parseIntegrationTime(const String& integrationStr) const {
    if (integrationStr == "13ms") return TSL2561_INTEGRATIONTIME_13MS;
    if (integrationStr == "101ms") return TSL2561_INTEGRATIONTIME_101MS;
    if (integrationStr == "402ms") return TSL2561_INTEGRATIONTIME_402MS;
    return TSL2561_INTEGRATIONTIME_402MS;  // Default
}