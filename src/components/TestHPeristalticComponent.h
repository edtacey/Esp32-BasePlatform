#pragma once

#include "BaseComponent.h"

// Forward declaration
class Orchestrator;

/**
 * @brief Test harness for multi-pump peristaltic dosing operations
 * 
 * Automated testing component that can trigger random dosing operations on 
 * up to 8 peristaltic pumps with configurable intervals and volumes.
 */
class TestHPeristalticComponent : public BaseComponent {
public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for component access
     */
    TestHPeristalticComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator);
    
    /**
     * @brief Destructor
     */
    ~TestHPeristalticComponent() override;

    // Required BaseComponent implementations
    JsonDocument getDefaultSchema() const override;
    bool initialize(const JsonDocument& config) override;
    ExecutionResult execute() override;
    void cleanup() override;
    
    // Test harness functions
    /**
     * @brief Start automated dosing test
     * @return true if test started successfully
     */
    bool startTest();
    
    /**
     * @brief Stop automated dosing test
     * @return true if test stopped successfully
     */
    bool stopTest();
    
    /**
     * @brief Trigger manual dose on configured pumps
     * @return true if dose triggered successfully
     */
    bool triggerManualDose();

private:
    // Configuration parameters
    float m_minDoseVolume = 5.0;      // Minimum dose volume (ml)
    float m_maxDoseVolume = 100.0;    // Maximum dose volume (ml)  
    uint32_t m_doseFreqSec = 20;      // Frequency between doses (seconds)
    bool m_randomDosing = true;       // Enable random dosing mode
    
    // 8-Pump configuration
    struct PumpConfig {
        String pumpId;
        String functionName;
        bool enabled;
    };
    
    PumpConfig m_pumps[8] = {
        {"pump-1", "Nutrient-A", true},
        {"pump-2", "Nutrient-B", true}, 
        {"pump-3", "pH-Up", true},
        {"pump-4", "pH-Down", true},
        {"pump-5", "CalMag", true},
        {"pump-6", "Bloom", true},
        {"pump-7", "Root-Boost", true},
        {"pump-8", "Flush", true}
    };
    
    // State tracking
    bool m_testRunning = false;
    uint32_t m_lastDoseTime = 0;
    uint32_t m_nextDoseTime = 0;
    uint32_t m_doseAttempts = 0;
    uint32_t m_successfulDoses = 0;
    uint32_t m_failedDoses = 0;
    float m_totalVolumeDispensed = 0;
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    bool triggerRandomDose();
    bool triggerPumpDose(const String& pumpId, float volume);
    bool isPumpAvailable(const String& pumpId);
    void updateTestState();
    void resetTestStats();
    float generateRandomVolume();
    
    // Orchestrator reference for component lookup
    Orchestrator* m_orchestrator;
};