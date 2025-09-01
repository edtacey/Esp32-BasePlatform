#pragma once

#include "BaseComponent.h"
#include <vector>

/**
 * @brief pH sensor operational modes
 */
enum class PHSensorMode {
    SLEEPING = 0,       // Sensor inactive/powered down
    READ_NORMAL = 1,    // Normal pH measurement mode
    READ_CALIBRATE = 2, // Calibration measurement mode
    MOCK = 3           // Mock mode (when pin is 0/null)
};

/**
 * @brief pH calibration point structure
 */
struct PHCalibrationPoint {
    float voltage = 0.0f;      // Measured voltage at this pH
    float ph = 0.0f;           // Known pH value from calibration solution
    bool isValid = false;      // Whether this calibration point is set
    uint32_t timestampMs = 0;  // When this calibration was performed
};

/**
 * @brief pH sensor component for precision pH measurement
 * 
 * Supports 3-point calibration (pH 4.0, 7.0, 10.0), temperature compensation,
 * sample averaging, and real-time pH monitoring with configurable precision.
 */
class PHSensorComponent : public BaseComponent {
public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for component access
     */
    PHSensorComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Destructor
     */
    ~PHSensorComponent() override;

    // Required BaseComponent implementations
    JsonDocument getDefaultSchema() const override;
    bool initialize(const JsonDocument& config) override;
    ExecutionResult execute() override;
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
    
    /**
     * @brief Get supported actions for pH sensor
     * @return Vector of supported actions with parameter definitions
     */
    std::vector<ComponentAction> getSupportedActions() const override;
    
    /**
     * @brief Execute pH sensor action with validated parameters
     * @param actionName Name of action to execute
     * @param parameters Validated JSON parameters
     * @return Action execution result
     */
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;
    
    // === Data Filtering (BaseComponent virtual methods) ===
    JsonDocument getCoreData() const override;

public:
    // pH sensor specific methods
    /**
     * @brief Perform 3-point pH calibration
     * @param ph4_voltage Voltage reading at pH 4.0 calibration solution
     * @param ph7_voltage Voltage reading at pH 7.0 calibration solution  
     * @param ph10_voltage Voltage reading at pH 10.0 calibration solution
     * @return true if calibration successful
     */
    bool calibrate(float ph4_voltage, float ph7_voltage, float ph10_voltage);
    
    /**
     * @brief Perform single-point calibration update
     * @param ph_value Known pH value of calibration solution
     * @param voltage Measured voltage for this pH
     * @return true if calibration point updated
     */
    bool calibratePoint(float ph_value, float voltage);
    
    /**
     * @brief Clear all calibration data
     * @return true if calibration cleared
     */
    bool clearCalibration();
    
    /**
     * @brief Take a raw voltage reading
     * @return Raw voltage from ADC
     */
    float readRawVoltage();
    
    /**
     * @brief Get current pH reading with temperature compensation
     * @param temperature_c Current temperature in Celsius (optional)
     * @return pH value or -1 if error
     */
    float getCurrentPH(float temperature_c = 25.0f);
    
    /**
     * @brief Check if sensor is properly calibrated
     * @return true if at least 2 calibration points are valid
     */
    bool isCalibrated() const;
    
    // === State Output Getters ===
    
    /**
     * @brief Get current voltage reading
     * @return Current voltage in volts
     */
    float getCurrentVolts() const { return m_currentVolts; }
    
    /**
     * @brief Get current temperature used for compensation
     * @return Current temperature in Celsius
     */
    float getCurrentTemp() const { return m_currentTemp; }
    
    /**
     * @brief Get current pH reading
     * @return Current pH value
     */
    float getCurrentPH() const { return m_currentPH; }
    
    /**
     * @brief Get sample size for averaging
     * @return Number of samples averaged
     */
    uint16_t getSampleSize() const { return m_sampleSize; }
    
    /**
     * @brief Get last readings array
     * @return Vector of recent voltage readings
     */
    const std::vector<float>& getLastReads() const { return m_lastReads; }
    
