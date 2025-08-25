#pragma once

#include "BaseComponent.h"

// Forward declaration
class Orchestrator;

/**
 * @brief Test harness for peristaltic pump dosing operations
 * 
 * Automated testing component that can trigger dosing operations on 
 * one or two peristaltic pumps at configurable intervals.
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
    float m_doseAmtMls = 10.0;        // Amount to dose per cycle (ml)
    uint32_t m_doseFreqSec = 30;      // Frequency between doses (seconds)
    String m_dosagePump1ID = "";      // ID of primary pump
    String m_dosagePump2ID = "";      // ID of secondary pump (optional)
    
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
    bool triggerPumpDose(const String& pumpId);
    void updateTestState();
    void resetTestStats();
    
    // Orchestrator reference for component lookup
    Orchestrator* m_orchestrator;
};