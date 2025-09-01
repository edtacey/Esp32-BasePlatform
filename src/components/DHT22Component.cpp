/**
 * @file DHT22Component.cpp
 * @brief DHT22Component implementation
 */

#include "DHT22Component.h"

DHT22Component::DHT22Component(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "DHT22", name, storage, orchestrator)
{
    log(Logger::DEBUG, "DHT22Component created");
}

DHT22Component::~DHT22Component() {
    cleanup();
}

JsonDocument DHT22Component::getDefaultSchema() const {
    JsonDocument schema;
    
    // Schema metadata
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["title"] = "DHT22 Temperature/Humidity Sensor";
    schema["description"] = "DHT22/AM2302 sensor configuration schema";
    schema["type"] = "object";
    
    // No required fields with default hydration architecture
    // All fields have defaults and are applied automatically
    
    // Properties with defaults
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // Pin configuration (default: 15)
    JsonObject pinProp = properties["pin"].to<JsonObject>();
    pinProp["type"] = "integer";
    pinProp["minimum"] = 0;
    pinProp["maximum"] = 39;  // ESP32 GPIO range
    pinProp["default"] = 15;
    pinProp["description"] = "GPIO pin connected to DHT22 data line";
    
    // Sampling interval (default: 5000ms)
    JsonObject intervalProp = properties["samplingIntervalMs"].to<JsonObject>();
    intervalProp["type"] = "integer";
    intervalProp["minimum"] = 2000;  // DHT22 minimum is ~2 seconds
    intervalProp["maximum"] = 3600000;  // 1 hour max
    intervalProp["default"] = 5000;
    intervalProp["description"] = "Sampling interval in milliseconds";
    
    // Temperature unit (default: false = Celsius)
    JsonObject fahrenheitProp = properties["fahrenheit"].to<JsonObject>();
    fahrenheitProp["type"] = "boolean";
    fahrenheitProp["default"] = false;
    fahrenheitProp["description"] = "If true, report temperature in Fahrenheit";
    
    // Sensor name/label
    JsonObject nameProp = properties["sensorName"].to<JsonObject>();
    nameProp["type"] = "string";
    nameProp["maxLength"] = 64;
    nameProp["default"] = "DHT22 Sensor";
    nameProp["description"] = "Human-readable sensor name";
    
    // Sensor type (DHT11 or DHT22)
    JsonObject typeProp = properties["sensorType"].to<JsonObject>();
    typeProp["type"] = "integer";
    typeProp["minimum"] = 11;
    typeProp["maximum"] = 22;
    typeProp["default"] = 11;  // Default to DHT11
    typeProp["description"] = "Sensor type: 11 for DHT11, 22 for DHT22";
    
    log(Logger::DEBUG, "Generated default schema with pin=15, interval=5000ms, type=DHT11");
    return schema;
}

bool DHT22Component::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing DHT22 sensor...");
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
    
    // Initialize sensor hardware
    if (!initializeSensor()) {
        setError("Failed to initialize DHT22 sensor");
        return false;
    }
    
    // Set initial execution time
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    
    setState(ComponentState::READY);
    log(Logger::INFO, String("DHT11/22 initialized on pin ") + m_pin + 
                      ", interval=" + m_samplingIntervalMs + "ms");
    
    return true;
}

ExecutionResult DHT22Component::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    
    log(Logger::DEBUG, "Executing DHT22 reading...");
    setState(ComponentState::EXECUTING);
    
    // Perform the sensor reading
    bool success = performReading();
    
    // Prepare result data
    JsonDocument data;
    data["timestamp"] = millis();
    data["pin"] = m_pin;
    data["success"] = success;
    
    if (success) {
        data["temperature"] = m_lastTemperature;
        data["humidity"] = m_lastHumidity;
        data["heatIndex"] = m_lastHeatIndex;
        data["unit"] = m_fahrenheit ? "F" : "C";
        
        result.success = true;
        result.message = "Reading successful";
        
        log(Logger::INFO, String("T=") + m_lastTemperature + 
                          (m_fahrenheit ? "°F" : "°C") + 
                          ", H=" + m_lastHumidity + "%");
    } else {
        data["error"] = "Sensor reading failed";
        result.success = false;
        result.message = "Sensor reading failed";
        
        log(Logger::WARNING, "Sensor reading failed");
    }
    
    // Update statistics
    result.executionTimeMs = millis() - startTime;
    result.data = data;
    
    // Store last execution data for API/dashboard access
    // Deep copy the data using serialize/deserialize to ensure it persists
    String dataStr;
    serializeJson(data, dataStr);
    storeExecutionDataString(dataStr);  // Store as string for API access
    m_lastData.clear();
    deserializeJson(m_lastData, dataStr);
    
    // Schedule next execution
    setNextExecutionMs(millis() + m_samplingIntervalMs);
    
    // Update component stats
    updateExecutionStats();
    
    setState(ComponentState::READY);
    return result;
}

