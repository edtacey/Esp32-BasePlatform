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
    schema["title"] = "Multi-Pump Test Harness Configuration";
    schema["required"].add("minDoseVolume");
    schema["required"].add("maxDoseVolume");
    schema["required"].add("doseFreqSec");
    
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // Minimum dose volume (required)
    JsonObject minDose = properties["minDoseVolume"].to<JsonObject>();
    minDose["type"] = "number";
    minDose["minimum"] = 1.0;
    minDose["maximum"] = 50.0;
    minDose["default"] = 5.0;
    minDose["description"] = "Minimum random dose volume (ml)";
    
    // Maximum dose volume (required)
    JsonObject maxDose = properties["maxDoseVolume"].to<JsonObject>();
    maxDose["type"] = "number";
    maxDose["minimum"] = 10.0;
    maxDose["maximum"] = 500.0;
    maxDose["default"] = 100.0;
    maxDose["description"] = "Maximum random dose volume (ml)";
    
    // Dose frequency (required)
    JsonObject doseFreq = properties["doseFreqSec"].to<JsonObject>();
    doseFreq["type"] = "integer";
    doseFreq["minimum"] = 5;
    doseFreq["maximum"] = 300;
    doseFreq["default"] = 20;
    doseFreq["description"] = "Interval between random doses (seconds)";
    
    // Random dosing enabled
    JsonObject randomMode = properties["randomDosing"].to<JsonObject>();
    randomMode["type"] = "boolean";
    randomMode["default"] = true;
    randomMode["description"] = "Enable random pump selection and volume dosing";
    
    log(Logger::DEBUG, "Generated multi-pump test harness schema");
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
    log(Logger::INFO, String("Multi-pump test harness initialized - ") + m_minDoseVolume + 
                      "-" + m_maxDoseVolume + "ml random doses every " + m_doseFreqSec + "s");
    
    return true;
}

