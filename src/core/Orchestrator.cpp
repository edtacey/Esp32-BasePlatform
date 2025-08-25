/**
 * @file Orchestrator.cpp
 * @brief Orchestrator implementation
 */

#include "Orchestrator.h"
#include "../components/PeristalticPumpComponent.h"
#include "../components/TestHPeristalticComponent.h"

Orchestrator::Orchestrator() {
    log(Logger::DEBUG, "Orchestrator created");
}

Orchestrator::~Orchestrator() {
    shutdown();
}

bool Orchestrator::init() {
    log(Logger::INFO, "Initializing ESP32 IoT Orchestrator...");
    
    // Initialize logger first
    Logger::init(Logger::DEBUG);
    
    // Record start time
    m_startTime = millis();
    
    // Initialize configuration storage
    log(Logger::INFO, "Initializing configuration storage...");
    if (!m_storage.init()) {
        log(Logger::ERROR, "Failed to initialize configuration storage");
        return false;
    }
    
    // Load system configuration
    loadSystemConfig();
    
    // Initialize default components
    if (!initializeDefaultComponents()) {
        log(Logger::ERROR, "Failed to initialize default components");
        return false;
    }
    
    m_initialized = true;
    m_running = true;
    
    log(Logger::INFO, "Orchestrator initialization complete");
    log(Logger::INFO, String("System started with ") + m_components.size() + " components");
    
    return true;
}

void Orchestrator::loop() {
    if (!m_initialized || !m_running) {
        return;
    }
    
    m_loopCount++;
    
    // Execute component loop
    executeComponentLoop();
    
    // Perform system checks periodically
    uint32_t now = millis();
    if (now - m_lastSystemCheck >= m_systemCheckInterval) {
        performSystemCheck();
        m_lastSystemCheck = now;
    }
    
    // Update statistics
    updateStatistics();
}

void Orchestrator::shutdown() {
    if (!m_running) {
        return;
    }
    
    log(Logger::INFO, "Shutting down orchestrator...");
    
    m_running = false;
    
    // Cleanup all components
    for (auto* component : m_components) {
        if (component) {
            log(Logger::DEBUG, "Cleaning up component: " + component->getId());
            component->cleanup();
            delete component;
        }
    }
    m_components.clear();
    
    // Save system configuration
    saveSystemConfig();
    
    log(Logger::INFO, "Orchestrator shutdown complete");
}

bool Orchestrator::registerComponent(BaseComponent* component) {
    if (!component) {
        log(Logger::ERROR, "Cannot register null component");
        return false;
    }
    
    // Check if component already registered
    for (auto* existing : m_components) {
        if (existing && existing->getId() == component->getId()) {
            log(Logger::WARNING, "Component already registered: " + component->getId());
            return false;
        }
    }
    
    // Check maximum component limit
    if (m_components.size() >= m_maxComponents) {
        log(Logger::ERROR, "Maximum component limit reached: " + String(m_maxComponents));
        return false;
    }
    
    // Add component to list
    m_components.push_back(component);
    
    log(Logger::INFO, "Component registered: " + component->getId() + 
                      " (" + component->getType() + ")");
    
    return true;
}

bool Orchestrator::unregisterComponent(const String& componentId) {
    auto it = std::find_if(m_components.begin(), m_components.end(),
        [&componentId](BaseComponent* c) {
            return c && c->getId() == componentId;
        });
    
    if (it == m_components.end()) {
        log(Logger::WARNING, "Component not found for unregistration: " + componentId);
        return false;
    }
    
    BaseComponent* component = *it;
    component->cleanup();
    delete component;
    m_components.erase(it);
    
    log(Logger::INFO, "Component unregistered: " + componentId);
    return true;
}

BaseComponent* Orchestrator::findComponent(const String& componentId) {
    auto it = std::find_if(m_components.begin(), m_components.end(),
        [&componentId](BaseComponent* c) {
            return c && c->getId() == componentId;
        });
    
    return (it != m_components.end()) ? *it : nullptr;
}

bool Orchestrator::updateNextCheck(const String& componentId, uint32_t timeToWakeUp) {
    BaseComponent* component = findComponent(componentId);
    if (!component) {
        log(Logger::WARNING, "Cannot update schedule - component not found: " + componentId);
        return false;
    }
    
    uint32_t oldTime = component->getNextExecutionMs();
    component->setNextExecutionMs(timeToWakeUp);
    
    log(Logger::DEBUG, String("Updated schedule for ") + componentId + 
                       ": " + oldTime + "ms -> " + timeToWakeUp + "ms");
    
    return true;
}

uint32_t Orchestrator::getUptime() const {
    return millis() - m_startTime;
}