    /**
     * @brief Get current sensor operational mode
     * @return Current PHSensorMode
     */
    PHSensorMode getCurrentMode() const { return m_currentMode; }

private:
    // === Persisted Configuration Parameters ===
    uint8_t m_gpioPin = 36;                     // ADC GPIO pin for pH sensor (0 = mock mode)
    float m_tempCoefficient = -0.0198f;         // pH/°C temperature coefficient (typical: -0.0198)
    uint16_t m_sampleSize = 10;                 // Number of samples for weighted average
    float m_adcVoltageRef = 3.3f;               // ADC reference voltage
    uint16_t m_adcResolution = 4096;            // ADC resolution (12-bit = 4096)
    uint32_t m_readingIntervalMs = 1000;        // Time between readings (1 second)
    uint32_t m_timePeriodForSampling = 10000;   // Sampling window duration (10 seconds default)
    float m_outlierThreshold = 2.0f;            // Standard deviations for outlier detection
    String m_temperatureSourceId = "";          // Component ID for temperature readings
    String m_exciteVoltageComponentId = "";     // Component ID for excitation voltage control
    uint32_t m_exciteStabilizeMs = 500;         // Time to wait after excitation on (500ms)
    
    // === Calibration Data ===
    PHCalibrationPoint m_calibrationPoints[3]; // pH 4.0, 7.0, 10.0 calibration points
    uint32_t m_lastCalibrationMs = 0;           // Last calibration timestamp
    bool m_calibrationValid = false;            // Whether calibration is valid
    
    // === Sample Averaging ===
    std::vector<float> m_lastReads;             // Array of last X voltage readings
    uint16_t m_readIndex = 0;                   // Current position in circular buffer
    bool m_bufferFull = false;                  // Whether buffer has been filled once
    
    // === State Output Fields ===
    PHSensorMode m_currentMode = PHSensorMode::SLEEPING;  // Current sensor operational mode
    float m_currentVolts = 0.0f;                // Current voltage reading
    float m_currentTemp = 25.0f;                // Current temperature for compensation
    float m_currentPH = -1.0f;                  // Current pH reading (-1 = invalid)
    
    // === Runtime State ===
    uint32_t m_lastReadingMs = 0;               // Last reading timestamp
    uint32_t m_samplingStartMs = 0;             // Start of current sampling window
    uint32_t m_samplingEndMs = 0;               // End of current sampling window
    bool m_samplingActive = false;              // Whether sampling window is active
    uint32_t m_exciteOnMs = 0;                  // When excitation voltage was turned on
    bool m_exciteVoltageOn = false;             // Whether excitation voltage is currently on
    uint32_t m_totalReadings = 0;               // Total readings taken
    uint32_t m_errorCount = 0;                  // Number of read errors
    uint32_t m_outliersRemoved = 0;             // Number of outliers removed
    float m_minRecordedPH = 14.0f;              // Minimum pH recorded
    float m_maxRecordedPH = 0.0f;               // Maximum pH recorded
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    bool initializeSensor();
    float calculateWeightedAverage() const;
    float calculateWeightedAverageWithOutlierRemoval();
    void addReading(float voltage);
    bool isOutlier(float value, const std::vector<float>& data) const;
    void removeOutliers();
    float calculateStandardDeviation(const std::vector<float>& data) const;
    float calculateMean(const std::vector<float>& data) const;
    void startSamplingWindow();
    void endSamplingWindow();
    bool isSamplingWindowComplete() const;
    uint32_t calculateNextExecutionTime() const;
    bool controlExcitationVoltage(bool enable);
    bool isExcitationStabilized() const;
    void updateSensorMode();
    String getModeString(PHSensorMode mode) const;
    float convertVoltageToPH(float voltage, float temperature_c = 25.0f) const;
    float getTemperatureReading();
    bool updateCalibrationFromConfig(const JsonDocument& config);
    void logCalibrationStatus();
    bool validateCalibrationPoints() const;
    float interpolatePH(float voltage) const;
    BaseComponent* getTemperatureComponent();
    BaseComponent* getExciteVoltageComponent();
};