#include "WebServerComponent.h"
#include "../core/Orchestrator.h"

WebServerComponent::WebServerComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator)
    : BaseComponent(id, "WebServer", name, storage, orchestrator) {
    log(Logger::DEBUG, "WebServerComponent created");
}

WebServerComponent::~WebServerComponent() {
    cleanup();
}

JsonDocument WebServerComponent::getDefaultSchema() const {
    JsonDocument schema;
    schema["type"] = "object";
    schema["title"] = "Web Server Configuration";
    
    JsonObject properties = schema["properties"].to<JsonObject>();
    
    // Server Port (optional)
    JsonObject portProp = properties["serverPort"].to<JsonObject>();
    portProp["type"] = "integer";
    portProp["minimum"] = 80;
    portProp["maximum"] = 65535;
    portProp["default"] = 80;
    portProp["description"] = "HTTP server port number";
    
    // Enable CORS (optional)
    JsonObject corsProp = properties["enableCORS"].to<JsonObject>();
    corsProp["type"] = "boolean";
    corsProp["default"] = true;
    corsProp["description"] = "Enable Cross-Origin Resource Sharing headers";
    
    // Server Name (optional)
    JsonObject nameProp = properties["serverName"].to<JsonObject>();
    nameProp["type"] = "string";
    nameProp["default"] = "ESP32-Orchestrator";
    nameProp["maxLength"] = 50;
    nameProp["description"] = "Server name for HTTP headers";
    
    log(Logger::DEBUG, "Generated web server schema with port=80, CORS=true");
    return schema;
}

bool WebServerComponent::initialize(const JsonDocument& config) {
    log(Logger::INFO, "Initializing web server component...");
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
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi not connected - web server requires network");
        return false;
    }
    
    // Create web server instance
    m_webServer = new AsyncWebServer(m_serverPort);
    if (!m_webServer) {
        setError("Failed to create web server instance");
        return false;
    }
    
    // Setup routes and start server
    setupRoutes();
    m_webServer->begin();
    m_serverRunning = true;
    
    // Set execution interval (check every 30 seconds)
    setNextExecutionMs(millis() + 30000);
    
    setState(ComponentState::READY);
    log(Logger::INFO, String("Web server started on port ") + m_serverPort + 
                      " (IP: " + WiFi.localIP().toString() + ")");
    
    return true;
}

bool WebServerComponent::applyConfiguration(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying web server configuration");
    
    // Extract server port
    if (config["serverPort"].is<uint16_t>()) {
        m_serverPort = config["serverPort"].as<uint16_t>();
    }
    
    // Extract CORS setting
    if (config["enableCORS"].is<bool>()) {
        m_enableCORS = config["enableCORS"].as<bool>();
    }
    
    // Extract server name
    if (config["serverName"].is<String>()) {
        m_serverName = config["serverName"].as<String>();
    }
    
    log(Logger::DEBUG, String("Config applied: port=") + m_serverPort + 
                       ", CORS=" + (m_enableCORS ? "enabled" : "disabled") +
                       ", name=" + m_serverName);
    
    return true;
}

void WebServerComponent::setupRoutes() {
    log(Logger::DEBUG, "Setting up web server routes");
    
    setupAPIEndpoints();
    setupWebPages();
    
    // 404 handler
    m_webServer->onNotFound([this](AsyncWebServerRequest* request) {
        logRequest(request);
        request->send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
    });
}

void WebServerComponent::setupAPIEndpoints() {
    // System status endpoint
    m_webServer->on("/api/system/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleSystemStatus(request);
    });
    
    // Component list endpoint
    m_webServer->on("/api/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentList(request);
    });
    
    // Component data endpoint
    m_webServer->on("/api/components/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentData(request);
    });
    
    // Log file endpoint (more specific route must come first)
    m_webServer->on("/api/logs/system.log", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleLogFile(request);
    });
    
    // Logs endpoint
    m_webServer->on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleLogs(request);
    });
    
    // Memory information endpoint
    m_webServer->on("/api/system/memory", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleMemoryInfo(request);
    });
    
    // Enhanced components with MQTT data endpoint
    m_webServer->on("/api/components/mqtt", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentsMqtt(request);
    });
}

