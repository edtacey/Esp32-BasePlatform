#include "TestHPeristalticComponent.h"
#include "PeristalticPumpComponent.h"
#include "../core/Orchestrator.h"

TestHPeristalticComponent::TestHPeristalticComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "TestHPeristaltic", name, storage, orchestrator), m_orchestrator(orchestrator) {
    log(Logger::DEBUG, "TestHPeristalticComponent created");
}

TestHPeristalticComponent::~TestHPeristalticComponent() {
    cleanup();
}

JsonDocument TestHPeristalticComponent::getDefaultSchema() const {
    JsonDocument schema;
    schema["type"] = "object";
    schema["title"] = "Peristaltic Pump Test Harness Configuration";
    schema["required"].add("doseAmtMls");
    schema["required"].add("doseFreqSec");
    schema["required"].add("dosagePump1ID");
    
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // Dose amount (required)
    JsonObject doseAmt = properties["doseAmtMls"].to<JsonObject>();
    doseAmt["type"] = "number";
    doseAmt["minimum"] = 0.1;
    doseAmt["maximum"] = 1000.0;
    doseAmt["default"] = 10.0;
    doseAmt["description"] = "Volume to dose per cycle (ml)";
    
    // Dose frequency (required)
    JsonObject doseFreq = properties["doseFreqSec"].to<JsonObject>();
    doseFreq["type"] = "integer";
    doseFreq["minimum"] = 1;
    doseFreq["maximum"] = 3600;
    doseFreq["default"] = 30;
    doseFreq["description"] = "Interval between doses (seconds)";
    
    // Primary pump ID (required)
    JsonObject pump1ID = properties["dosagePump1ID"].to<JsonObject>();
    pump1ID["type"] = "string";
    pump1ID["default"] = "pump-1";
    pump1ID["maxLength"] = 32;
    pump1ID["description"] = "Component ID of primary pump";
    
    // Secondary pump ID (optional)
    JsonObject pump2ID = properties["dosagePump2ID"].to<JsonObject>();
    pump2ID["type"] = "string";
    pump2ID["default"] = "";
    pump2ID["maxLength"] = 32;
    pump2ID["description"] = "Component ID of secondary pump (optional)";
    
    log(Logger::DEBUG, "Generated test harness schema with 10ml doses every 30s");
    return schema;
}

bool TestHPeristalticComponent::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing peristaltic pump test harness...");
    setState(ComponentState::INITIALIZING);
    
    // Load configuration (will use defaults if config is empty)
    if (!loadConfiguration(config)) {
        setError("Failed to load configuration");
        return false;
    }
    
    // Apply the configuration
    if (!applyConfiguration(getConfiguration())) {
        setError("Failed to apply configuration");
        return false;
    }
    
    // Find pump components (this will be done at runtime during execute)
    resetTestStats();
    
    // Set initial execution time
    setNextExecutionMs(millis() + 5000);  // Start checking after 5 seconds
    
    setState(ComponentState::READY);
    log(Logger::INFO, String("Test harness initialized - ") + m_doseAmtMls + 
                      "ml every " + m_doseFreqSec + "s to pumps: " + 
                      m_dosagePump1ID + (m_dosagePump2ID.length() > 0 ? ", " + m_dosagePump2ID : ""));
    
    return true;
}

bool TestHPeristalticComponent::applyConfiguration(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying test harness configuration");
    
    // Extract dose amount
    if (config["doseAmtMls"].is<float>()) {
        m_doseAmtMls = config["doseAmtMls"].as<float>();
    }
    
    // Extract dose frequency
    if (config["doseFreqSec"].is<uint32_t>()) {
        m_doseFreqSec = config["doseFreqSec"].as<uint32_t>();
    }
    
    // Extract pump IDs
    if (config["dosagePump1ID"].is<String>()) {
        m_dosagePump1ID = config["dosagePump1ID"].as<String>();
    }
    
    if (config["dosagePump2ID"].is<String>()) {
        m_dosagePump2ID = config["dosagePump2ID"].as<String>();
    }
    
    log(Logger::DEBUG, String("Config applied: dose=") + m_doseAmtMls + "ml" +
                       ", freq=" + m_doseFreqSec + "s" +
                       ", pump1=" + m_dosagePump1ID +
                       ", pump2=" + m_dosagePump2ID);
    
    return true;
}

ExecutionResult TestHPeristalticComponent::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    
    setState(ComponentState::EXECUTING);
    
    // Update test state
    updateTestState();
    
    // Prepare output data
    JsonDocument data;
    data["timestamp"] = millis();
    data["test_running"] = m_testRunning;
    data["dose_amount_ml"] = m_doseAmtMls;
    data["dose_frequency_sec"] = m_doseFreqSec;
    data["pump1_id"] = m_dosagePump1ID;
    data["pump2_id"] = m_dosagePump2ID;
    data["dose_attempts"] = m_doseAttempts;
    data["successful_doses"] = m_successfulDoses;
    data["failed_doses"] = m_failedDoses;
    data["total_volume_ml"] = m_totalVolumeDispensed;
    data["success"] = true;
    
    if (m_testRunning && m_nextDoseTime > 0) {
        uint32_t timeToNext = (m_nextDoseTime > millis()) ? (m_nextDoseTime - millis()) : 0;
        data["next_dose_in_ms"] = timeToNext;
        data["next_dose_in_sec"] = timeToNext / 1000;
    }
    
    // Update execution interval
    setNextExecutionMs(millis() + 15000); // Check every 15 seconds
    
    result.success = true;
    result.data = data;
    result.executionTimeMs = millis() - startTime;
    
    setState(ComponentState::READY);
    return result;
}