void DHT22Component::cleanup() {
    log(Logger::DEBUG, "Cleaning up DHT22 sensor");
    
    if (m_dht) {
        delete m_dht;
        m_dht = nullptr;
    }
    
    m_sensorInitialized = false;
}

float DHT22Component::readTemperature(bool fahrenheit) {
    if (!m_sensorInitialized || !m_dht) {
        return NAN;
    }
    
    return m_dht->readTemperature(fahrenheit);
}

float DHT22Component::readHumidity() {
    if (!m_sensorInitialized || !m_dht) {
        return NAN;
    }
    
    return m_dht->readHumidity();
}

float DHT22Component::readHeatIndex(bool fahrenheit) {
    if (!m_sensorInitialized || !m_dht) {
        return NAN;
    }
    
    float temp = readTemperature(fahrenheit);
    float hum = readHumidity();
    
    if (isnan(temp) || isnan(hum)) {
        return NAN;
    }
    
    return m_dht->computeHeatIndex(temp, hum, fahrenheit);
}

JsonDocument DHT22Component::getLastReadings() const {
    JsonDocument readings;
    
    readings["temperature"] = m_lastTemperature;
    readings["humidity"] = m_lastHumidity;
    readings["heatIndex"] = m_lastHeatIndex;
    readings["unit"] = m_fahrenheit ? "F" : "C";
    readings["timestamp"] = m_lastReadingTime;
    readings["valid"] = !isnan(m_lastTemperature) && !isnan(m_lastHumidity);
    
    return readings;
}

bool DHT22Component::isSensorHealthy() const {
    // Consider sensor healthy if we've had successful readings recently
    // and the failure rate isn't too high
    if (m_successfulReadings == 0) return false;
    
    uint32_t totalReadings = m_successfulReadings + m_failedReadings;
    if (totalReadings == 0) return false;
    
    float successRate = (float)m_successfulReadings / totalReadings;
    return successRate >= 0.7f;  // 70% success rate threshold
}

