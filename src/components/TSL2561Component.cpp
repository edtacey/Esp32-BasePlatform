/**
 * @file TSL2561Component.cpp
 * @brief TSL2561Component implementation - Direct I2C (lightweight)
 */

#include "TSL2561Component.h"
#include "../core/Orchestrator.h"

TSL2561Component::TSL2561Component(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "TSL2561", name, storage, orchestrator)
{
    log(Logger::DEBUG, "TSL2561Component created (direct I2C)");
}

TSL2561Component::~TSL2561Component() {
    cleanup();
}

JsonDocument TSL2561Component::getDefaultSchema() const {
    JsonDocument schema;
    
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["title"] = "TSL2561 Digital Light Sensor";
    schema["description"] = "TSL2561 light sensor configuration schema";
    schema["type"] = "object";
    
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // I2C Address
    JsonObject addrProp = properties["i2cAddress"].to<JsonObject>();
    addrProp["type"] = "integer";
    addrProp["minimum"] = TSL2561_ADDR_LOW;
    addrProp["maximum"] = TSL2561_ADDR_HIGH;
    addrProp["default"] = TSL2561_ADDR_FLOAT;
    addrProp["description"] = "I2C address of TSL2561 sensor";
    
    // SDA Pin
    JsonObject sdaProp = properties["sdaPin"].to<JsonObject>();
    sdaProp["type"] = "integer";
    sdaProp["minimum"] = 0;
    sdaProp["maximum"] = 39;
    sdaProp["default"] = 21;
    sdaProp["description"] = "GPIO pin for I2C SDA line";
    
    // SCL Pin
    JsonObject sclProp = properties["sclPin"].to<JsonObject>();
    sclProp["type"] = "integer";
    sclProp["minimum"] = 0;
    sclProp["maximum"] = 39;
    sclProp["default"] = 22;
    sclProp["description"] = "GPIO pin for I2C SCL line";
    
    // I2C Frequency
    JsonObject freqProp = properties["i2cFrequency"].to<JsonObject>();
    freqProp["type"] = "integer";
    freqProp["minimum"] = 100000;
    freqProp["maximum"] = 400000;
    freqProp["default"] = 100000;
    freqProp["description"] = "I2C bus frequency in Hz";
    
    // Sampling interval
    JsonObject intervalProp = properties["samplingIntervalMs"].to<JsonObject>();
    intervalProp["type"] = "integer";
    intervalProp["minimum"] = 500;
    intervalProp["maximum"] = 3600000;
    intervalProp["default"] = 2000;
    intervalProp["description"] = "Sampling interval in milliseconds";
    
    // Gain setting
    JsonObject gainProp = properties["gain"].to<JsonObject>();
    gainProp["type"] = "string";
    JsonArray gainEnum = gainProp["enum"].to<JsonArray>();
    gainEnum.add("1x");
    gainEnum.add("16x");
    gainProp["default"] = "1x";
    gainProp["description"] = "Sensor gain setting (1x or 16x)";
    
    // Integration time
    JsonObject integrationProp = properties["integrationTime"].to<JsonObject>();
    integrationProp["type"] = "string";
    JsonArray integrationEnum = integrationProp["enum"].to<JsonArray>();
    integrationEnum.add("13ms");
    integrationEnum.add("101ms");
    integrationEnum.add("402ms");
    integrationProp["default"] = "402ms";
    integrationProp["description"] = "Integration time";
    
    // Remote sensor configuration
    JsonObject remoteEnabledProp = properties["useRemoteSensor"].to<JsonObject>();
    remoteEnabledProp["type"] = "boolean";
    remoteEnabledProp["default"] = false;
    remoteEnabledProp["description"] = "Use remote HTTP light sensor instead of local I2C";
    
    JsonObject remoteHostProp = properties["remoteHost"].to<JsonObject>();
    remoteHostProp["type"] = "string";
    remoteHostProp["maxLength"] = 64;
    remoteHostProp["default"] = "";
    remoteHostProp["description"] = "Remote sensor host IP/hostname";
    
    JsonObject remotePortProp = properties["remotePort"].to<JsonObject>();
    remotePortProp["type"] = "integer";
    remotePortProp["minimum"] = 1;
    remotePortProp["maximum"] = 65535;
    remotePortProp["default"] = 80;
    remotePortProp["description"] = "Remote sensor HTTP port";
    
    JsonObject remotePathProp = properties["remotePath"].to<JsonObject>();
    remotePathProp["type"] = "string";
    remotePathProp["maxLength"] = 128;
    remotePathProp["default"] = "/light";
    remotePathProp["description"] = "Remote sensor HTTP endpoint path";
    
    JsonObject httpTimeoutProp = properties["httpTimeoutMs"].to<JsonObject>();
    httpTimeoutProp["type"] = "integer";
    httpTimeoutProp["minimum"] = 1000;
    httpTimeoutProp["maximum"] = 30000;
    httpTimeoutProp["default"] = 5000;
    httpTimeoutProp["description"] = "HTTP timeout in milliseconds";
    
    return schema;
}

