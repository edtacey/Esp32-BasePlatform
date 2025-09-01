#pragma once

#include "BaseComponent.h"

/**
 * @brief Dispense mode enumeration
 */
enum class DispenseMode {
    IDLE = 0,           // Not dispensing
    DOSE = 1,           // Fixed volume dispensing
    CONTINUOUS = 2,     // Continuous pumping
    TIMED = 3,          // Time-based dispensing
    CALIBRATION = 4     // Calibration mode
};

/**
 * @brief Peristaltic pump component for precision liquid dosing
 * 
 * Controls a relay-driven peristaltic pump with configurable flow rates,
 * safety timeouts, precise volume dosing capabilities, and full
 * tracking of liquid properties and dispense operations.
 */
class PeristalticPumpComponent : public BaseComponent {
public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     */
    PeristalticPumpComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Destructor - cleanup pump resources
     */
    ~PeristalticPumpComponent() override;

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
     * @brief Get supported actions for peristaltic pump
     * @return Vector of supported actions with parameter definitions
     */
    std::vector<ComponentAction> getSupportedActions() const override;
    
    /**
     * @brief Execute pump action with validated parameters
     * @param actionName Name of action to execute
     * @param parameters Validated JSON parameters
     * @return Action execution result
     */
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;

public:
    // Pump-specific functions
    /**
     * @brief Dose a specific volume of liquid
     * @param volume_ml Volume to dispense in milliliters
     * @param flow_rate Optional flow rate override (uses default if 0)
     * @return true if dosing started successfully
     */
    bool dose(float volume_ml, float flow_rate = 0);
    
    /**
     * @brief Start continuous pumping until stopped
     * @return true if started successfully
     */
    bool startContinuous();
    
    /**
     * @brief Stop pump operation immediately
     * @return true if stopped successfully
     */
    bool stop();
    
    /**
     * @brief Get current pump status
     * @return true if pump is currently running
     */
    bool isPumping() const { return m_isPumping; }
    
    /**
     * @brief Get total volume pumped lifetime
     * @return Total volume in milliliters
     */
    float getTotalVolume() const { return m_totalVolumePumped; }
    
    // === State Machine Output Getters ===
    
    /**
     * @brief Get current dispense mode
     * @return Current DispenseMode
     */
    DispenseMode getDispenseMode() const { return m_dispenseMode; }
    
    /**
     * @brief Get dispense target (volume in ml or time in seconds)
     * @return Target value based on mode
     */
    float getDispenseTarget() const { return m_dispenseTarget; }
    
    /**
     * @brief Get current volume dispensed in active operation
     * @return Volume in milliliters
     */
    float getCurrentVolume() const { return m_currentVolume; }
    
    /**
     * @brief Get dispense operation start timestamp
     * @return Milliseconds since boot
     */
    uint32_t getDispenseStartMs() const { return m_dispenseStartMs; }
    
    /**
     * @brief Get dispense operation end timestamp
     * @return Milliseconds since boot (actual or expected)
     */
    uint32_t getDispenseEndMs() const { return m_dispenseEndMs; }
    
    /**
     * @brief Get liquid name
     * @return Name of liquid being pumped
     */
    const String& getLiquidName() const { return m_liquidName; }
    
    /**
     * @brief Get liquid concentration
     * @return Concentration percentage (0-100%)
     */
    float getLiquidConcentration() const { return m_liquidConcentration; }

private:
    // === Persisted Configuration Parameters ===
    uint8_t m_pinNo = 26;                    // GPIO pin number for pump control
    float m_mlsPerSec = 40.0;                // Flow rate in milliliters per second
    String m_boardRef = "BOARD_1";           // Board reference identifier
    String m_liquidName = "Unknown";         // Name of liquid being pumped
    float m_liquidConcentration = 100.0;     // Concentration percentage (0-100%)
    uint32_t m_maxRuntimeMs = 60000;         // Safety timeout (60 seconds)
    bool m_relayInverted = true;             // Inverted relay logic
    
    // === State Machine Output Fields ===
    DispenseMode m_dispenseMode = DispenseMode::IDLE;  // Current dispense mode
    float m_dispenseTarget = 0.0;            // Target volume or duration
    float m_currentVolume = 0.0;             // Current volume dispensed in this operation
    uint32_t m_dispenseStartMs = 0;          // Dispense operation start time
    uint32_t m_dispenseEndMs = 0;            // Dispense operation end time (or expected end)
    
    // === Runtime State Tracking ===
    bool m_isPumping = false;                // Pump actively running
    uint32_t m_pumpStartTime = 0;            // Current pump cycle start
    uint32_t m_targetDurationMs = 0;         // Target duration for current operation
    float m_currentDoseVolume = 0;           // Current dose target volume
    
    // === Lifetime Statistics ===
    float m_totalVolumePumped = 0;           // Total volume pumped lifetime
    uint32_t m_totalPumpTimeMs = 0;          // Total pump runtime
    uint32_t m_doseCount = 0;                // Number of doses completed
    uint32_t m_lastCalibrationMs = 0;        // Last calibration timestamp
    
    // === Legacy compatibility ===
    uint8_t m_pumpPin = 26;                  // Legacy: maps to m_pinNo
    float m_mlPerSecond = 40.0;              // Legacy: maps to m_mlsPerSec
    bool m_continuousMode = false;           // Legacy: maps to DispenseMode::CONTINUOUS
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    bool initializePump();
    void updatePumpState();
    uint32_t calculatePumpTime(float volume_ml, float flow_rate);
    void startPump();
    void stopPump();
    void setPumpRelay(bool active);
};