JsonDocument Orchestrator::getSystemStats() const {
    JsonDocument stats;
    
    stats["uptime"] = getUptime();
    stats["componentCount"] = m_components.size();
    stats["totalExecutions"] = m_totalExecutions;
    stats["totalErrors"] = m_totalErrors;
    stats["loopCount"] = m_loopCount;
    stats["initialized"] = m_initialized;
    stats["running"] = m_running;
    
    // Memory statistics
    stats["freeHeap"] = ESP.getFreeHeap();
    stats["minFreeHeap"] = ESP.getMinFreeHeap();
    stats["maxAllocHeap"] = ESP.getMaxAllocHeap();
    
    // Component states
    JsonObject componentStates = stats["componentStates"].to<JsonObject>();
    for (auto* component : m_components) {
        if (component) {
            componentStates[component->getId()] = component->getStateString();
        }
    }
    
    return stats;
}

JsonDocument Orchestrator::getHealthStatus() const {
    JsonDocument health;
    
    health["overall"] = "healthy";  // Will be updated based on checks
    health["timestamp"] = millis();
    
    // System resources
    JsonObject resources = health["resources"].to<JsonObject>();
    resources["freeHeap"] = ESP.getFreeHeap();
    resources["heapHealth"] = ESP.getFreeHeap() > 10000 ? "good" : "critical";
    
    // Component health
    JsonObject components = health["components"].to<JsonObject>();
    int healthyCount = 0;
    int errorCount = 0;
    
    for (auto* component : m_components) {
        if (component) {
            String componentId = component->getId();
            ComponentState state = component->getState();
            
            if (state == ComponentState::READY) {
                components[componentId] = "healthy";
                healthyCount++;
            } else if (state == ComponentState::ERROR) {
                components[componentId] = "error";
                errorCount++;
            } else {
                components[componentId] = component->getStateString();
            }
        }
    }
    
    // Overall health assessment
    if (errorCount > 0) {
        health["overall"] = "degraded";
    } else if (healthyCount == 0 && m_components.size() > 0) {
        health["overall"] = "critical";
    }
    
    health["healthyComponents"] = healthyCount;
    health["errorComponents"] = errorCount;
    
    return health;
}

bool Orchestrator::loadSystemConfig() {
    log(Logger::DEBUG, "Loading system configuration...");
    
    JsonDocument config;
    if (m_storage.loadSystemConfig(config)) {
        // Apply system configuration
        if (config["systemCheckInterval"].is<uint32_t>()) {
            m_systemCheckInterval = config["systemCheckInterval"].as<uint32_t>();
        }
        if (config["maxComponents"].is<uint16_t>()) {
            m_maxComponents = config["maxComponents"].as<uint32_t>();
        }
        
        log(Logger::INFO, "System configuration loaded");
        return true;
    } else {
        log(Logger::INFO, "No system configuration found - using defaults");
        return true;  // Not an error - use defaults
    }
}

bool Orchestrator::saveSystemConfig() {
    log(Logger::DEBUG, "Saving system configuration...");
    
    JsonDocument config;
    config["systemCheckInterval"] = m_systemCheckInterval;
    config["maxComponents"] = m_maxComponents;
    config["version"] = "1.0.0";
    config["lastSaved"] = millis();
    
    if (m_storage.saveSystemConfig(config)) {
        log(Logger::INFO, "System configuration saved");
        return true;
    } else {
        log(Logger::ERROR, "Failed to save system configuration");
        return false;
    }
}

// Private methods

bool Orchestrator::initializeDefaultComponents() {
    log(Logger::INFO, "Initializing default components...");
    
    bool allSuccess = true;
    
    // Initialize DHT22 sensor with default configuration (pin 15)
    log(Logger::INFO, "Creating DHT22 temperature/humidity sensor...");
    DHT22Component* dht = new DHT22Component("dht22-1", "Room Temperature/Humidity", m_storage, this);
    
    if (!dht->initialize(JsonDocument())) {  // Use default configuration
        log(Logger::ERROR, "Failed to initialize DHT22 component");
        delete dht;
        allSuccess = false;
    } else {
        if (!registerComponent(dht)) {
            log(Logger::ERROR, "Failed to register DHT22 component");
            delete dht;
            allSuccess = false;
        }
    }
    
    // Initialize TSL2561 light sensor with default configuration
    log(Logger::INFO, "Creating TSL2561 light sensor...");
    TSL2561Component* tsl = new TSL2561Component("tsl2561-1", "Ambient Light Sensor", m_storage, this);
    
    if (!tsl->initialize(JsonDocument())) {  // Use default configuration
        log(Logger::ERROR, "Failed to initialize TSL2561 component");
        delete tsl;
        allSuccess = false;
    } else {
        if (!registerComponent(tsl)) {
            log(Logger::ERROR, "Failed to register TSL2561 component");
            delete tsl;
            allSuccess = false;
        }
    }
    
    // Initialize Peristaltic Pump with default configuration (GPIO26, 40ml/s)
    log(Logger::INFO, "Creating peristaltic pump...");
    PeristalticPumpComponent* pump = new PeristalticPumpComponent("pump-1", "Nutrient Pump", m_storage, this);
    
    if (!pump->initialize(JsonDocument())) {  // Use default configuration
        log(Logger::ERROR, "Failed to initialize pump component");
        delete pump;
        allSuccess = false;
    } else {
        if (!registerComponent(pump)) {
            log(Logger::ERROR, "Failed to register pump component");
            delete pump;
            allSuccess = false;
        }
    }
    
    // Initialize Test Harness for pump testing
    log(Logger::INFO, "Creating pump test harness...");
    TestHPeristalticComponent* testHarness = new TestHPeristalticComponent("test-harness-1", "Pump Test Harness", m_storage, this);
    
    if (!testHarness->initialize(JsonDocument())) {  // Use default configuration
        log(Logger::ERROR, "Failed to initialize test harness component");
        delete testHarness;
        allSuccess = false;
    } else {
        if (!registerComponent(testHarness)) {
            log(Logger::ERROR, "Failed to register test harness component");
            delete testHarness;
            allSuccess = false;
        }
    }
    
    if (allSuccess) {
        log(Logger::INFO, String("Default components initialized successfully (") + 
                          m_components.size() + " total)");
    } else {
        log(Logger::WARNING, "Some default components failed to initialize");
    }
    
    return allSuccess;
}

