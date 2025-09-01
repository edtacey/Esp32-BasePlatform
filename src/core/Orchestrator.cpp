/**
 * @file Orchestrator.cpp
 * @brief Orchestrator implementation
 */

#include "Orchestrator.h"
#include "../components/DHT22Component.h"
#include "../components/TSL2561Component.h"
#include "../components/PeristalticPumpComponent.h"
#include "../components/WebServerComponent.h"
#include "../components/PHSensorComponent.h"
#include "../components/ECProbeComponent.h"
// #include "../components/TestHPeristalticComponent.h"  // Disabled to save memory
// #include "../components/MqttBroadcastComponent.h"      // Disabled to save memory
// #include "../components/ServoDimmerComponent.h"         // Disabled to save memory
// #include "../components/LightOrchestrator.h"            // Disabled to save memory

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
    
    // Load execution loop configuration
    JsonDocument execConfig;
    if (m_storage.loadExecutionLoopConfig(execConfig)) {
        log(Logger::INFO, "Loading saved execution loop configuration");
        updateExecutionLoopConfig(execConfig);
    } else {
        log(Logger::INFO, "No saved execution loop config found, using defaults");
    }
    
    // Initialize components from persistent storage or defaults
    if (!initializeComponents()) {
        log(Logger::ERROR, "Failed to initialize components");
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
    
    // Execute component loop (unless paused)
    if (!m_executionLoopPaused) {
        executeComponentLoop();
    }
    
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

bool Orchestrator::initializeComponents() {
    log(Logger::INFO, "Initializing components from storage or defaults...");
    
    // Get list of all stored component configurations
    std::vector<String> storedComponents = m_storage.listComponentConfigs();
    
    log(Logger::INFO, String("Found ") + storedComponents.size() + " stored component configurations");
    
    if (storedComponents.size() == 0) {
        // No stored components, initialize with defaults
        log(Logger::INFO, "No stored components found, initializing defaults...");
        return initializeDefaultComponents();
    }
    
    // Load each stored component
    bool allSuccess = true;
    int loadedCount = 0;
    
    for (const String& componentId : storedComponents) {
        JsonDocument config;
        if (m_storage.loadComponentConfig(componentId, config)) {
            
            // Extract component type from stored config
            String componentType = config["component_type"];
            if (componentType.isEmpty()) {
                log(Logger::WARNING, "Stored config for " + componentId + " missing component_type, skipping");
                continue;
            }
            
            // Create component instance based on type
            BaseComponent* component = createComponentByType(componentId, componentType);
            if (!component) {
                log(Logger::ERROR, "Failed to create component " + componentId + " of type " + componentType);
                allSuccess = false;
                continue;
            }
            
            // Initialize component with stored config
            if (component->initialize(config)) {
                if (registerComponent(component)) {
                    loadedCount++;
                    log(Logger::INFO, "Loaded component: " + componentId + " (" + componentType + ")");
                } else {
                    log(Logger::ERROR, "Failed to register component: " + componentId);
                    delete component;
                    allSuccess = false;
                }
            } else {
                log(Logger::ERROR, "Failed to initialize component: " + componentId);
                delete component;
                allSuccess = false;
            }
            
        } else {
            log(Logger::ERROR, "Failed to load config for component: " + componentId);
            allSuccess = false;
        }
    }
    
    log(Logger::INFO, String("Loaded ") + loadedCount + " components from storage");
    
    // If no components were loaded successfully, fall back to defaults
    if (loadedCount == 0) {
        log(Logger::WARNING, "No components loaded from storage, falling back to defaults");
        return initializeDefaultComponents();
    }
    
    return allSuccess;
}

BaseComponent* Orchestrator::createComponentByType(const String& componentId, const String& componentType) {
    String componentName = componentId; // Default name, can be overridden by config
    
    if (componentType == "DHT22" || componentType == "TemperatureSensor") {
        return new DHT22Component(componentId, componentName, m_storage, this);
    }
    else if (componentType == "TSL2561" || componentType == "LightSensor") {
        return new TSL2561Component(componentId, componentName, m_storage, this);
    }
    else if (componentType == "PeristalticPump" || componentType == "Pump") {
        return new PeristalticPumpComponent(componentId, componentName, m_storage, this);
    }
    else if (componentType == "WebServer") {
        return new WebServerComponent(componentId, componentName, m_storage, this);
    }
    else if (componentType == "PHSensor") {
        return new PHSensorComponent(componentId, componentName, m_storage, this);
    }
    else if (componentType == "ECProbe") {
        return new ECProbeComponent(componentId, componentName, m_storage, this);
    }
    else {
        log(Logger::ERROR, "Unknown component type: " + componentType);
        return nullptr;
    }
}

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
    
    // Local TSL2561 light sensor disabled to save flash memory
    // log(Logger::INFO, "Creating local TSL2561 light sensor...");
    // TSL2561Component* tslLocal = new TSL2561Component("tsl2561-local", "Local Light Sensor", m_storage, this);
    
    // Initialize remote TSL2561 light sensor with HTTP
    log(Logger::INFO, "Creating remote TSL2561 light sensor...");
    TSL2561Component* tslRemote = new TSL2561Component("tsl2561-remote", "Remote Light Sensor", m_storage, this);
    
    JsonDocument remoteConfig;
    remoteConfig["useRemoteSensor"] = true;
    remoteConfig["remoteHost"] = "192.168.1.156";
    remoteConfig["remotePort"] = 80;
    remoteConfig["remotePath"] = "/light";
    remoteConfig["httpTimeoutMs"] = 5000;
    remoteConfig["samplingIntervalMs"] = 3000;  // Different interval
    
    if (!tslRemote->initialize(remoteConfig)) {
        log(Logger::ERROR, "Failed to initialize remote TSL2561 component");
        delete tslRemote;
        allSuccess = false;
    } else {
        if (!registerComponent(tslRemote)) {
            log(Logger::ERROR, "Failed to register remote TSL2561 component");
            delete tslRemote;
            allSuccess = false;
        }
    }
    
    // Initialize 8 Peristaltic Pumps with different GPIO pins and functions
    struct PumpConfig {
        String id;
        String name;
        String function;
        uint8_t pin;
    };
    
    PumpConfig pumpConfigs[1] = {
        {"pump-1", "Nutrient-A", "Base nutrients", 26}
    };
    
    for (int i = 0; i < 1; i++) {  // Single pump for debugging
        log(Logger::INFO, String("Creating pump ") + (i+1) + ": " + pumpConfigs[i].name + " (" + pumpConfigs[i].function + ")");
        PeristalticPumpComponent* pump = new PeristalticPumpComponent(pumpConfigs[i].id, pumpConfigs[i].name, m_storage, this);
        
        // Create custom config with specific GPIO pin
        JsonDocument pumpConfig;
        pumpConfig["pumpPin"] = pumpConfigs[i].pin;
        pumpConfig["mlPerSecond"] = 40.0;  // Standard flow rate
        pumpConfig["maxRuntimeMs"] = 60000; // 60 second safety timeout
        
        if (!pump->initialize(pumpConfig)) {
            log(Logger::ERROR, String("Failed to initialize pump: ") + pumpConfigs[i].id);
            delete pump;
            allSuccess = false;
        } else {
            if (!registerComponent(pump)) {
                log(Logger::ERROR, String("Failed to register pump: ") + pumpConfigs[i].id);
                delete pump;
                allSuccess = false;
            } else {
                log(Logger::INFO, String("Successfully registered pump: ") + pumpConfigs[i].id + " on GPIO" + pumpConfigs[i].pin);
            }
        }
    }
    
    // Initialize Test Harness for pump testing - TEMPORARILY DISABLED TO SAVE FLASH MEMORY
    // log(Logger::INFO, "Creating pump test harness...");
    // TestHPeristalticComponent* testHarness = new TestHPeristalticComponent("test-harness-1", "Pump Test Harness", m_storage, this);
    // 
    // if (!testHarness->initialize(JsonDocument())) {  // Use default configuration
    //     log(Logger::ERROR, "Failed to initialize test harness component");
    //     delete testHarness;
    //     allSuccess = false;
    // } else {
    //     if (!registerComponent(testHarness)) {
    //         log(Logger::ERROR, "Failed to register test harness component");
    //         delete testHarness;
    //         allSuccess = false;
    //     }
    // }
    
    // MQTT Broadcast Component disabled to save flash memory
    // log(Logger::INFO, "Creating MQTT broadcast component...");
    // MqttBroadcastComponent* mqttBroadcast = new MqttBroadcastComponent("mqtt-broadcast-1", "MQTT Component Broadcaster", m_storage, this);
    
    // Initialize Web Server Component (after WiFi is connected)
    log(Logger::INFO, "Creating web server component...");
    WebServerComponent* webServer = new WebServerComponent("web-server-1", "HTTP API Server", m_storage, this);
    
    if (!webServer->initialize(JsonDocument())) {  // Use default configuration
        log(Logger::ERROR, "Failed to initialize web server component");
        delete webServer;
        allSuccess = false;
    } else {
        if (!registerComponent(webServer)) {
            log(Logger::ERROR, "Failed to register web server component");
            delete webServer;
            allSuccess = false;
        }
    }
    
    // Initialize Servo Dimmer Component - TEMPORARILY DISABLED TO SAVE FLASH MEMORY
    // log(Logger::INFO, "Creating servo dimmer component...");
    // ServoDimmerComponent* servoDimmer = new ServoDimmerComponent("servo-dimmer-1", "Lighting Servo Dimmer", m_storage, this);
    // 
    // if (!servoDimmer->initialize(JsonDocument())) {  // Use default configuration
    //     log(Logger::ERROR, "Failed to initialize servo dimmer component");
    //     delete servoDimmer;
    //     allSuccess = false;
    // } else {
    //     if (!registerComponent(servoDimmer)) {
    //         log(Logger::ERROR, "Failed to register servo dimmer component");
    //         delete servoDimmer;
    //         allSuccess = false;
    //     }
    // }
    
    // Initialize Light Orchestrator Component - TEMPORARILY DISABLED TO SAVE FLASH MEMORY
    // log(Logger::INFO, "Creating light orchestrator component...");
    // LightOrchestrator* lightOrchestrator = new LightOrchestrator("light-orchestrator-1", "Intelligent Lighting Controller", m_storage, this);
    // 
    // if (!lightOrchestrator->initialize(JsonDocument())) {  // Use default configuration
    //     log(Logger::ERROR, "Failed to initialize light orchestrator component");
    //     delete lightOrchestrator;
    //     allSuccess = false;
    // } else {
    //     if (!registerComponent(lightOrchestrator)) {
    //         log(Logger::ERROR, "Failed to register light orchestrator component");
    //         delete lightOrchestrator;
    //         allSuccess = false;
    //     }
    // }
    
    // Initialize pH Sensor Component (Mock Mode - GPIO 0)
    log(Logger::INFO, "Creating pH sensor component in mock mode...");
    PHSensorComponent* phSensor = new PHSensorComponent("ph-sensor-1", "pH Sensor (Mock)", m_storage, this);
    
    // Configure for mock mode (GPIO pin 0)
    JsonDocument phConfig;
    phConfig["gpio_pin"] = 0;  // Mock mode
    phConfig["temp_coefficient"] = 0.05992;
    phConfig["sample_size"] = 10;
    phConfig["adc_voltage_ref"] = 3.3;
    phConfig["adc_resolution"] = 4096;
    phConfig["reading_interval_ms"] = 500;
    phConfig["time_period_for_sampling"] = 10000;
    phConfig["outlier_threshold"] = 2.0;
    phConfig["temperature_source_id"] = "dht22-1";
    phConfig["excite_voltage_component_id"] = "";
    phConfig["excite_stabilize_ms"] = 500;
    
    if (!phSensor->initialize(phConfig)) {
        log(Logger::ERROR, "Failed to initialize pH sensor component");
        delete phSensor;
        allSuccess = false;
    } else {
        if (!registerComponent(phSensor)) {
            log(Logger::ERROR, "Failed to register pH sensor component");
            delete phSensor;
            allSuccess = false;
        } else {
            log(Logger::INFO, "Successfully registered pH sensor in mock mode");
        }
    }
    
    // Initialize EC Probe Component (Mock Mode - GPIO 0) 
    log(Logger::INFO, "Creating EC probe component in mock mode...");
    ECProbeComponent* ecProbe = new ECProbeComponent("ec-probe-1", "EC Probe (Mock)", m_storage, this);
    
    // Configure for mock mode (GPIO pin 0)
    JsonDocument ecConfig;
    ecConfig["gpio_pin"] = 0;  // Mock mode
    ecConfig["temp_coefficient"] = 2.0;
    ecConfig["sample_size"] = 15;
    ecConfig["adc_voltage_ref"] = 3.3;
    ecConfig["adc_resolution"] = 4096;
    ecConfig["reading_interval_ms"] = 800;
    ecConfig["time_period_for_sampling"] = 15000;
    ecConfig["outlier_threshold"] = 2.5;
    ecConfig["tds_conversion_factor"] = 0.64;
    ecConfig["temperature_source_id"] = "dht22-1";
    ecConfig["excite_voltage_component_id"] = "";
    ecConfig["excite_stabilize_ms"] = 1000;
    
    if (!ecProbe->initialize(ecConfig)) {
        log(Logger::ERROR, "Failed to initialize EC probe component");
        delete ecProbe;
        allSuccess = false;
    } else {
        if (!registerComponent(ecProbe)) {
            log(Logger::ERROR, "Failed to register EC probe component");
            delete ecProbe;
            allSuccess = false;
        } else {
            log(Logger::INFO, "Successfully registered EC probe in mock mode");
        }
    }
    
    if (allSuccess) {
        log(Logger::INFO, String("Default components initialized successfully (") + 
                          m_components.size() + " total)");
    } else {
        log(Logger::WARNING, String("Some default components failed to initialize, continuing with ") + 
                            m_components.size() + " successful components");
    }
    
    // Always return true if at least one component succeeded
    bool hasComponents = m_components.size() > 0;
    if (!hasComponents) {
        log(Logger::ERROR, "No components successfully initialized!");
        return false;
    }
    
    return true;  // Continue even if some components failed
}

