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
#include <Wire.h>

// TSL2561 I2C addresses
#define TSL2561_ADDR_LOW  0x29
#define TSL2561_ADDR_FLOAT 0x39  
#define TSL2561_ADDR_HIGH 0x49

// TSL2561 registers
#define TSL2561_REGISTER_CONTROL   0x00
#define TSL2561_REGISTER_TIMING    0x01
#define TSL2561_REGISTER_CHAN0_LOW 0x0C
#define TSL2561_REGISTER_CHAN1_LOW 0x0E

// Control register values
#define TSL2561_CONTROL_POWERON    0x03
#define TSL2561_CONTROL_POWEROFF   0x00

// Timing register values (gain + integration time)
#define TSL2561_GAIN_1X            0x00
#define TSL2561_GAIN_16X           0x10
#define TSL2561_INTEGRATION_13MS   0x00
#define TSL2561_INTEGRATION_101MS  0x01  
#define TSL2561_INTEGRATION_402MS  0x02

/**
 * @brief TSL2561 light sensor component class
 * 
 * Provides light intensity readings from TSL2561 digital light sensor.
 * Uses schema-driven configuration with ESP32 I2C defaults.
 */
class TSL2561Component : public BaseComponent {
private:
    // Hardware - Direct I2C (no Adafruit library)
    uint8_t m_i2cAddress = TSL2561_ADDR_FLOAT; // TSL2561 I2C address (0x39)
    uint8_t m_sdaPin = 21;                     // ESP32 default SDA
    uint8_t m_sclPin = 22;                     // ESP32 default SCL
    uint32_t m_i2cFreq = 100000;              // 100kHz default
    
    // Configuration - Direct register values
    uint8_t m_gain = TSL2561_GAIN_1X;         // Default 1x gain
    uint8_t m_integration = TSL2561_INTEGRATION_402MS; // Default 402ms integration
    uint32_t m_samplingIntervalMs = 2000;     // 2 seconds default
    
    // Remote sensor configuration
    bool m_useRemoteSensor = false;                   // Use remote sensor instead of local I2C
    String m_remoteHost = "";                         // Remote sensor host IP/hostname
    uint16_t m_remotePort = 80;                       // Remote sensor port
    String m_remotePath = "/light";                   // Remote sensor endpoint path
    uint32_t m_httpTimeoutMs = 5000;                  // HTTP timeout
    
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

public:
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
     * @param gainStr Gain setting ("1x" or "16x")
     * @return true if gain set successfully
     */
    bool setGain(const String& gainStr);
    
    /**
     * @brief Set integration time
     * @param integrationStr Integration time ("13ms", "101ms", "402ms")
     * @return true if integration time set successfully
     */
    bool setIntegrationTime(const String& integrationStr);

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
     * @brief Convert gain register value to string
     * @param gain Gain register value (0x00 or 0x10)
     * @return Gain as string
     */
    String gainToString(uint8_t gain) const;
    
    /**
     * @brief Convert integration time register value to string
     * @param integration Integration time register value
     * @return Integration time as string
     */
    String integrationToString(uint8_t integration) const;
    
    /**
     * @brief Parse gain from string
     * @param gainStr Gain string ("1x" or "16x")
     * @return Gain register value
     */
    uint8_t parseGain(const String& gainStr) const;
    
    /**
     * @brief Parse integration time from string
     * @param integrationStr Integration time string
     * @return Integration time register value
     */
    uint8_t parseIntegrationTime(const String& integrationStr) const;
    
    /**
     * @brief Read from remote light sensor via HTTP (using shared Orchestrator service)
     * @return true if reading successful
     */
    bool performRemoteReading();
    
    /**
     * @brief Parse remote sensor JSON response
     * @param jsonStr JSON response from remote sensor
     * @return true if parsing successful
     */
    bool parseRemoteResponse(const String& jsonStr);
    
    /**
     * @brief Parse remote sensor JSON response from JsonDocument
     * @param doc Parsed JSON document from remote sensor
     * @return true if parsing successful
     */
    bool parseRemoteResponse(const JsonDocument& doc);
    
    // === Direct I2C Methods ===
    
    /**
     * @brief Write byte to TSL2561 register
     * @param reg Register address
     * @param value Value to write
     * @return true if write successful
     */
    bool writeRegister(uint8_t reg, uint8_t value);
    
    /**
     * @brief Read byte from TSL2561 register
     * @param reg Register address
     * @return Register value or 0xFF on error
     */
    uint8_t readRegister(uint8_t reg);
    
    /**
     * @brief Read 16-bit value from TSL2561 register pair
     * @param reg Low register address
     * @return 16-bit value or 0xFFFF on error
     */
    uint16_t readRegister16(uint8_t reg);
    
    /**
     * @brief Calculate lux from raw channel data
     * @param ch0 Broadband channel (visible + IR)
     * @param ch1 Infrared channel
     * @return Calculated lux value
     */
    float calculateLux(uint16_t ch0, uint16_t ch1) const;
};

#endif // TSL2561_COMPONENT_H