bool TestHPeristalticComponent::applyConfiguration(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying multi-pump test harness configuration");
    
    // Extract dose volume range
    if (config["minDoseVolume"].is<float>()) {
        m_minDoseVolume = config["minDoseVolume"].as<float>();
    }
    
    if (config["maxDoseVolume"].is<float>()) {
        m_maxDoseVolume = config["maxDoseVolume"].as<float>();
    }
    
    // Extract dose frequency
    if (config["doseFreqSec"].is<uint32_t>()) {
        m_doseFreqSec = config["doseFreqSec"].as<uint32_t>();
    }
    
    // Extract random dosing mode
    if (config["randomDosing"].is<bool>()) {
        m_randomDosing = config["randomDosing"].as<bool>();
    }
    
    log(Logger::DEBUG, String("Config applied: minDose=") + m_minDoseVolume + "ml" +
                       ", maxDose=" + m_maxDoseVolume + "ml" +
                       ", freq=" + m_doseFreqSec + "s" +
                       ", random=" + (m_randomDosing ? "enabled" : "disabled"));
    
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
    data["random_dosing"] = m_randomDosing;
    data["min_dose_volume_ml"] = m_minDoseVolume;
    data["max_dose_volume_ml"] = m_maxDoseVolume;
    data["dose_frequency_sec"] = m_doseFreqSec;
    data["dose_attempts"] = m_doseAttempts;
    data["successful_doses"] = m_successfulDoses;
    data["failed_doses"] = m_failedDoses;
    data["total_volume_ml"] = m_totalVolumeDispensed;
    
    // Add pump configuration status
    JsonArray pumps = data["pumps"].to<JsonArray>();
    for (int i = 0; i < 8; i++) {
        JsonObject pump = pumps.add<JsonObject>();
        pump["id"] = m_pumps[i].pumpId;
        pump["function"] = m_pumps[i].functionName;
        pump["enabled"] = m_pumps[i].enabled;
        pump["available"] = isPumpAvailable(m_pumps[i].pumpId);
    }
    
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
        if (m_randomDosing) {
            if (triggerRandomDose()) {
                float volume = generateRandomVolume();
                log(Logger::INFO, String("Random dose triggered - ") + volume + "ml");
            }
        } else {
            if (triggerManualDose()) {
                log(Logger::INFO, String("Manual dose triggered"));
            }
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
    int validPumps = 0;
    for (int i = 0; i < 8; i++) {
        if (m_pumps[i].enabled) {
            if (!m_orchestrator || !m_orchestrator->findComponent(m_pumps[i].pumpId)) {
                log(Logger::WARNING, String("Pump component not found: ") + m_pumps[i].pumpId + " (" + m_pumps[i].functionName + ")");
                m_pumps[i].enabled = false;
            } else {
                validPumps++;
            }
        }
    }
    
    if (validPumps == 0) {
        log(Logger::ERROR, "No valid pump components found - cannot start test");
        return false;
    }
    
    log(Logger::INFO, String("Found ") + validPumps + " valid pumps for testing");
    
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
    log(Logger::INFO, "Manual dose triggered - using first available pump");
    
    // Find first available pump
    for (int i = 0; i < 8; i++) {
        if (m_pumps[i].enabled && isPumpAvailable(m_pumps[i].pumpId)) {
            float volume = (m_minDoseVolume + m_maxDoseVolume) / 2.0; // Use average volume for manual dose
            
            m_doseAttempts++;
            bool success = triggerPumpDose(m_pumps[i].pumpId, volume);
            
            if (success) {
                m_successfulDoses++;
                m_totalVolumeDispensed += volume;
                m_lastDoseTime = millis();
                log(Logger::INFO, String("Manual dose completed: ") + m_pumps[i].pumpId + " (" + m_pumps[i].functionName + ") - " + volume + "ml");
            } else {
                m_failedDoses++;
            }
            
            return success;
        }
    }
    
    log(Logger::WARNING, "No available pumps for manual dose");
    m_failedDoses++;
    return false;
}

bool TestHPeristalticComponent::triggerPumpDose(const String& pumpId, float volume) {
    log(Logger::INFO, String("Triggering dose on pump ") + pumpId + ": " + volume + "ml");
    
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
    bool success = pump->dose(volume);
    if (success) {
        log(Logger::INFO, String("Successfully started dosing ") + volume + "ml on " + pumpId);
    } else {
        log(Logger::ERROR, String("Failed to start dosing on ") + pumpId);
    }
    
    return success;
}

bool TestHPeristalticComponent::triggerRandomDose() {
    log(Logger::DEBUG, "Triggering random dose on available pumps");
    
    // Create list of available pumps
    String availablePumps[8];
    int availableCount = 0;
    
    for (int i = 0; i < 8; i++) {
        if (m_pumps[i].enabled && isPumpAvailable(m_pumps[i].pumpId)) {
            availablePumps[availableCount] = m_pumps[i].pumpId;
            availableCount++;
        }
    }
    
    if (availableCount == 0) {
        log(Logger::WARNING, "No available pumps for random dosing");
        m_failedDoses++;
        return false;
    }
    
    // Select random pump
    int randomIndex = random(0, availableCount);
    String selectedPumpId = availablePumps[randomIndex];
    
    // Generate random volume
    float volume = generateRandomVolume();
    
    // Find function name for logging
    String functionName = "Unknown";
    for (int i = 0; i < 8; i++) {
        if (m_pumps[i].pumpId == selectedPumpId) {
            functionName = m_pumps[i].functionName;
            break;
        }
    }
    
    log(Logger::INFO, String("Random dose: ") + selectedPumpId + " (" + functionName + ") - " + volume + "ml");
    
    // Trigger the dose
    m_doseAttempts++;
    bool success = triggerPumpDose(selectedPumpId, volume);
    
    if (success) {
        m_successfulDoses++;
        m_totalVolumeDispensed += volume;
        m_lastDoseTime = millis();
    } else {
        m_failedDoses++;
    }
    
    return success;
}

bool TestHPeristalticComponent::isPumpAvailable(const String& pumpId) {
    if (!m_orchestrator) {
        return false;
    }
    
    BaseComponent* pumpComponent = m_orchestrator->findComponent(pumpId);
    if (!pumpComponent) {
        return false;
    }
    
    // Check if component is a PeristalticPumpComponent by type
    if (pumpComponent->getType() != "PeristalticPump") {
        return false;
    }
    
    // Safe cast and check if pump is currently running
    PeristalticPumpComponent* pump = static_cast<PeristalticPumpComponent*>(pumpComponent);
    return !pump->isPumping();
}

float TestHPeristalticComponent::generateRandomVolume() {
    // Generate random volume between min and max
    return m_minDoseVolume + (random(0, 1000) / 1000.0) * (m_maxDoseVolume - m_minDoseVolume);
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