void Orchestrator::executeComponentLoop() {
    int executedCount = executeReadyComponents();
    
    if (executedCount > 0) {
        log(Logger::DEBUG, String("Executed ") + executedCount + " components");
    }
}

void Orchestrator::performSystemCheck() {
    log(Logger::DEBUG, "Performing system health check...");
    
    // Check system resources
    if (!checkSystemResources()) {
        log(Logger::WARNING, "System resources are under pressure");
    }
    
    // Check component health
    int errorComponents = 0;
    for (auto* component : m_components) {
        if (component && component->getState() == ComponentState::ERROR) {
            errorComponents++;
            log(Logger::WARNING, "Component in error state: " + component->getId() + 
                                 " - " + component->getLastError());
        }
    }
    
    if (errorComponents > 0) {
        log(Logger::WARNING, String(errorComponents) + " components in error state");
    }
    
    // Log system statistics periodically
    JsonDocument stats = getSystemStats();
    log(Logger::INFO, String("System Stats - Uptime: ") + (getUptime() / 1000) + "s" +
                      ", Components: " + m_components.size() +
                      ", Executions: " + m_totalExecutions +
                      ", Free Heap: " + ESP.getFreeHeap() + " bytes");
}

int Orchestrator::executeReadyComponents() {
    int executedCount = 0;
    
    for (auto* component : m_components) {
        if (!component) continue;
        
        // Check if component is ready to execute
        if (component->isReadyToExecute()) {
            log(Logger::DEBUG, "Executing component: " + component->getId());
            
            // Execute the component
            ExecutionResult result = component->execute();
            
            // Handle the result
            handleExecutionResult(component, result);
            
            executedCount++;
        }
    }
    
    return executedCount;
}

void Orchestrator::handleExecutionResult(BaseComponent* component, const ExecutionResult& result) {
    m_totalExecutions++;
    
    if (result.success) {
        log(Logger::DEBUG, "Component execution successful: " + component->getId() +
                           " (" + result.executionTimeMs + "ms)");
    } else {
        m_totalErrors++;
        log(Logger::WARNING, "Component execution failed: " + component->getId() +
                            " - " + result.message);
    }
    
    // Log detailed data for debugging (only in DEBUG mode)
    if (result.success && !result.data.isNull()) {
        String dataStr;
        serializeJson(result.data, dataStr);
        log(Logger::DEBUG, "Component data: " + component->getId() + " -> " + dataStr);
    }
}

void Orchestrator::log(Logger::Level level, const String& message) {
    switch (level) {
        case Logger::DEBUG:
            Logger::debug("Orchestrator", message);
            break;
        case Logger::INFO:
            Logger::info("Orchestrator", message);
            break;
        case Logger::WARNING:
            Logger::warning("Orchestrator", message);
            break;
        case Logger::ERROR:
            Logger::error("Orchestrator", message);
            break;
        case Logger::CRITICAL:
            Logger::critical("Orchestrator", message);
            break;
    }
}

void Orchestrator::updateStatistics() {
    // Statistics are updated in real-time during execution
    // This function could be used for periodic calculations
}

bool Orchestrator::checkSystemResources() {
    // Check available heap memory
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 10000) {  // Less than 10KB free
        log(Logger::WARNING, String("Low memory: ") + freeHeap + " bytes free");
        return false;
    }
    
    // Check for heap fragmentation
    uint32_t maxBlock = ESP.getMaxAllocHeap();
    if (maxBlock < freeHeap / 2) {  // Fragmentation detected
        log(Logger::WARNING, "Heap fragmentation detected");
        return false;
    }
    
    return true;
}