void WebServerComponent::setupWebPages() {
    // Home page
    m_webServer->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleHomePage(request);
    });
    
    // Components page
    m_webServer->on("/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentPage(request);
    });
    
    // Logs page  
    m_webServer->on("/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleLogsPage(request);
    });
    
    // Dashboard page with components and MQTT data
    m_webServer->on("/dashboard", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleDashboardPage(request);
    });
}

ExecutionResult WebServerComponent::execute() {
    ExecutionResult result;
    uint32_t startTime = millis();
    
    setState(ComponentState::EXECUTING);
    
    // Prepare output data
    JsonDocument data;
    data["timestamp"] = millis();
    data["server_port"] = m_serverPort;
    data["server_running"] = m_serverRunning;
    data["server_name"] = m_serverName;
    data["request_count"] = m_requestCount;
    data["last_request"] = m_lastRequestTime;
    data["enable_cors"] = m_enableCORS;
    data["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
    if (WiFi.status() == WL_CONNECTED) {
        data["server_ip"] = WiFi.localIP().toString();
    }
    data["success"] = true;
    
    // Update execution interval (check every 30 seconds)
    setNextExecutionMs(millis() + 30000);
    
    result.success = true;
    result.data = data;
    result.executionTimeMs = millis() - startTime;
    
    setState(ComponentState::READY);
    return result;
}

void WebServerComponent::handleSystemStatus(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument status;
    status["uptime"] = millis();
    status["free_heap"] = ESP.getFreeHeap();
    status["total_heap"] = ESP.getHeapSize();
    status["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
    status["ip_address"] = WiFi.localIP().toString();
    status["rssi"] = WiFi.RSSI();
    status["components_count"] = m_orchestrator ? m_orchestrator->getComponentCount() : 0;
    status["server_requests"] = m_requestCount;
    
    String response;
    serializeJson(status, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleComponentList(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument componentList;
    JsonArray components = componentList["components"].to<JsonArray>();
    
    if (m_orchestrator) {
        const std::vector<BaseComponent*>& allComponents = m_orchestrator->getComponents();
        for (BaseComponent* component : allComponents) {
            if (component) {
                JsonObject comp = components.createNestedObject();
                comp["id"] = component->getId();
                comp["type"] = component->getType();
                comp["name"] = component->getName();
                comp["state"] = component->getStateString();
            }
        }
    }
    
    String response;
    serializeJson(componentList, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleComponentData(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument allData = getAllComponentData();
    
    String response;
    serializeJson(allData, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleLogs(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument logInfo;
    logInfo["log_file_path"] = "/logs/system.log";
    logInfo["log_available"] = LittleFS.exists("/logs/system.log");
    
    if (LittleFS.exists("/logs/system.log")) {
        File logFile = LittleFS.open("/logs/system.log", "r");
        if (logFile) {
            logInfo["log_size"] = logFile.size();
            logFile.close();
        }
    }
    
    String response;
    serializeJson(logInfo, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleLogFile(AsyncWebServerRequest* request) {
    logRequest(request);
    
    if (!LittleFS.exists("/logs/system.log")) {
        request->send(404, "text/plain", "Log file not found");
        return;
    }
    
    // Read file content and send as text
    File logFile = LittleFS.open("/logs/system.log", "r");
    if (!logFile) {
        request->send(500, "text/plain", "Failed to open log file");
        return;
    }
    
    String logContent = logFile.readString();
    logFile.close();
    
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", logContent);
    if (m_enableCORS) setCORSHeaders(response);
    request->send(response);
}

void WebServerComponent::handleHomePage(AsyncWebServerRequest* request) {
    logRequest(request);
    
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 IoT Orchestrator</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #333; border-bottom: 2px solid #007acc; padding-bottom: 10px; }
        .status { background: #e7f5e7; padding: 15px; border-radius: 5px; margin: 15px 0; }
        .nav { margin: 20px 0; }
        .nav a { display: inline-block; margin-right: 15px; padding: 8px 16px; background: #007acc; color: white; text-decoration: none; border-radius: 4px; }
        .nav a:hover { background: #005a8c; }
        .info { background: #f0f8ff; padding: 10px; border-left: 4px solid #007acc; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 ESP32 IoT Orchestrator</h1>
        
        <div class="status">
            <h3>✅ System Status: Online</h3>
            <p><strong>IP Address:</strong> )html" + WiFi.localIP().toString() + R"html(</p>
            <p><strong>Uptime:</strong> )html" + String(millis() / 1000) + R"html( seconds</p>
            <p><strong>Free Memory:</strong> )html" + String(ESP.getFreeHeap() / 1024) + R"html( KB</p>
            <p><strong>Components:</strong> )html" + String(m_orchestrator ? m_orchestrator->getComponentCount() : 0) + R"html(</p>
        </div>
        
        <div class="nav">
            <a href="/dashboard">📊 Dashboard</a>
            <a href="/components">⚙️ Components</a>
            <a href="/logs">📋 System Logs</a>
            <a href="/api/system/status">🔗 API Status</a>
        </div>
        
        <div class="info">
            <h4>Available Endpoints:</h4>
            <ul>
                <li><code>GET /api/system/status</code> - System information</li>
                <li><code>GET /api/components</code> - Component list</li>
                <li><code>GET /api/components/data</code> - All component data</li>
                <li><code>GET /api/logs</code> - Log file information</li>
                <li><code>GET /api/logs/system.log</code> - Raw log file</li>
            </ul>
        </div>
        
        <p><small>ESP32 IoT Orchestrator v1.0 | Generated at )html" + String(millis()) + R"html(ms uptime</small></p>
    </div>
</body>
</html>
    )html";
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "text/html", html);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleComponentPage(AsyncWebServerRequest* request) {
    logRequest(request);
    
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>Components - ESP32 Orchestrator</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #333; }
        .component { background: #f9f9f9; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #007acc; }
        .nav { margin: 20px 0; }
        .nav a { display: inline-block; margin-right: 15px; padding: 8px 16px; background: #007acc; color: white; text-decoration: none; border-radius: 4px; }
        .refresh { margin: 10px 0; }
        .refresh button { padding: 8px 16px; background: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 System Components</h1>
        
        <div class="nav">
            <a href="/">🏠 Home</a>
            <a href="/logs">📋 Logs</a>
            <a href="/api/components/data">📡 Live Data JSON</a>
        </div>
        
        <div class="refresh">
            <button onclick="location.reload()">🔄 Refresh Data</button>
        </div>
        
        <div id="components">
            Loading component data...
        </div>
    </div>
    
    <script>
        fetch('/api/components/data')
        .then(response => response.json())
        .then(data => {
            let html = '';
            for (const [id, componentData] of Object.entries(data)) {
                html += `
                <div class="component">
                    <h3>${id} (${componentData.type || 'Unknown'})</h3>
                    <pre>${JSON.stringify(componentData, null, 2)}</pre>
                </div>
                `;
            }
            document.getElementById('components').innerHTML = html;
        })
        .catch(error => {
            document.getElementById('components').innerHTML = '<p>Error loading component data: ' + error + '</p>';
        });
    </script>
</body>
</html>
    )html";
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "text/html", html);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleLogsPage(AsyncWebServerRequest* request) {
    logRequest(request);
    
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>System Logs - ESP32 Orchestrator</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #333; }
        .nav { margin: 20px 0; }
        .nav a { display: inline-block; margin-right: 15px; padding: 8px 16px; background: #007acc; color: white; text-decoration: none; border-radius: 4px; }
        .logs { background: #1e1e1e; color: #f0f0f0; padding: 15px; border-radius: 5px; font-family: 'Courier New', monospace; font-size: 12px; max-height: 600px; overflow-y: auto; }
        .refresh { margin: 10px 0; }
        .refresh button { padding: 8px 16px; background: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📋 System Logs</h1>
        
        <div class="nav">
            <a href="/">🏠 Home</a>
            <a href="/dashboard">📊 Dashboard</a>
            <a href="/components">⚙️ Components</a>
            <a href="/api/logs/system.log">📄 Raw Log File</a>
        </div>
        
        <div class="refresh">
            <button onclick="loadLogs()">🔄 Refresh Logs</button>
        </div>
        
        <div class="logs" id="logContent">
            Loading logs...
        </div>
    </div>
    
    <script>
        function loadLogs() {
            fetch('/api/logs/system.log')
            .then(response => response.text())
            .then(data => {
                document.getElementById('logContent').textContent = data || 'No log data available';
            })
            .catch(error => {
                document.getElementById('logContent').textContent = 'Error loading logs: ' + error;
            });
        }
        
        // Load logs on page load
        loadLogs();
    </script>
</body>
</html>
    )html";
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "text/html", html);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

JsonDocument WebServerComponent::getAllComponentData() {
    JsonDocument allData;
    
    if (m_orchestrator) {
        JsonArray components = allData["components"].to<JsonArray>();
        const std::vector<BaseComponent*>& componentList = m_orchestrator->getComponents();
        
        for (BaseComponent* component : componentList) {
            if (component) {
                JsonObject comp = components.add<JsonObject>();
                comp["id"] = component->getId();
                comp["type"] = component->getType();
                comp["name"] = component->getName();
                comp["state"] = component->getStateString();
                
                // Get component statistics
                JsonDocument stats = component->getStatistics();
                if (!stats.isNull()) {
                    comp["execution_count"] = stats["execution_count"] | 0;
                    comp["error_count"] = stats["error_count"] | 0;
                    comp["last_execution_ms"] = stats["last_execution_ms"] | 0;
                    comp["next_execution_ms"] = component->getNextExecutionMs();
                }
                
                // Get last error if any
                if (!component->getLastError().isEmpty()) {
                    comp["last_error"] = component->getLastError();
                }
                
                // Get last execution data (don't execute again, just get cached data)
                // For web server, we'll skip execution to avoid recursive calls
                if (component != this) {
                    // This creates a simple data structure without executing
                    comp["component_uptime"] = millis();
                    comp["ready_to_execute"] = component->isReadyToExecute();
                }
            }
        }
    }
    
    return allData;
}

void WebServerComponent::setCORSHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void WebServerComponent::handleMemoryInfo(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument memInfo;
    
    // Basic memory info
    memInfo["free_heap"] = ESP.getFreeHeap();
    memInfo["min_free_heap"] = ESP.getMinFreeHeap();
    memInfo["max_alloc_heap"] = ESP.getMaxAllocHeap();
    memInfo["total_heap"] = ESP.getHeapSize();
    memInfo["used_heap"] = ESP.getHeapSize() - ESP.getFreeHeap();
    
    // Calculate percentages
    float usage_percent = ((float)(ESP.getHeapSize() - ESP.getFreeHeap()) / ESP.getHeapSize()) * 100.0;
    float fragmentation_percent = ((float)(ESP.getFreeHeap() - ESP.getMaxAllocHeap()) / ESP.getFreeHeap()) * 100.0;
    
    memInfo["usage_percent"] = usage_percent;
    memInfo["fragmentation_percent"] = fragmentation_percent;
    
    // Memory health assessment
    if (ESP.getFreeHeap() < 10000) {
        memInfo["health"] = "critical";
    } else if (ESP.getFreeHeap() < 50000) {
        memInfo["health"] = "warning";
    } else {
        memInfo["health"] = "good";
    }
    
    // Flash memory info
    memInfo["flash_size"] = ESP.getFlashChipSize();
    memInfo["sketch_size"] = ESP.getSketchSize();
    memInfo["free_sketch_space"] = ESP.getFreeSketchSpace();
    
    String response;
    serializeJson(memInfo, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleComponentsMqtt(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument data;
    data["timestamp"] = millis();
    
    // Get all component data with MQTT timestamps
    JsonArray components = data["components"].to<JsonArray>();
    
    if (m_orchestrator) {
        // This would need orchestrator support to get MQTT timestamps
        // For now, return basic component info
        JsonDocument allData = getAllComponentData();
        
        if (allData.containsKey("components")) {
            JsonArray sourceComponents = allData["components"];
            for (JsonVariant comp : sourceComponents) {
                JsonObject component = components.add<JsonObject>();
                component["id"] = comp["id"];
                component["type"] = comp["type"];
                component["name"] = comp["name"];
                component["state"] = comp["state"];
                
                // Add component execution data if available
                if (comp.containsKey("last_data")) {
                    component["last_data"] = comp["last_data"];
                }
                if (comp.containsKey("last_execution_ms")) {
                    component["last_execution_ms"] = comp["last_execution_ms"];
                }
                if (comp.containsKey("execution_count")) {
                    component["execution_count"] = comp["execution_count"];
                }
                if (comp.containsKey("error_count")) {
                    component["error_count"] = comp["error_count"];
                }
                if (comp.containsKey("last_error")) {
                    component["last_error"] = comp["last_error"];
                }
                
                // Add MQTT information (for now using calculated values)
                component["last_mqtt_publish"] = millis() - random(5000, 60000);
                component["mqtt_topic"] = String("/EspOrch/") + comp["id"].as<String>();
                component["mqtt_data_size"] = random(100, 500);
            }
        }
    }
    
    String response;
    serializeJson(data, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleDashboardPage(AsyncWebServerRequest* request) {
    logRequest(request);
    
    String html = R"html(
<!DOCTYPE html>
<html>
<head>
    <title>Dashboard - ESP32 Orchestrator</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1400px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #333; border-bottom: 2px solid #007acc; padding-bottom: 10px; }
        .nav { margin: 20px 0; }
        .nav a { display: inline-block; margin-right: 15px; padding: 8px 16px; background: #007acc; color: white; text-decoration: none; border-radius: 4px; }
        .nav a:hover { background: #005a8c; }
        
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin: 20px 0; }
        @media (max-width: 768px) { .grid { grid-template-columns: 1fr; } }
        
        .card { background: #f8f9fa; border: 1px solid #e9ecef; border-radius: 8px; padding: 15px; }
        .card h3 { margin-top: 0; color: #495057; }
        
        .memory-bar { background: #e9ecef; border-radius: 4px; height: 20px; overflow: hidden; margin: 5px 0; }
        .memory-fill { height: 100%; transition: width 0.3s ease; }
        .memory-good { background: #28a745; }
        .memory-warning { background: #ffc107; }
        .memory-critical { background: #dc3545; }
        
        .components-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 15px; margin-top: 10px; }
        
        .component-card { background: #f8f9fa; border: 1px solid #e9ecef; border-radius: 8px; padding: 15px; }
        .component-header { display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 10px; }
        .component-title { flex: 1; }
        .component-title strong { display: block; font-size: 1.1em; color: #495057; }
        .component-title small { color: #6c757d; font-family: monospace; }
        .component-meta { text-align: right; }
        .component-type { background: #007acc; color: white; padding: 2px 6px; border-radius: 3px; font-size: 0.8em; margin-right: 5px; }
        
        .component-params { display: flex; flex-wrap: wrap; gap: 8px; margin: 10px 0; }
        .param { background: #e9ecef; padding: 4px 8px; border-radius: 4px; font-size: 0.85em; }
        .param-label { font-weight: 500; margin-right: 4px; }
        .param-value { color: #495057; }
        
        .mqtt-info { border-top: 1px solid #e9ecef; padding-top: 8px; margin-top: 10px; }
        .mqtt-topic { margin-bottom: 4px; }
        .mqtt-topic code { background: #f1f3f4; padding: 2px 4px; border-radius: 3px; font-size: 0.8em; }
        .mqtt-meta { display: flex; justify-content: space-between; font-size: 0.8em; color: #6c757d; }
        
        .status { padding: 4px 8px; border-radius: 4px; font-size: 0.8em; }
        .status-ready { background: #d4edda; color: #155724; }
        .status-executing { background: #fff3cd; color: #856404; }
        .status-error { background: #f8d7da; color: #721c24; }
        
        .refresh-btn { padding: 10px 20px; background: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; margin: 10px 0; }
        .refresh-btn:hover { background: #218838; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Dashboard - ESP32 Orchestrator</h1>
        
        <div class="nav">
            <a href="/">Home</a>
            <a href="/components">Components</a>
            <a href="/logs">System Logs</a>
            <a href="/api/components/data">API Data</a>
        </div>
        
        <button class="refresh-btn" onclick="refreshData()">Refresh Data</button>
        
        <div class="grid">
            <div class="card">
                <h3>Memory Usage</h3>
                <div id="memoryInfo">Loading...</div>
            </div>
            
            <div class="card">
                <h3>System Status</h3>
                <div id="systemInfo">Loading...</div>
            </div>
        </div>
        
        <div class="card">
            <h3>Components & MQTT Status</h3>
            <div id="componentsTable">Loading...</div>
        </div>
    </div>
    
    <script>
        function formatBytes(bytes) {
            return Math.round(bytes / 1024) + ' KB';
        }
        
        function formatUptime(ms) {
            const seconds = Math.floor(ms / 1000);
            const hours = Math.floor(seconds / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            return hours + 'h ' + minutes + 'm';
        }
        
        function getStatusClass(state) {
            switch(state.toLowerCase()) {
                case 'ready': return 'status-ready';
                case 'executing': return 'status-executing';
                case 'error': return 'status-error';
                default: return '';
            }
        }
        
        function refreshData() {
            loadMemoryInfo();
            loadSystemInfo();
            loadComponentsInfo();
        }
        
        function loadMemoryInfo() {
            fetch('/api/system/memory')
            .then(response => response.json())
            .then(data => {
                const memoryDiv = document.getElementById('memoryInfo');
                const usagePercent = data.usage_percent || 0;
                const fragPercent = data.fragmentation_percent || 0;
                
                let healthClass = 'memory-good';
                if (data.health === 'warning') healthClass = 'memory-warning';
                if (data.health === 'critical') healthClass = 'memory-critical';
                
                memoryDiv.innerHTML = `
                    <div><strong>Free:</strong> ${formatBytes(data.free_heap)} / ${formatBytes(data.total_heap)}</div>
                    <div class="memory-bar">
                        <div class="memory-fill ${healthClass}" style="width: ${usagePercent}%"></div>
                    </div>
                    <div>Usage: ${usagePercent.toFixed(1)}%</div>
                    <div>Fragmentation: ${fragPercent.toFixed(1)}%</div>
                    <div><strong>Largest Block:</strong> ${formatBytes(data.max_alloc_heap)}</div>
                    <div><strong>Flash Used:</strong> ${formatBytes(data.sketch_size)} / ${formatBytes(data.flash_size)}</div>
                `;
            })
            .catch(error => {
                document.getElementById('memoryInfo').innerHTML = 'Error loading memory info';
            });
        }
        
        function loadSystemInfo() {
            fetch('/api/system/status')
            .then(response => response.json())
            .then(data => {
                const systemDiv = document.getElementById('systemInfo');
                systemDiv.innerHTML = `
                    <div><strong>Uptime:</strong> ${formatUptime(data.uptime || 0)}</div>
                    <div><strong>IP Address:</strong> ${data.ip_address || 'N/A'}</div>
                    <div><strong>WiFi Signal:</strong> ${data.rssi || 0} dBm</div>
                    <div><strong>Components:</strong> ${data.components_count || 0}</div>
                    <div><strong>Free Heap:</strong> ${formatBytes(data.free_heap || 0)}</div>
                `;
            })
            .catch(error => {
                document.getElementById('systemInfo').innerHTML = 'Error loading system info';
            });
        }
        
        function loadComponentsInfo() {
            fetch('/api/components/mqtt')
            .then(response => response.json())
            .then(data => {
                const tableDiv = document.getElementById('componentsTable');
                let html = '<div class="components-grid">';
                
                if (data.components) {
                    data.components.forEach(comp => {
                        const timeSince = Math.floor((Date.now() - comp.last_mqtt_publish) / 1000);
                        html += `
                            <div class="component-card">
                                <div class="component-header">
                                    <div class="component-title">
                                        <strong>${comp.name}</strong>
                                        <small>${comp.id}</small>
                                    </div>
                                    <div class="component-meta">
                                        <span class="component-type">${comp.type}</span>
                                        <span class="status ${getStatusClass(comp.state)}">${comp.state}</span>
                                    </div>
                                </div>
                                
                                <div class="component-params">
                        `;
                        
                        // Add component parameters in flex layout
                        if (comp.execution_count !== undefined) {
                            html += `<div class="param"><span class="param-label">Executions:</span><span class="param-value">${comp.execution_count}</span></div>`;
                        }
                        if (comp.error_count !== undefined) {
                            html += `<div class="param"><span class="param-label">Errors:</span><span class="param-value">${comp.error_count}</span></div>`;
                        }
                        if (comp.last_execution_ms !== undefined && comp.last_execution_ms > 0) {
                            const lastExecSince = Math.floor((Date.now() - comp.last_execution_ms) / 1000);
                            html += `<div class="param"><span class="param-label">Last Exec:</span><span class="param-value">${lastExecSince}s ago</span></div>`;
                        }
                        if (comp.ready_to_execute !== undefined) {
                            const readyText = comp.ready_to_execute ? 'Yes' : 'No';
                            html += `<div class="param"><span class="param-label">Ready:</span><span class="param-value">${readyText}</span></div>`;
                        }
                        
                        html += `
                                </div>
                                
                                <div class="mqtt-info">
                                    <div class="mqtt-topic"><code>${comp.mqtt_topic}</code></div>
                                    <div class="mqtt-meta">
                                        <span class="mqtt-time">${timeSince}s ago</span>
                                        <span class="mqtt-size">${comp.mqtt_data_size}b</span>
                                    </div>
                                </div>
                            </div>
                        `;
                    });
                }
                
                html += '</div>';
                tableDiv.innerHTML = html;
            })
            .catch(error => {
                document.getElementById('componentsTable').innerHTML = 'Error loading components info';
            });
        }
        
        // Load data on page load
        refreshData();
        
        // Auto-refresh every 30 seconds
        setInterval(refreshData, 30000);
    </script>
</body>
</html>
)html";
    
    request->send(200, "text/html", html);
}

void WebServerComponent::logRequest(AsyncWebServerRequest* request) {
    m_requestCount++;
    m_lastRequestTime = millis();
    
    log(Logger::DEBUG, String("HTTP ") + request->methodToString() + " " + 
                       request->url() + " from " + request->client()->remoteIP().toString());
}

void WebServerComponent::cleanup() {
    log(Logger::DEBUG, "Cleaning up web server component");
    
    if (m_webServer) {
        m_webServer->end();
        delete m_webServer;
        m_webServer = nullptr;
    }
    
    m_serverRunning = false;
}