void Orchestrator::executeComponentLoop() {
    int executedCount = executeReadyComponents();
    
    if (executedCount > 0) {
        log(Logger::DEBUG, String("Executed ") + executedCount + " components");
    } else {
        // Force execute first component if nothing is executing  
        static int noExecCount = 0;
        noExecCount++;
        if (noExecCount > 100 && m_components.size() > 0) { // After 100 loops with no execution
            log(Logger::WARNING, "No components executing - forcing first component");
            if (m_components[0] && m_components[0]->getState() == ComponentState::READY) {
                ExecutionResult result = m_components[0]->execute();
                handleExecutionResult(m_components[0], result);
                noExecCount = 0;
            }
        }
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

JsonDocument Orchestrator::fetchRemoteData(const String& url, uint32_t timeoutMs) {
    JsonDocument result;
    
    // Use HttpClientWrapper for smart retry/backoff
    HttpResult httpResult = m_httpWrapper.get(url, timeoutMs);
    
    log(Logger::DEBUG, "Smart HTTP request to: " + url);
    
    if (httpResult.success) {
        log(Logger::DEBUG, "Remote data response: " + httpResult.response);
        
        DeserializationError error = deserializeJson(result, httpResult.response);
        if (error) {
            log(Logger::ERROR, String("Failed to parse remote JSON: ") + error.c_str());
            result.clear();
            result["error"] = "JSON parse failed";
        } else {
            result["success"] = true;
        }
    } else {
        log(Logger::ERROR, String("Smart HTTP request failed: ") + httpResult.error);
        result["error"] = httpResult.error;
        result["httpCode"] = httpResult.httpCode;
        result["shouldDefer"] = httpResult.shouldDefer;
        result["nextRetryMs"] = httpResult.nextRetryMs;
        
        // If we should defer, calculate retry delay in seconds for easier consumption
        if (httpResult.shouldDefer && httpResult.nextRetryMs > millis()) {
            result["retryInSeconds"] = (httpResult.nextRetryMs - millis()) / 1000;
        }
    }
    
    return result;
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

ActionResult Orchestrator::executeComponentAction(const String& componentId, const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.actionName = actionName;
    result.success = false;
    
    log(Logger::INFO, String("Inter-component action request: ") + componentId + "/" + actionName);
    
    // Find the target component
    BaseComponent* targetComponent = findComponent(componentId);
    if (!targetComponent) {
        result.message = "Component not found: " + componentId;
        log(Logger::ERROR, result.message);
        return result;
    }
    
    // Check if component is in a valid state for action execution
    if (targetComponent->getState() == ComponentState::ERROR) {
        result.message = "Cannot execute action on component in ERROR state: " + componentId;
        log(Logger::WARNING, result.message);
        return result;
    }
    
    if (targetComponent->getState() == ComponentState::COMPONENT_DISABLED) {
        result.message = "Cannot execute action on disabled component: " + componentId;
        log(Logger::WARNING, result.message);
        return result;
    }
    
    // Execute the action using the component's action system
    try {
        result = targetComponent->executeAction(actionName, parameters);
        
        if (result.success) {
            log(Logger::INFO, String("Inter-component action completed: ") + componentId + "/" + actionName + 
                             " (" + result.executionTimeMs + "ms)");
        } else {
            log(Logger::WARNING, String("Inter-component action failed: ") + componentId + "/" + actionName + 
                                " - " + result.message);
        }
        
    } catch (...) {
        result.success = false;
        result.message = "Exception during inter-component action execution";
        log(Logger::ERROR, result.message);
    }
    
    return result;
}

// === Execution Loop Control ===

bool Orchestrator::pauseExecutionLoop() {
    if (!m_initialized) {
        return false;
    }
    
    m_executionLoopPaused = true;
    log(Logger::INFO, "Execution loop paused");
    return true;
}

bool Orchestrator::resumeExecutionLoop() {
    if (!m_initialized) {
        return false;
    }
    
    m_executionLoopPaused = false;
    log(Logger::INFO, "Execution loop resumed");
    return true;
}

JsonDocument Orchestrator::getExecutionLoopConfig() const {
    JsonDocument config;
    
    config["paused"] = m_executionLoopPaused;
    config["system_check_interval_ms"] = m_systemCheckInterval;
    config["max_components"] = m_maxComponents;
    config["total_executions"] = m_totalExecutions;
    config["total_errors"] = m_totalErrors;
    config["loop_count"] = m_loopCount;
    config["uptime_ms"] = getUptime();
    
    return config;
}

bool Orchestrator::updateExecutionLoopConfig(const JsonDocument& config) {
    if (!m_initialized) {
        return false;
    }
    
    // Update system check interval if provided
    if (config["system_check_interval_ms"].is<uint32_t>()) {
        uint32_t newInterval = config["system_check_interval_ms"];
        if (newInterval >= 1000 && newInterval <= 300000) { // 1s to 5min range
            m_systemCheckInterval = newInterval;
            log(Logger::INFO, String("Updated system check interval to ") + newInterval + "ms");
        } else {
            log(Logger::WARNING, "Invalid system check interval: " + String(newInterval));
            return false;
        }
    }
    
    // Update pause state if provided
    if (config["paused"].is<bool>()) {
        bool shouldPause = config["paused"];
        if (shouldPause != m_executionLoopPaused) {
            if (shouldPause) {
                pauseExecutionLoop();
            } else {
                resumeExecutionLoop();
            }
        }
    }
    
    // Save config to persistent storage
    m_storage.saveExecutionLoopConfig(getExecutionLoopConfig());
    
    return true;
}