void TestHPeristalticComponent::updateTestState() {
    if (!m_testRunning) return;
    
    uint32_t currentTime = millis();
    
    // Check if it's time for next dose
    if (m_nextDoseTime > 0 && currentTime >= m_nextDoseTime) {
        if (triggerManualDose()) {
            log(Logger::INFO, String("Automated dose triggered - ") + m_doseAmtMls + "ml");
        }
        
        // Schedule next dose
        m_nextDoseTime = currentTime + (m_doseFreqSec * 1000);
    }
}

bool TestHPeristalticComponent::startTest() {
    if (m_testRunning) {
        log(Logger::WARNING, "Test already running");
        return true;
    }
    
    // Validate pump components exist
    if (m_dosagePump1ID.length() > 0 && (!m_orchestrator || !m_orchestrator->findComponent(m_dosagePump1ID))) {
        log(Logger::ERROR, "Primary pump component not found: " + m_dosagePump1ID);
        return false;
    }
    if (m_dosagePump2ID.length() > 0 && (!m_orchestrator || !m_orchestrator->findComponent(m_dosagePump2ID))) {
        log(Logger::ERROR, "Secondary pump component not found: " + m_dosagePump2ID);
        return false;
    }
    
    m_testRunning = true;
    m_nextDoseTime = millis() + (m_doseFreqSec * 1000);  // First dose after frequency interval
    
    log(Logger::INFO, "Test started - next dose in " + String(m_doseFreqSec) + " seconds");
    return true;
}

bool TestHPeristalticComponent::stopTest() {
    if (!m_testRunning) {
        log(Logger::DEBUG, "Test already stopped");
        return true;
    }
    
    m_testRunning = false;
    m_nextDoseTime = 0;
    
    log(Logger::INFO, String("Test stopped - Stats: ") + m_successfulDoses + 
                      "/" + m_doseAttempts + " successful doses, " + 
                      m_totalVolumeDispensed + "ml total");
    return true;
}

bool TestHPeristalticComponent::triggerManualDose() {
    if (m_dosagePump1ID.length() == 0) {
        log(Logger::ERROR, "No pump ID configured");
        return false;
    }
    
    bool success = true;
    m_doseAttempts++;
    
    // Dose pump 1
    if (m_dosagePump1ID.length() > 0) {
        if (triggerPumpDose(m_dosagePump1ID)) {
            m_totalVolumeDispensed += m_doseAmtMls;
        } else {
            success = false;
        }
    }
    
    // Dose pump 2 if configured
    if (m_dosagePump2ID.length() > 0) {
        if (triggerPumpDose(m_dosagePump2ID)) {
            m_totalVolumeDispensed += m_doseAmtMls;
        } else {
            success = false;
        }
    }
    
    if (success) {
        m_successfulDoses++;
        m_lastDoseTime = millis();
    } else {
        m_failedDoses++;
    }
    
    return success;
}

bool TestHPeristalticComponent::triggerPumpDose(const String& pumpId) {
    log(Logger::INFO, String("Triggering dose on pump ") + pumpId + ": " + m_doseAmtMls + "ml");
    
    // Find the pump component by ID
    if (!m_orchestrator) {
        log(Logger::ERROR, "No orchestrator reference - cannot find pump component: " + pumpId);
        return false;
    }
    
    BaseComponent* pumpComponent = m_orchestrator->findComponent(pumpId);
    if (!pumpComponent) {
        log(Logger::ERROR, "Pump component not found: " + pumpId);
        return false;
    }
    
    // Check if component is a PeristalticPumpComponent by type
    if (pumpComponent->getType() != "PeristalticPump") {
        log(Logger::ERROR, "Component is not a PeristalticPumpComponent: " + pumpId + " (type: " + pumpComponent->getType() + ")");
        return false;
    }
    
    // Safe cast since we verified the type
    PeristalticPumpComponent* pump = static_cast<PeristalticPumpComponent*>(pumpComponent);
    
    // Call the actual dose method
    bool success = pump->dose(m_doseAmtMls);
    if (success) {
        log(Logger::INFO, String("Successfully started dosing ") + m_doseAmtMls + "ml on " + pumpId);
    } else {
        log(Logger::ERROR, String("Failed to start dosing on ") + pumpId);
    }
    
    return success;
}


void TestHPeristalticComponent::resetTestStats() {
    m_doseAttempts = 0;
    m_successfulDoses = 0;
    m_failedDoses = 0;
    m_totalVolumeDispensed = 0;
    m_lastDoseTime = 0;
    
    log(Logger::DEBUG, "Test statistics reset");
}

void TestHPeristalticComponent::cleanup() {
    log(Logger::DEBUG, "Cleaning up test harness");
    
    if (m_testRunning) {
        stopTest();
    }
}