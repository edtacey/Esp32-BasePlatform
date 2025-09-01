#pragma once

#include "BaseComponent.h"
#include <vector>

/**
 * @brief Light orchestrator component for intelligent lighting control
 * 
 * Manages automatic lighting adjustment based on multiple light sensors,
 * time-based lighting modes, and target lumen requirements. Controls
 * servo dimmers to achieve optimal lighting conditions.
 */
class LightOrchestrator : public BaseComponent {
public:
    /**
     * @brief Lighting modes for different times of day
     */
    enum class LightingMode {
        MORNING = 0,    // Dawn/early morning lighting
        NOON = 1,       // Midday bright lighting
        TWILIGHT = 2,   // Evening/dusk lighting
        NIGHT = 3       // Night/low lighting
    };

    /**
     * @brief Light sensor configuration
     */
    struct LightSensor {
        String componentId;
        String name;
        bool isValid = false;
        float lastReading = 0.0f;
        uint32_t lastUpdateMs = 0;
        float weight = 1.0f;  // Sensor weighting for averaging
    };

    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for component access
     */
    LightOrchestrator(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator);
    
    /**
     * @brief Destructor
     */
    ~LightOrchestrator() override;

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

public:
    // Public API methods
    /**
     * @brief Set target lumens for current lighting mode
     * @param lumens Target lumen level
     * @return true if target set successfully
     */
    bool setTargetLumens(float lumens);
    
    /**
     * @brief Set lighting mode
     * @param mode New lighting mode
     * @return true if mode set successfully
     */
    bool setLightingMode(LightingMode mode);
    
    /**
     * @brief Get current target lumens
     * @return Current target lumen level
     */
    float getTargetLumens() const { return m_targetLumens; }
    
    /**
     * @brief Get current lighting mode
     * @return Current lighting mode
     */
    LightingMode getLightingMode() const { return m_lightingMode; }
    
    /**
     * @brief Get average sensor reading
     * @return Weighted average of all valid sensors
     */
    float getAverageSensorReading() const;
    
    /**
     * @brief Start light sweep test (0% to 100% in 5% increments, then back to 0%)
     * @return true if sweep test started successfully
     */
    bool startSweepTest();
    
    /**
     * @brief Stop active sweep test and return to normal operation
     * @return true if sweep test stopped
     */
    bool stopSweepTest();
    
    /**
     * @brief Check if sweep test is currently running
     * @return true if sweep test is active
     */
    bool isSweepTestActive() const { return m_sweepTestActive; }

private:
    // Configuration parameters
    float m_defMaxLumens = 1000.0f;         // Maximum target lumens
    float m_defMinLumens = 50.0f;           // Minimum target lumens
    float m_targetLumens = 500.0f;          // Current target lumens
    uint32_t m_samplePoolSize = 5;          // Number of readings to average
    float m_incrementStep = 0.10f;          // 10% increment step for adjustments
    uint32_t m_adjustmentIntervalMs = 10000; // Time between adjustments (10s)
    uint32_t m_sensorTimeoutMs = 30000;     // Sensor reading timeout (30s)
    
    // Light sensor configuration
    std::vector<LightSensor> m_lightSensors; // Array of light sensors
    int m_currentSensorIndex = 0;           // Current primary sensor
    String m_servoDimmerComponentId = "servo-dimmer-1"; // Servo dimmer to control
    
    // Lighting mode configuration
    LightingMode m_lightingMode = LightingMode::MORNING;
    float m_modeLumens[4] = {300.0f, 800.0f, 200.0f, 100.0f}; // Lumens per mode
    
    // Sample pool for sensor readings
    std::vector<std::vector<float>> m_sensorSamplePools;
    uint32_t m_poolIndex = 0;
    
    // Control state
    bool m_autoAdjustmentEnabled = true;
    float m_currentServoPosition = 0.0f;
    uint32_t m_lastAdjustmentMs = 0;
    uint32_t m_adjustmentCount = 0;
    float m_lastSensorAverage = 0.0f;
    String m_lastAdjustmentReason = "";
    
    // Statistics
    uint32_t m_totalAdjustments = 0;
    uint32_t m_sensorReadErrors = 0;
    uint32_t m_servoCommandErrors = 0;
    float m_maxRecordedReading = 0.0f;
    float m_minRecordedReading = 10000.0f;
    
    // Sweep test state
    bool m_sweepTestActive = false;
    int m_sweepCurrentPosition = 0;
    bool m_sweepDirectionUp = true;
    uint32_t m_sweepLastMoveMs = 0;
    uint32_t m_sweepIntervalMs = 2000;  // 2 seconds between moves
    int m_sweepStepSize = 5;            // 5% increments
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    bool validateSensorComponents();
    bool updateSensorReadings();
    float calculateWeightedAverage();
    bool shouldAdjustLighting();
    bool adjustServoDimmer();
    float calculateTargetServoPosition();
    bool sendServoCommand(float position);
    void updateSamplePools();
    void logAdjustmentDetails(const String& reason, float oldPosition, float newPosition);
    String getLightingModeString(LightingMode mode) const;
    LightingMode stringToLightingMode(const String& modeStr) const;
    bool isValidSensorReading(float reading) const;
    void updateSensorStatistics(float reading);
    BaseComponent* getComponentById(const String& componentId);
    JsonDocument getSensorData(const String& componentId);
    void updateSweepTest();

    // === Action System (BaseComponent virtual methods) ===
    std::vector<ComponentAction> getSupportedActions() const override;
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;
};