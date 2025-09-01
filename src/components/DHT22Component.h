/**
 * @file DHT22Component.h
 * @brief DHT22 Temperature and Humidity Sensor Component
 * 
 * Implements DHT22/AM2302 sensor with:
 * - Default pin 15 configuration
 * - Schema-driven configuration
 * - Temperature and humidity readings
 * - Configurable sampling rate
 */

#ifndef DHT22_COMPONENT_H
#define DHT22_COMPONENT_H

#include "BaseComponent.h"
#include <DHT.h>

/**
 * @brief DHT22 sensor component class
 * 
 * Provides temperature and humidity readings from DHT22/AM2302 sensors.
 * Uses schema-driven configuration with sensible defaults for ESP32.
 */
class DHT22Component : public BaseComponent {
private:
    // Hardware
    DHT* m_dht = nullptr;
    uint8_t m_pin = 15;  // Default pin
    
    // Configuration
    uint32_t m_samplingIntervalMs = 5000;  // 5 seconds default
    bool m_fahrenheit = false;             // Celsius default
    uint8_t m_sensorType = 11;             // DHT11 default (11 or 22)
    
    // State
    float m_lastTemperature = NAN;
    float m_lastHumidity = NAN;
    float m_lastHeatIndex = NAN;
    uint32_t m_lastReadingTime = 0;
    bool m_sensorInitialized = false;
    
    // Statistics
    uint32_t m_successfulReadings = 0;
    uint32_t m_failedReadings = 0;

public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name (optional)
     */
    DHT22Component(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Destructor
     */
    virtual ~DHT22Component();

    // === BaseComponent Interface ===
    
    /**
     * @brief Get default schema for DHT22 sensor
     * @return Default schema with pin 15 and 5-second interval
     */
    JsonDocument getDefaultSchema() const override;
    
    /**
     * @brief Initialize DHT22 sensor with configuration
     * @param config Configuration (empty for defaults)
     * @return true if initialization successful
     */
    bool initialize(const JsonDocument& config) override;
    
    /**
     * @brief Execute sensor reading
     * @return Execution result with temperature/humidity data
     */
    ExecutionResult execute() override;
    
    /**
     * @brief Cleanup sensor resources
     */
    void cleanup() override;

protected:
    // === Configuration Management (BaseComponent virtual methods) ===
    
    /**
     * @brief Get current component configuration as JSON
     * @return JsonDocument containing all current settings
     */
    JsonDocument getCurrentConfig() const override;
    
    /**
     * @brief Apply configuration to component variables
     * @param config Configuration to apply (may be empty for defaults)
     * @return true if configuration applied successfully
     */
    bool applyConfig(const JsonDocument& config) override;

    // === Action System (BaseComponent virtual methods) ===
    std::vector<ComponentAction> getSupportedActions() const override;
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;
    
    // === Data Filtering (BaseComponent virtual methods) ===
    JsonDocument getCoreData() const override;

public:
    // === DHT22 Specific Methods ===
    
    /**
     * @brief Read temperature
     * @param fahrenheit If true, return Fahrenheit; otherwise Celsius
     * @return Temperature value or NAN on error
     */
    float readTemperature(bool fahrenheit = false);
    
    /**
     * @brief Read humidity
     * @return Humidity percentage or NAN on error
     */
    float readHumidity();
    
    /**
     * @brief Calculate heat index
     * @param fahrenheit If true, calculate in Fahrenheit
     * @return Heat index or NAN on error
     */
    float readHeatIndex(bool fahrenheit = false);
    
    /**
     * @brief Get last successful readings
     * @return JSON document with last values
     */
    JsonDocument getLastReadings() const;
    
    /**
     * @brief Check if sensor is functioning
     * @return true if sensor responsive
     */
    bool isSensorHealthy() const;
    
    /**
     * @brief Get reading statistics
     * @return Statistics JSON document
     */
    JsonDocument getReadingStats() const;

private:
    /**
     * @brief Apply configuration settings
     * @param config Configuration document
     * @return true if configuration applied successfully
     */
    bool applyConfiguration(const JsonDocument& config);
    
    /**
     * @brief Initialize DHT sensor hardware
     * @return true if sensor initialized
     */
    bool initializeSensor();
    
    /**
     * @brief Perform sensor reading with error handling
     * @return true if reading successful
     */
    bool performReading();
    
    /**
     * @brief Validate sensor readings
     * @param temp Temperature reading
     * @param hum Humidity reading
     * @return true if readings are valid
     */
    bool validateReadings(float temp, float hum) const;
};

#endif // DHT22_COMPONENT_H