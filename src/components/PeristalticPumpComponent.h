#pragma once

#include "BaseComponent.h"

/**
 * @brief Peristaltic pump component for precision liquid dosing
 * 
 * Controls a relay-driven peristaltic pump with configurable flow rates,
 * safety timeouts, and precise volume dosing capabilities.
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

private:
    // Configuration parameters
    uint8_t m_pumpPin = 26;           // Default pump control pin (GPIO26)
    float m_mlPerSecond = 40.0;       // Default flow rate 40ml/s
    uint32_t m_maxRuntimeMs = 60000;  // Safety timeout (60 seconds)
    bool m_relayInverted = true;      // Inverted relay logic (default)
    
    // State tracking
    bool m_isPumping = false;
    uint32_t m_pumpStartTime = 0;
    uint32_t m_targetDurationMs = 0;
    float m_currentDoseVolume = 0;
    float m_totalVolumePumped = 0;
    uint32_t m_totalPumpTimeMs = 0;
    uint32_t m_doseCount = 0;
    bool m_continuousMode = false;
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    bool initializePump();
    void updatePumpState();
    uint32_t calculatePumpTime(float volume_ml, float flow_rate);
    void startPump();
    void stopPump();
    void setPumpRelay(bool active);
};