JsonDocument DHT22Component::getReadingStats() const {
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

// Private methods

JsonDocument DHT22Component::getCurrentConfig() const {
    JsonDocument config;
    
    config["pin"] = m_pin;
    config["samplingIntervalMs"] = m_samplingIntervalMs;
    config["fahrenheit"] = m_fahrenheit;
    config["sensorType"] = m_sensorType;
    config["sensorName"] = "DHT22 Sensor"; // Fixed value for now
    config["config_version"] = 1;
    
    return config;
}

bool DHT22Component::applyConfig(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying DHT22 configuration with default hydration");
    
    // Apply configuration with default hydration
    m_pin = config["pin"] | 15;
    m_samplingIntervalMs = config["samplingIntervalMs"] | 5000;
    m_fahrenheit = config["fahrenheit"] | false;
    m_sensorType = config["sensorType"] | 11;
    
    // Handle version migration if needed
    uint16_t configVersion = config["config_version"] | 1;
    if (configVersion < 1) {
        log(Logger::INFO, "Migrating DHT22 configuration to version 1");
        // Future migration logic would go here
    }
    
    log(Logger::DEBUG, String("Config applied: pin=") + m_pin + 
                       ", interval=" + m_samplingIntervalMs + "ms" +
                       ", fahrenheit=" + (m_fahrenheit ? "true" : "false") +
                       ", type=DHT" + m_sensorType);
    
    return true;
}

bool DHT22Component::applyConfiguration(const JsonDocument& config) {
    // Legacy method - now delegates to new architecture
    return applyConfig(config);
}

bool DHT22Component::initializeSensor() {
    log(Logger::DEBUG, String("Initializing DHT") + m_sensorType + " sensor on pin " + m_pin);
    
    // Clean up existing sensor if any
    if (m_dht) {
        delete m_dht;
        m_dht = nullptr;
    }
    
    // Create new DHT sensor instance based on configured type
    uint8_t dhtType = (m_sensorType == 22) ? DHT22 : DHT11;
    m_dht = new DHT(m_pin, dhtType);
    if (!m_dht) {
        log(Logger::ERROR, "Failed to create DHT sensor instance");
        return false;
    }
    
    // Initialize the sensor
    m_dht->begin();
    
    // Wait for sensor to stabilize
    delay(2000);
    
    // Test read to verify sensor is working
    float testTemp = m_dht->readTemperature();
    float testHum = m_dht->readHumidity();
    
    if (isnan(testTemp) || isnan(testHum)) {
        log(Logger::WARNING, "Initial sensor test read failed - sensor may not be connected");
        // Don't fail initialization - sensor might just need more time
    } else {
        log(Logger::INFO, String("Sensor test read successful: T=") + testTemp + 
                          "°C, H=" + testHum + "%");
    }
    
    m_sensorInitialized = true;
    return true;
}

bool DHT22Component::performReading() {
    if (!m_sensorInitialized || !m_dht) {
        m_failedReadings++;
        return false;
    }
    
    // Read temperature and humidity
    float temperature = m_dht->readTemperature(m_fahrenheit);
    float humidity = m_dht->readHumidity();
    
    // Validate readings
    if (!validateReadings(temperature, humidity)) {
        m_failedReadings++;
        return false;
    }
    
    // Calculate heat index
    float heatIndex = m_dht->computeHeatIndex(temperature, humidity, m_fahrenheit);
    
    // Store readings
    m_lastTemperature = temperature;
    m_lastHumidity = humidity;
    m_lastHeatIndex = heatIndex;
    m_lastReadingTime = millis();
    m_successfulReadings++;
    
    return true;
}

bool DHT22Component::validateReadings(float temp, float hum) const {
    // Check for NaN values
    if (isnan(temp) || isnan(hum)) {
        return false;
    }
    
    // Check temperature range (DHT22 specs: -40 to 80°C)
    if (m_fahrenheit) {
        if (temp < -40 || temp > 176) {  // Fahrenheit range
            return false;
        }
    } else {
        if (temp < -40 || temp > 80) {   // Celsius range
            return false;
        }
    }
    
    // Check humidity range (DHT22 specs: 0 to 100% RH)
    if (hum < 0 || hum > 100) {
        return false;
    }
    
    return true;
}

// === Action System Implementation ===

std::vector<ComponentAction> DHT22Component::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    
    // Read Status Action
    ComponentAction statusAction;
    statusAction.name = "read_sensor";
    statusAction.description = "Read current temperature and humidity";
    statusAction.timeoutMs = 5000;
    statusAction.requiresReady = false;
    actions.push_back(statusAction);
    
    return actions;
}

ActionResult DHT22Component::performAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.actionName = actionName;
    result.success = false;
    
    if (actionName == "read_sensor") {
        float temp = readTemperature();
        float humid = readHumidity();
        
        result.success = !isnan(temp) && !isnan(humid);
        if (result.success) {
            result.message = "Sensor reading successful";
            result.data["temperature"] = temp;
            result.data["humidity"] = humid;
            result.data["heat_index"] = readHeatIndex();
            result.data["unit"] = "C";
        } else {
            result.message = "Failed to read sensor data";
        }
    } else {
        result.message = "Unknown action: " + actionName;
    }
    
    return result;
}

JsonDocument DHT22Component::getCoreData() const {
    JsonDocument coreData;
    
    // For DHT22 sensors, core data is temperature, humidity, and success status
    if (!m_lastData.isNull() && m_lastData.size() > 0) {
        // Extract only the essential sensor values
        if (!m_lastData["temperature"].isNull()) {
            coreData["temperature"] = m_lastData["temperature"];
        }
        if (!m_lastData["humidity"].isNull()) {
            coreData["humidity"] = m_lastData["humidity"];
        }
        if (!m_lastData["heatIndex"].isNull()) {
            coreData["heat_index"] = m_lastData["heatIndex"];
        }
        if (!m_lastData["unit"].isNull()) {
            coreData["unit"] = m_lastData["unit"];
        }
        if (!m_lastData["success"].isNull()) {
            coreData["success"] = m_lastData["success"];
        }
        if (!m_lastData["timestamp"].isNull()) {
            coreData["timestamp"] = m_lastData["timestamp"];
        }
        
        log(Logger::DEBUG, String("[CORE-DATA] DHT22 ") + m_componentId + " returning " + 
            String(coreData.size()) + " core fields");
    } else {
        // No data available
        coreData["status"] = "No data available";
        log(Logger::DEBUG, String("[CORE-DATA] DHT22 ") + m_componentId + " has no data");
    }
    
    return coreData;
}