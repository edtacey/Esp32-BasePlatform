/**
 * @file TSL2561Component.h
 * @brief TSL2561 Digital Light Sensor Component
 * 
 * Implements TSL2561 light sensor with:
 * - Default I2C configuration (SDA=21, SCL=22, Address=0x39)
 * - Schema-driven configuration
 * - Lux and broadband/IR readings
 * - Configurable gain and integration time
 */

#ifndef TSL2561_COMPONENT_H
#define TSL2561_COMPONENT_H

#include "BaseComponent.h"
#include <Adafruit_TSL2561_U.h>
#include <Wire.h>

/**
 * @brief TSL2561 light sensor component class
 * 
 * Provides light intensity readings from TSL2561 digital light sensor.
 * Uses schema-driven configuration with ESP32 I2C defaults.
 */
class TSL2561Component : public BaseComponent {
private:
    // Hardware
    Adafruit_TSL2561_Unified* m_tsl = nullptr;
    uint8_t m_i2cAddress = TSL2561_ADDR_FLOAT;  // 0x39 default
    uint8_t m_sdaPin = 21;                      // ESP32 default SDA
    uint8_t m_sclPin = 22;                      // ESP32 default SCL
    uint32_t m_i2cFreq = 100000;               // 100kHz default
    
    // Configuration
    tsl2561Gain_t m_gain = TSL2561_GAIN_1X;           // Default gain
    tsl2561IntegrationTime_t m_integration = TSL2561_INTEGRATIONTIME_402MS; // Default integration
    uint32_t m_samplingIntervalMs = 2000;             // 2 seconds default
    
    // State
    float m_lastLux = NAN;
    uint16_t m_lastBroadband = 0;
    uint16_t m_lastInfrared = 0;
    uint32_t m_lastReadingTime = 0;
    bool m_sensorInitialized = false;
    bool m_i2cInitialized = false;
    
    // Statistics
    uint32_t m_successfulReadings = 0;
    uint32_t m_failedReadings = 0;
    uint32_t m_sensorId = 0;

public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name (optional)
     */
    TSL2561Component(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Destructor
     */
    virtual ~TSL2561Component();

    // === BaseComponent Interface ===
    
    /**
     * @brief Get default schema for TSL2561 sensor
     * @return Default schema with I2C pin 21/22 and address 0x39
     */
    JsonDocument getDefaultSchema() const override;
    
    /**
     * @brief Initialize TSL2561 sensor with configuration
     * @param config Configuration (empty for defaults)
     * @return true if initialization successful
     */
    bool initialize(const JsonDocument& config) override;
    
    /**
     * @brief Execute sensor reading
     * @return Execution result with lux and spectral data
     */
    ExecutionResult execute() override;
    
    /**
     * @brief Cleanup sensor resources
     */
    void cleanup() override;

    // === TSL2561 Specific Methods ===
    
    /**
     * @brief Read light intensity in lux
     * @return Light intensity in lux or NAN on error
     */
    float readLux();
    
    /**
     * @brief Read raw broadband and infrared values
     * @param broadband Output broadband value
     * @param infrared Output infrared value
     * @return true if reading successful
     */
    bool readRawValues(uint16_t& broadband, uint16_t& infrared);
    
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
     * @brief Get sensor information
     * @return Sensor info JSON document
     */
    JsonDocument getSensorInfo() const;
    
    /**
     * @brief Get reading statistics
     * @return Statistics JSON document
     */
    JsonDocument getReadingStats() const;

    // === Configuration Methods ===
    
    /**
     * @brief Set sensor gain
     * @param gain New gain setting
     * @return true if gain set successfully
     */
    bool setGain(tsl2561Gain_t gain);
    
    /**
     * @brief Set integration time
     * @param integration New integration time
     * @return true if integration time set successfully
     */
    bool setIntegrationTime(tsl2561IntegrationTime_t integration);

private:
    /**
     * @brief Apply configuration settings
     * @param config Configuration document
     * @return true if configuration applied successfully
     */
    bool applyConfiguration(const JsonDocument& config);
    
    /**
     * @brief Initialize I2C bus
     * @return true if I2C initialized
     */
    bool initializeI2C();
    
    /**
     * @brief Initialize TSL2561 sensor
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
     * @param lux Lux reading
     * @param broadband Broadband reading
     * @param infrared Infrared reading
     * @return true if readings are valid
     */
    bool validateReadings(float lux, uint16_t broadband, uint16_t infrared) const;
    
    /**
     * @brief Convert gain enum to string
     * @param gain Gain setting
     * @return Gain as string
     */
    String gainToString(tsl2561Gain_t gain) const;
    
    /**
     * @brief Convert integration time enum to string
     * @param integration Integration time setting
     * @return Integration time as string
     */
    String integrationToString(tsl2561IntegrationTime_t integration) const;
    
    /**
     * @brief Parse gain from string
     * @param gainStr Gain string
     * @return Gain enum value
     */
    tsl2561Gain_t parseGain(const String& gainStr) const;
    
    /**
     * @brief Parse integration time from string
     * @param integrationStr Integration time string
     * @return Integration time enum value
     */
    tsl2561IntegrationTime_t parseIntegrationTime(const String& integrationStr) const;
};

#endif // TSL2561_COMPONENT_H