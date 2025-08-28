#pragma once

#include "BaseComponent.h"
#include <vector>

/**
 * @brief EC probe operational modes
 */
enum class ECProbeMode {
    SLEEPING = 0,       // Sensor inactive/powered down
    READ_NORMAL = 1,    // Normal EC measurement mode
    READ_CALIBRATE = 2, // Calibration measurement mode
    MOCK = 3           // Mock mode (when pin is 0/null)
};

/**
 * @brief EC calibration point structure
 */
struct ECCalibrationPoint {
    float voltage = 0.0f;      // Measured voltage at this conductivity
    float ec_us_cm = 0.0f;     // Known EC value in µS/cm from calibration solution
    bool isValid = false;      // Whether this calibration point is set
    uint32_t timestampMs = 0;  // When this calibration was performed
};

/**
 * @brief Electrical Conductivity (EC) probe component for precision conductivity measurement
 * 
 * Supports multi-point calibration (dry, low, high conductivity solutions), 
 * temperature compensation, sample averaging with outlier detection, and 
 * real-time EC/TDS monitoring with configurable precision.
 */
class ECProbeComponent : public BaseComponent {
public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for component access
     */
    ECProbeComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Destructor
     */
    ~ECProbeComponent() override;

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
     * @brief Get supported actions for EC probe
     * @return Vector of supported actions with parameter definitions
     */
    std::vector<ComponentAction> getSupportedActions() const override;
    
    /**
     * @brief Execute EC probe action with validated parameters
     * @param actionName Name of action to execute
     * @param parameters Validated JSON parameters
     * @return Action execution result
     */
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;

public:
    // EC probe specific methods
    /**
     * @brief Perform 3-point EC calibration
     * @param dry_voltage Voltage reading in air (dry calibration)
     * @param low_ec_voltage Voltage reading at low EC solution (typically 84 µS/cm)
     * @param high_ec_voltage Voltage reading at high EC solution (typically 1413 µS/cm)
     * @param low_ec_value Known EC value of low calibration solution
     * @param high_ec_value Known EC value of high calibration solution
     * @return true if calibration successful
     */
    bool calibrate(float dry_voltage, float low_ec_voltage, float high_ec_voltage, 
                   float low_ec_value = 84.0f, float high_ec_value = 1413.0f);
    
    /**
     * @brief Perform single-point calibration update
     * @param ec_value Known EC value of calibration solution (µS/cm)
     * @param voltage Measured voltage for this EC
     * @return true if calibration point updated
     */
    bool calibratePoint(float ec_value, float voltage);
    
    /**
     * @brief Clear all calibration data
     * @return true if calibration cleared
     */
    bool clearCalibration();
    
    /**
     * @brief Take a raw voltage reading from EC probe
     * @return Raw voltage from ADC
     */
    float readRawVoltage();
    
    /**
     * @brief Get current EC reading with temperature compensation
     * @param temperature_c Current temperature in Celsius (optional)
     * @return EC value in µS/cm or -1 if error
     */
    float getCurrentEC(float temperature_c = 25.0f);
    
    /**
     * @brief Get current TDS (Total Dissolved Solids) reading
     * @param temperature_c Current temperature in Celsius (optional)
     * @return TDS value in ppm or -1 if error
     */
    float getCurrentTDS(float temperature_c = 25.0f);
    
    /**
     * @brief Check if probe is properly calibrated
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
     * @brief Get current EC reading
     * @return Current EC value in µS/cm
     */
    float getCurrentECValue() const { return m_currentEC; }
    
    /**
     * @brief Get current TDS reading
     * @return Current TDS value in ppm
     */
    float getCurrentTDSValue() const { return m_currentTDS; }
    
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
     * @return Current ECProbeMode
     */
    ECProbeMode getCurrentMode() const { return m_currentMode; }

private:
    // === Persisted Configuration Parameters ===
    uint8_t m_gpioPin = 35;                     // ADC GPIO pin for EC probe (0 = mock mode)
    float m_tempCoefficient = 2.0f;             // %/°C temperature coefficient (typical: 2.0%/°C)
    uint16_t m_sampleSize = 15;                 // Number of samples for weighted average (EC needs more stability)
    float m_adcVoltageRef = 3.3f;               // ADC reference voltage
    uint16_t m_adcResolution = 4096;            // ADC resolution (12-bit = 4096)
    uint32_t m_readingIntervalMs = 800;         // Time between readings (800ms for EC stability)
    uint32_t m_timePeriodForSampling = 15000;   // Sampling window duration (15 seconds for EC)
    float m_outlierThreshold = 2.5f;            // Standard deviations for outlier detection (more conservative)
    float m_tdsConversionFactor = 0.64f;        // TDS conversion factor (EC µS/cm * factor = TDS ppm)
    String m_temperatureSourceId = "";          // Component ID for temperature readings
    String m_exciteVoltageComponentId = "";     // Component ID for excitation voltage control
    uint32_t m_exciteStabilizeMs = 1000;        // Time to wait after excitation on (1000ms for EC)
    
    // === Calibration Data ===
    ECCalibrationPoint m_calibrationPoints[3]; // Dry (0), Low EC, High EC calibration points
    uint32_t m_lastCalibrationMs = 0;           // Last calibration timestamp
    bool m_calibrationValid = false;            // Whether calibration is valid
    
    // === Sample Averaging ===
    std::vector<float> m_lastReads;             // Array of last X voltage readings
    uint16_t m_readIndex = 0;                   // Current position in circular buffer
    bool m_bufferFull = false;                  // Whether buffer has been filled once
    
    // === State Output Fields ===
    ECProbeMode m_currentMode = ECProbeMode::SLEEPING;  // Current sensor operational mode
    float m_currentVolts = 0.0f;                // Current voltage reading
    float m_currentTemp = 25.0f;                // Current temperature for compensation
    float m_currentEC = -1.0f;                  // Current EC reading in µS/cm (-1 = invalid)
    float m_currentTDS = -1.0f;                 // Current TDS reading in ppm (-1 = invalid)
    
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
    float m_minRecordedEC = 50000.0f;           // Minimum EC recorded
    float m_maxRecordedEC = 0.0f;               // Maximum EC recorded
    
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
    String getModeString(ECProbeMode mode) const;
    float convertVoltageToEC(float voltage, float temperature_c = 25.0f) const;
    float convertECtoTDS(float ec_us_cm) const;
    float getTemperatureReading();
    bool updateCalibrationFromConfig(const JsonDocument& config);
    void logCalibrationStatus();
    bool validateCalibrationPoints() const;
    float interpolateEC(float voltage) const;
    BaseComponent* getTemperatureComponent();
    BaseComponent* getExciteVoltageComponent();
};