bool TSL2561Component::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing TSL2561 sensor...");
    setState(ComponentState::INITIALIZING);
    
    if (!loadConfiguration(config)) {
        setError("Failed to load configuration");
        return false;
    }
    
    if (!applyConfiguration(getConfiguration())) {
        setError("Failed to apply configuration");
        return false;
    }
    
    // Initialize based on sensor mode
    if (m_useRemoteSensor) {
        if (m_remoteHost.length() == 0) {
            setError("Remote host not configured");
            return false;
        }
        log(Logger::INFO, String("Using remote light sensor: ") + m_remoteHost + ":" + m_remotePort + m_remotePath);
        m_sensorInitialized = true;
    } else {
        if (!initializeI2C()) {
            setError("Failed to initialize I2C bus");
            return false;
        }
        
        if (!initializeSensor()) {
            setError("Failed to initialize TSL2561 sensor");
            return false;
        }
    }
    
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    setState(ComponentState::READY);
    
    log(Logger::INFO, String("TSL2561 initialized - ") + 
        (m_useRemoteSensor ? "Remote mode" : ("I2C addr=0x" + String(m_i2cAddress, HEX))));
    
    return true;
}

ExecutionResult TSL2561Component::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    
    setState(ComponentState::EXECUTING);
    
    bool success = performReading();
    
    JsonDocument data;
    data["timestamp"] = millis();
    data["sensorType"] = "TSL2561";
    data["i2cAddress"] = String("0x") + String(m_i2cAddress, HEX);
    data["initialized"] = m_sensorInitialized;
    data["success"] = success;
    
    if (success) {
        data["lux"] = m_lastLux;
        data["broadband"] = m_lastBroadband;
        data["infrared"] = m_lastInfrared;
        data["lastReadingTime"] = m_lastReadingTime;
        data["healthy"] = isSensorHealthy();
        data["readingInterval"] = m_samplingIntervalMs;
        
        // PAR calculation for hydroponics
        float parValue = m_lastLux * 0.0185f;
        data["par_umol"] = parValue;
        
        String ppfdCategory = parValue < 50 ? "very_low" : 
                             parValue < 200 ? "low" : 
                             parValue < 500 ? "medium" : 
                             parValue < 1000 ? "high" : "very_high";
        data["ppfd_category"] = ppfdCategory;
        
        result.success = true;
        result.message = "Reading successful";
        
        log(Logger::INFO, String("Lux=") + m_lastLux + ", BB=" + m_lastBroadband + ", IR=" + m_lastInfrared);
    } else {
        data["error"] = "Sensor reading failed";
        result.success = false;
        result.message = "Sensor reading failed";
    }
    
    result.executionTimeMs = millis() - startTime;
    result.data = data;
    
    String dataStr;
    serializeJson(data, dataStr);
    storeExecutionDataString(dataStr);
    
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    updateExecutionStats();
    setState(ComponentState::READY);
    
    return result;
}

void TSL2561Component::cleanup() {
    log(Logger::DEBUG, "Cleaning up TSL2561 sensor");
    
    if (m_sensorInitialized) {
        writeRegister(TSL2561_REGISTER_CONTROL, TSL2561_CONTROL_POWEROFF);
    }
    
    m_sensorInitialized = false;
    m_i2cInitialized = false;
}

float TSL2561Component::readLux() {
    if (!m_sensorInitialized) {
        return NAN;
    }
    
    uint16_t ch0 = readRegister16(TSL2561_REGISTER_CHAN0_LOW);
    uint16_t ch1 = readRegister16(TSL2561_REGISTER_CHAN1_LOW);
    
    if (ch0 == 0xFFFF || ch1 == 0xFFFF) {
        return NAN;
    }
    
    return calculateLux(ch0, ch1);
}

bool TSL2561Component::readRawValues(uint16_t& broadband, uint16_t& infrared) {
    if (!m_sensorInitialized) {
        return false;
    }
    
    broadband = readRegister16(TSL2561_REGISTER_CHAN0_LOW);
    infrared = readRegister16(TSL2561_REGISTER_CHAN1_LOW);
    
    return (broadband != 0xFFFF && infrared != 0xFFFF);
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
    if (m_successfulReadings == 0) return false;
    uint32_t totalReadings = m_successfulReadings + m_failedReadings;
    if (totalReadings == 0) return false;
    float successRate = (float)m_successfulReadings / totalReadings;
    return successRate >= 0.8f;
}

JsonDocument TSL2561Component::getSensorInfo() const {
    JsonDocument info;
    info["sensorType"] = "TSL2561";
    info["manufacturer"] = "AMS";
    info["description"] = "Digital Light Sensor (Direct I2C)";
    info["i2cAddress"] = m_i2cAddress;
    info["sensorId"] = m_sensorId;
    info["initialized"] = m_sensorInitialized;
    info["useRemoteSensor"] = m_useRemoteSensor;
    
    if (m_useRemoteSensor) {
        info["remoteHost"] = m_remoteHost;
        info["remotePath"] = m_remotePath;
    } else if (m_sensorInitialized) {
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

bool TSL2561Component::setGain(const String& gainStr) {
    if (!m_sensorInitialized || m_useRemoteSensor) {
        return false;
    }
    
    uint8_t newGain = parseGain(gainStr);
    uint8_t timing = newGain | m_integration;
    
    if (writeRegister(TSL2561_REGISTER_TIMING, timing)) {
        m_gain = newGain;
        log(Logger::INFO, "Gain set to " + gainStr);
        return true;
    }
    return false;
}

bool TSL2561Component::setIntegrationTime(const String& integrationStr) {
    if (!m_sensorInitialized || m_useRemoteSensor) {
        return false;
    }
    
    uint8_t newIntegration = parseIntegrationTime(integrationStr);
    uint8_t timing = m_gain | newIntegration;
    
    if (writeRegister(TSL2561_REGISTER_TIMING, timing)) {
        m_integration = newIntegration;
        log(Logger::INFO, "Integration time set to " + integrationStr);
        return true;
    }
    return false;
}

// Private methods

JsonDocument TSL2561Component::getCurrentConfig() const {
    JsonDocument config;
    config["i2cAddress"] = m_i2cAddress;
    config["sdaPin"] = m_sdaPin;
    config["sclPin"] = m_sclPin;
    config["i2cFrequency"] = m_i2cFreq;
    config["samplingIntervalMs"] = m_samplingIntervalMs;
    config["gain"] = gainToString(m_gain);
    config["integrationTime"] = integrationToString(m_integration);
    config["useRemoteSensor"] = m_useRemoteSensor;
    config["remoteHost"] = m_remoteHost;
    config["remotePort"] = m_remotePort;
    config["remotePath"] = m_remotePath;
    config["httpTimeoutMs"] = m_httpTimeoutMs;
    config["config_version"] = 1;
    return config;
}

bool TSL2561Component::applyConfig(const JsonDocument& config) {
    // For partial configuration updates, merge with current values instead of schema defaults
    m_i2cAddress = config["i2cAddress"] | m_i2cAddress;
    m_sdaPin = config["sdaPin"] | m_sdaPin;
    m_sclPin = config["sclPin"] | m_sclPin;
    m_i2cFreq = config["i2cFrequency"] | m_i2cFreq;
    m_samplingIntervalMs = config["samplingIntervalMs"] | m_samplingIntervalMs;
    
    // Handle gain setting
    if (config.containsKey("gain")) {
        String gainStr = config["gain"] | "1x";
        m_gain = parseGain(gainStr);
    }
    
    // Handle integration time setting
    if (config.containsKey("integrationTime")) {
        String integrationStr = config["integrationTime"] | "402ms";
        m_integration = parseIntegrationTime(integrationStr);
    }
    
    // Remote sensor configuration - merge with current values
    m_useRemoteSensor = config["useRemoteSensor"] | m_useRemoteSensor;
    if (config.containsKey("remoteHost")) {
        m_remoteHost = config["remoteHost"] | m_remoteHost;
    }
    m_remotePort = config["remotePort"] | m_remotePort;
    if (config.containsKey("remotePath")) {
        m_remotePath = config["remotePath"] | m_remotePath;
    }
    m_httpTimeoutMs = config["httpTimeoutMs"] | m_httpTimeoutMs;
    
    return true;
}

bool TSL2561Component::applyConfiguration(const JsonDocument& config) {
    return applyConfig(config);
}

bool TSL2561Component::initializeI2C() {
    Wire.begin(m_sdaPin, m_sclPin, m_i2cFreq);
    
    Wire.beginTransmission(m_i2cAddress);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        log(Logger::INFO, String("I2C device found at address 0x") + String(m_i2cAddress, HEX));
        m_i2cInitialized = true;
        return true;
    } else {
        log(Logger::WARNING, String("No I2C device found at address 0x") + String(m_i2cAddress, HEX) + 
                            " - continuing anyway for testing");
        m_i2cInitialized = true; // Allow to continue for testing
        return true;
    }
}

bool TSL2561Component::initializeSensor() {
    // Power on the sensor
    if (!writeRegister(TSL2561_REGISTER_CONTROL, TSL2561_CONTROL_POWERON)) {
        log(Logger::ERROR, "Failed to power on TSL2561 sensor");
        return false;
    }
    
    delay(10);
    
    // Set timing register (gain + integration time)
    uint8_t timing = m_gain | m_integration;
    if (!writeRegister(TSL2561_REGISTER_TIMING, timing)) {
        log(Logger::ERROR, "Failed to set TSL2561 timing register");
        return false;
    }
    
    // Verify sensor is responding
    uint8_t control = readRegister(TSL2561_REGISTER_CONTROL);
    if (control != TSL2561_CONTROL_POWERON) {
        log(Logger::ERROR, "TSL2561 sensor not responding");
        return false;
    }
    
    m_sensorId = 2561;
    m_sensorInitialized = true;
    
    log(Logger::INFO, String("TSL2561 initialized - Gain=") + gainToString(m_gain) + 
        ", Integration=" + integrationToString(m_integration));
    
    return true;
}

bool TSL2561Component::performReading() {
    if (m_useRemoteSensor) {
        return performRemoteReading();
    }
    
    if (!m_sensorInitialized) {
        m_failedReadings++;
        return false;
    }
    
    float lux = readLux();
    uint16_t broadband, infrared;
    bool rawSuccess = readRawValues(broadband, infrared);
    
    if (isnan(lux) || !rawSuccess || !validateReadings(lux, broadband, infrared)) {
        m_failedReadings++;
        return false;
    }
    
    m_lastLux = lux;
    m_lastBroadband = broadband;
    m_lastInfrared = infrared;
    m_lastReadingTime = millis();
    m_successfulReadings++;
    
    return true;
}

bool TSL2561Component::validateReadings(float lux, uint16_t broadband, uint16_t infrared) const {
    if (isnan(lux) || lux < 0 || lux > 100000) return false;
    if (broadband == 0xFFFF && infrared == 0xFFFF) return false;
    return true;
}

// Direct I2C Methods

bool TSL2561Component::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(m_i2cAddress);
    Wire.write(0x80 | reg); // Command register with auto-increment
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

uint8_t TSL2561Component::readRegister(uint8_t reg) {
    Wire.beginTransmission(m_i2cAddress);
    Wire.write(0x80 | reg); // Command register
    if (Wire.endTransmission() != 0) return 0xFF;
    
    Wire.requestFrom(m_i2cAddress, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0xFF;
}

uint16_t TSL2561Component::readRegister16(uint8_t reg) {
    Wire.beginTransmission(m_i2cAddress);
    Wire.write(0x80 | reg); // Command register  
    if (Wire.endTransmission() != 0) return 0xFFFF;
    
    Wire.requestFrom(m_i2cAddress, (uint8_t)2);
    if (Wire.available() >= 2) {
        uint8_t low = Wire.read();
        uint8_t high = Wire.read();
        return (uint16_t)low | ((uint16_t)high << 8);
    }
    return 0xFFFF;
}

float TSL2561Component::calculateLux(uint16_t ch0, uint16_t ch1) const {
    if (ch0 == 0) return 0.0;
    
    // Simplified lux calculation based on TSL2561 datasheet
    float ratio = (float)ch1 / (float)ch0;
    float lux = 0;
    
    // Scale for integration time
    float scale = 1.0;
    if (m_integration == TSL2561_INTEGRATION_13MS) scale = 322.0 / 11.0;
    else if (m_integration == TSL2561_INTEGRATION_101MS) scale = 322.0 / 81.0;
    else if (m_integration == TSL2561_INTEGRATION_402MS) scale = 1.0;
    
    // Scale for gain
    if (m_gain == TSL2561_GAIN_1X) scale *= 16.0;
    
    // Calculate lux based on ratio
    if (ratio <= 0.50) {
        lux = (0.0304 * ch0) - (0.062 * ch0 * pow(ratio, 1.4));
    } else if (ratio <= 0.61) {
        lux = (0.0224 * ch0) - (0.031 * ch1);
    } else if (ratio <= 0.80) {
        lux = (0.0128 * ch0) - (0.0153 * ch1);
    } else if (ratio <= 1.30) {
        lux = (0.00146 * ch0) - (0.00112 * ch1);
    } else {
        lux = 0.0;
    }
    
    return lux * scale;
}

// String conversion helpers

String TSL2561Component::gainToString(uint8_t gain) const {
    return (gain == TSL2561_GAIN_1X) ? "1x" : "16x";
}

String TSL2561Component::integrationToString(uint8_t integration) const {
    switch (integration) {
        case TSL2561_INTEGRATION_13MS: return "13ms";
        case TSL2561_INTEGRATION_101MS: return "101ms";
        case TSL2561_INTEGRATION_402MS: return "402ms";
        default: return "unknown";
    }
}

uint8_t TSL2561Component::parseGain(const String& gainStr) const {
    return (gainStr == "16x") ? TSL2561_GAIN_16X : TSL2561_GAIN_1X;
}

uint8_t TSL2561Component::parseIntegrationTime(const String& integrationStr) const {
    if (integrationStr == "13ms") return TSL2561_INTEGRATION_13MS;
    if (integrationStr == "101ms") return TSL2561_INTEGRATION_101MS;
    return TSL2561_INTEGRATION_402MS;
}

// Remote reading using shared Orchestrator service

bool TSL2561Component::performRemoteReading() {
    if (m_remoteHost.length() == 0) {
        log(Logger::ERROR, "Remote host not configured");
        m_failedReadings++;
        return false;
    }
    
    String url = "http://" + m_remoteHost;
    if (m_remotePort != 80) {
        url += ":" + String(m_remotePort);
    }
    url += m_remotePath;
    
    log(Logger::DEBUG, "Fetching remote light data from: " + url);
    
    JsonDocument remoteData = fetchRemoteData(url, m_httpTimeoutMs);
    
    // Check if we should defer execution due to backoff
    if (remoteData.containsKey("shouldDefer") && remoteData["shouldDefer"].as<bool>()) {
        uint32_t retryInSeconds = remoteData["retryInSeconds"] | 0;
        String errorMsg = remoteData["error"] | "URL in backoff";
        
        log(Logger::INFO, "⏰ Remote sensor deferred for " + String(retryInSeconds) + "s: " + errorMsg);
        
        // Set next execution time to the suggested retry time via orchestrator
        if (remoteData.containsKey("nextRetryMs")) {
            uint32_t nextRetryMs = remoteData["nextRetryMs"];
            if (m_orchestrator && m_orchestrator->updateNextCheck(getId(), nextRetryMs)) {
                log(Logger::INFO, "🔄 Execution deferred via orchestrator to " + String(nextRetryMs));
            } else {
                // Fallback to direct setting if orchestrator update fails
                setNextExecutionMs(nextRetryMs);
                log(Logger::INFO, "🔄 Next execution deferred locally to " + String(nextRetryMs));
            }
        }
        
        // This is not a traditional failure, but a smart deferral
        // Don't increment failed readings counter for backoff scenarios
        return false;  // Still return false to indicate no new data
    }
    
    // Check if the remote fetch was successful
    bool hasSuccess = remoteData.containsKey("success") && remoteData["success"].as<bool>();
    bool hasError = remoteData.containsKey("error") && remoteData["error"].as<String>().length() > 0;
    
    if (!hasSuccess || hasError) {
        String errorMsg = hasError ? remoteData["error"].as<String>() : "No success flag in remote response";
        log(Logger::ERROR, "Remote sensor fetch failed: " + errorMsg);
        m_failedReadings++;
        return false;
    }
    
    return parseRemoteResponse(remoteData);
}

bool TSL2561Component::parseRemoteResponse(const String& jsonStr) {
    // This method is now called with parsed JsonDocument from performRemoteReading
    return true;
}

bool TSL2561Component::parseRemoteResponse(const JsonDocument& doc) {
    float remoteLux = doc["lux"] | NAN;
    uint16_t remoteBroadband = doc["broadband"] | 0;
    uint16_t remoteInfrared = doc["infrared"] | 0;
    
    if (!validateReadings(remoteLux, remoteBroadband, remoteInfrared)) {
        log(Logger::ERROR, "Remote sensor readings failed validation");
        m_failedReadings++;
        return false;
    }
    
    m_lastLux = remoteLux;
    m_lastBroadband = remoteBroadband;
    m_lastInfrared = remoteInfrared;
    m_lastReadingTime = millis();
    m_successfulReadings++;
    
    log(Logger::INFO, String("Remote light reading - Lux=") + m_lastLux);
    return true;
}

// Action System Implementation

std::vector<ComponentAction> TSL2561Component::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    
    ComponentAction readAction;
    readAction.name = "read_light";
    readAction.description = "Read current light levels";
    readAction.timeoutMs = 5000;
    readAction.requiresReady = false;
    actions.push_back(readAction);
    
    return actions;
}

ActionResult TSL2561Component::performAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.actionName = actionName;
    result.success = false;
    
    if (actionName == "read_light") {
        float lux = readLux();
        result.success = !isnan(lux) && lux >= 0;
        if (result.success) {
            result.message = "Light reading successful";
            result.data["lux"] = lux;
        } else {
            result.message = "Failed to read light sensor";
        }
    } else {
        result.message = "Unknown action: " + actionName;
    }
    
    return result;
}