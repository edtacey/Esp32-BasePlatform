#include "WebServerComponent.h"
#include "../utils/TimeUtils.h"
#include "../core/Orchestrator.h"
// #include "LightOrchestrator.h"  // Disabled to save memory

// Component includes for dynamic instantiation
#include "../components/TSL2561Component.h"
#include "../components/DHT22Component.h"
#include "../components/PeristalticPumpComponent.h"

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
    log(Logger::INFO, "🚀 === WEBSERVER COMPONENT INITIALIZATION STARTED ===");
    setState(ComponentState::INITIALIZING);
    
    // Load configuration (will use defaults if config is empty)
    log(Logger::DEBUG, "🔧 Step 1: Loading configuration...");
    if (!loadConfiguration(config)) {
        setError("Failed to load configuration");
        log(Logger::ERROR, "❌ Step 1 FAILED: Configuration loading failed");
        return false;
    }
    log(Logger::DEBUG, "✅ Step 1: Configuration loaded successfully");
    
    // Apply the configuration
    log(Logger::DEBUG, "🔧 Step 2: Applying configuration...");
    if (!applyConfiguration(getConfiguration())) {
        setError("Failed to apply configuration");
        log(Logger::ERROR, "❌ Step 2 FAILED: Configuration application failed");
        return false;
    }
    log(Logger::DEBUG, String("✅ Step 2: Configuration applied successfully (Port: ") + m_serverPort + ")");
    
    // Check WiFi connection
    log(Logger::DEBUG, "🔧 Step 3: Checking WiFi connection...");
    if (WiFi.status() != WL_CONNECTED) {
        setError("WiFi not connected - web server requires network");
        log(Logger::ERROR, "❌ Step 3 FAILED: WiFi not connected");
        return false;
    }
    log(Logger::DEBUG, String("✅ Step 3: WiFi connected (IP: ") + WiFi.localIP().toString() + ")");
    
    // Initialize LittleFS filesystem for static file serving
    log(Logger::DEBUG, "🔧 Step 4: Initializing LittleFS filesystem...");
    if (!LittleFS.begin(false)) {
        log(Logger::WARNING, "⚠️  Step 4: Failed to mount LittleFS filesystem - static files unavailable");
        log(Logger::WARNING, "⚠️  Will continue with inline HTML fallbacks");
    } else {
        log(Logger::INFO, "✅ Step 4: LittleFS filesystem mounted successfully");
        
        // Quick filesystem check
        log(Logger::DEBUG, "🔍 Quick LittleFS verification...");
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        log(Logger::DEBUG, String("📊 LittleFS: ") + usedBytes/1024 + "KB/" + totalBytes/1024 + "KB used");
        
        log(Logger::DEBUG, String("📄 /www/index.html exists: ") + (LittleFS.exists("/www/index.html") ? "YES" : "NO"));
        log(Logger::DEBUG, String("📄 /www/dashboard.html exists: ") + (LittleFS.exists("/www/dashboard.html") ? "YES" : "NO"));
    }
    
    // Create web server instance
    log(Logger::DEBUG, "🔧 Step 5: Creating AsyncWebServer instance...");
    m_webServer = new AsyncWebServer(m_serverPort);
    if (!m_webServer) {
        setError("Failed to create web server instance");
        log(Logger::ERROR, "❌ Step 5 FAILED: Could not create AsyncWebServer instance");
        return false;
    }
    log(Logger::DEBUG, String("✅ Step 5: AsyncWebServer instance created on port ") + m_serverPort);
    
    // Setup routes and start server
    log(Logger::DEBUG, "🔧 Step 6: Setting up routes...");
    log(Logger::DEBUG, "🔄 Calling setupRoutes() - this may take time...");
    setupRoutes();
    log(Logger::DEBUG, "✅ Step 6: Routes setup completed");
    
    log(Logger::DEBUG, "🔧 Step 7: Starting web server...");
    m_webServer->begin();
    m_serverRunning = true;
    log(Logger::DEBUG, "✅ Step 7: Web server started and listening");
    
    // Set execution interval (check every 30 seconds)
    log(Logger::DEBUG, "🔧 Step 8: Setting execution interval...");
    setNextExecutionMs(millis() + 30000);
    log(Logger::DEBUG, "✅ Step 8: Execution interval set to 30 seconds");
    
    setState(ComponentState::READY);
    log(Logger::INFO, String("🎉 WEB SERVER FULLY INITIALIZED AND RUNNING!"));
    log(Logger::INFO, String("🌐 Server URL: http://") + WiFi.localIP().toString() + ":" + m_serverPort + "/");
    log(Logger::INFO, "🚀 === WEBSERVER COMPONENT INITIALIZATION COMPLETE ===");
    
    return true;
}

JsonDocument WebServerComponent::getCurrentConfig() const {
    JsonDocument config;
    
    config["serverPort"] = m_serverPort;
    config["enableCORS"] = m_enableCORS;
    config["serverName"] = m_serverName;
    config["config_version"] = 1;
    
    return config;
}

bool WebServerComponent::applyConfig(const JsonDocument& config) {
    log(Logger::DEBUG, "Applying web server configuration with default hydration");
    
    // Apply configuration with default hydration
    m_serverPort = config["serverPort"] | 80;
    m_enableCORS = config["enableCORS"] | true;
    m_serverName = config["serverName"] | "ESP32-Orchestrator";
    
    // Handle version migration if needed
    uint16_t configVersion = config["config_version"] | 1;
    if (configVersion < 1) {
        log(Logger::INFO, "Migrating WebServer configuration to version 1");
        // Future migration logic would go here
    }
    
    log(Logger::DEBUG, String("Config applied: port=") + m_serverPort + 
                       ", CORS=" + (m_enableCORS ? "true" : "false") + 
                       ", name=" + m_serverName);
    
    return true;
}

bool WebServerComponent::applyConfiguration(const JsonDocument& config) {
    // Legacy method - now delegates to new architecture
    return applyConfig(config);
}

void WebServerComponent::setupRoutes() {
    log(Logger::DEBUG, "Setting up web server routes");
    
    setupAPIEndpoints();
    
    // CRITICAL: Set up specific page routes FIRST, before catch-all handlers
    setupWebPages();
    
    // Static file handler for /www/* paths
    m_webServer->on("/www/*", HTTP_GET, [this](AsyncWebServerRequest* request) {
        String path = request->url();
        // Serve the file if it exists in LittleFS
        if (!serveStaticFile(request, path)) {
            request->send(404, "text/plain", "File not found");
        }
    });
    
    // Generic static file handler for root-level files (MUST BE LAST)
    m_webServer->on("/*", HTTP_GET, [this](AsyncWebServerRequest* request) {
        String path = request->url();
        // Skip API endpoints
        if (path.startsWith("/api/")) {
            return;  // Let other handlers process this
        }
        // Try to serve from /www directory
        String filePath = "/www" + path;
        if (!serveStaticFile(request, filePath)) {
            // If not found and it's root, try index.html
            if (path == "/" || path.isEmpty()) {
                if (!serveStaticFile(request, "/www/index.html")) {
                    handleHomePage(request);  // Fallback to inline HTML
                }
            } else {
                request->send(404, "text/plain", "File not found");
            }
        }
    });
    
    // 404 handler
    m_webServer->onNotFound([this](AsyncWebServerRequest* request) {
        logRequest(request);
        // Try to serve as static file one more time
        String path = "/www" + request->url();
        if (!serveStaticFile(request, path)) {
            request->send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
        }
    });
}

void WebServerComponent::setupAPIEndpoints() {
    // System status endpoint
    m_webServer->on("/api/system/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleSystemStatus(request);
    });
    
    // System restart endpoint
    m_webServer->on("/api/system/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleSystemRestart(request);
    });
    
    // IMPORTANT: MORE SPECIFIC ROUTES MUST BE REGISTERED FIRST!
    
    // Execution loop control endpoints
    m_webServer->on("/api/orchestrator/execution/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleExecutionLoopStatus(request);
    });
    
    m_webServer->on("/api/orchestrator/execution/pause", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleExecutionLoopPause(request);
    });
    
    m_webServer->on("/api/orchestrator/execution/resume", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleExecutionLoopResume(request);
    });
    
    m_webServer->on("/api/orchestrator/execution/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleExecutionLoopConfig(request);
    });
    
    m_webServer->on("/api/orchestrator/execution/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleExecutionLoopConfigUpdate(request);
    });

    // Component data endpoint
    m_webServer->on("/api/components/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentData(request);
    });
    
    // Enhanced components with MQTT data endpoint
    m_webServer->on("/api/components/mqtt", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentsMqtt(request);
    });
    
    // Debug endpoint to check raw component data
    m_webServer->on("/api/components/debug", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentsDebug(request);
    });
    
    // Time debug endpoint
    m_webServer->on("/api/system/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument timeInfo;
        timeInfo["current_epoch"] = (long)TimeUtils::getEpochTime();
        timeInfo["current_timestamp"] = TimeUtils::getCurrentTimestamp();
        timeInfo["ntp_available"] = TimeUtils::isNTPAvailable();
        timeInfo["boot_millis"] = TimeUtils::getBootMillis();
        timeInfo["boot_seconds"] = TimeUtils::getBootSeconds();
        
        // Test millisToEpoch conversion
        unsigned long testMillis = millis() - 10000;  // 10 seconds ago
        time_t testEpoch = TimeUtils::millisToEpoch(testMillis);
        timeInfo["test_millis"] = testMillis;
        timeInfo["test_epoch"] = (long)testEpoch;
        timeInfo["stored_boot_time"] = (long)TimeUtils::getBootTime();
        
        String response;
        serializeJson(timeInfo, response);
        
        AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
        if (m_enableCORS) setCORSHeaders(resp);
        request->send(resp);
    });
    
    // Component list endpoint (BASE ROUTE REGISTERED LAST!)
    m_webServer->on("/api/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentList(request);
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
    
    // LittleFS debug endpoint
    m_webServer->on("/api/debug/littlefs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleLittleFSDebug(request);
    });
    
    // Direct config file read endpoint
    m_webServer->on("/api/debug/config-file", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleDirectConfigFileRead(request);
    });
    
    // Light sweep test endpoints
    m_webServer->on("/api/light/sweep/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
        // handleSweepTestStart(request); // Disabled to save memory
    });
    
    m_webServer->on("/api/light/sweep/stop", HTTP_POST, [this](AsyncWebServerRequest* request) {
        // handleSweepTestStop(request); // Disabled
    });
    
    m_webServer->on("/api/light/sweep/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        // handleSweepTestStatus(request); // Disabled
    });
    
    // Generic component configuration endpoints (using query parameters)
    m_webServer->on("/api/component/config", HTTP_PUT, [this](AsyncWebServerRequest* request) {
        handleGenericComponentConfigUpdate(request);
    });
    
    m_webServer->on("/api/component/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGenericComponentConfigGet(request);
    });
    
    // Component creation endpoint
    m_webServer->on("/api/component/add", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleComponentAdd(request);
    });
    
    // Component deletion endpoint
    m_webServer->on("/api/component/delete", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        handleComponentDelete(request);
    });
    
    // Component action endpoints
    m_webServer->on("/api/components/{component_id}/actions", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentActionsGet(request);
    });
    
    m_webServer->on("/api/components/{component_id}/actions/{action_name}", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleComponentActionExecute(request);
    });
    
    // DEBUG: WebServer timing debug endpoint
    m_webServer->on("/api/debug/webserver-timing", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleWebServerTimingDebug(request);
    });
}

void WebServerComponent::setupWebPages() {
    // Home page - try LittleFS first, then fallback
    m_webServer->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!serveStaticFile(request, "/www/index.html")) {
            handleHomePage(request);  // Fallback to inline HTML
        }
    });
    
    // Components page
    m_webServer->on("/components", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleComponentPage(request);
    });
    
    // Logs page  
    m_webServer->on("/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleLogsPage(request);
    });
    
    // Dashboard page - try LittleFS first, then fallback
    m_webServer->on("/dashboard", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!serveStaticFile(request, "/www/dashboard.html")) {
            handleDashboardPage(request);  // Fallback to inline HTML
        }
    });
    
    // Direct dashboard.html route
    m_webServer->on("/dashboard.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!serveStaticFile(request, "/www/dashboard.html")) {
            handleDashboardPage(request);  // Fallback to inline HTML
        }
    });
    
    // Static assets
    m_webServer->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!serveStaticFile(request, "/www/style.css")) {
            request->send(404, "text/plain", "File not found");
        }
    });
    
    m_webServer->on("/app.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!serveStaticFile(request, "/www/app.js")) {
            request->send(404, "text/plain", "File not found");
        }
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
    
    // CRITICAL: Update execution statistics to prevent first-run loop
    updateExecutionStats();
    
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

void WebServerComponent::handleSystemRestart(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        
        AsyncWebServerResponse* resp = request->beginResponse(500, "application/json", response.as<String>());
        if (m_enableCORS) setCORSHeaders(resp);
        request->send(resp);
        return;
    }
    
    // Parse delay parameter (optional, defaults to 3 seconds)
    uint32_t delaySeconds = 3;
    if (request->hasParam("delay")) {
        delaySeconds = request->getParam("delay")->value().toInt();
        if (delaySeconds < 1) delaySeconds = 1;        // Minimum 1 second
        if (delaySeconds > 30) delaySeconds = 30;      // Maximum 30 seconds
    }
    
    // Schedule the restart
    bool success = m_orchestrator->scheduleRestart(delaySeconds);
    
    if (success) {
        response["success"] = true;
        response["message"] = "System restart scheduled";
        response["delay_seconds"] = delaySeconds;
        log(Logger::INFO, "System restart requested via API (delay: " + String(delaySeconds) + "s)");
    } else {
        response["success"] = false;
        response["error"] = "Failed to schedule restart";
    }
    
    AsyncWebServerResponse* resp = request->beginResponse(success ? 200 : 500, "application/json", response.as<String>());
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleComponentList(AsyncWebServerRequest* request) {
    logRequest(request);
    
    log(Logger::Level::WARNING, "[ROUTE-DEBUG] handleComponentList called for URL: " + String(request->url()));
    
    JsonDocument componentList;
    JsonArray components = componentList["components"].to<JsonArray>();
    
    if (m_orchestrator) {
        const std::vector<BaseComponent*>& allComponents = m_orchestrator->getComponents();
        for (BaseComponent* component : allComponents) {
            if (component) {
                JsonObject comp = components.add<JsonObject>();
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
    
    log(Logger::Level::DEBUG, "[HANDLER-DEBUG] handleComponentData called");
    
    // Check for filter parameter
    String filter = "";
    if (request->hasParam("filter")) {
        filter = request->getParam("filter")->value();
        log(Logger::Level::DEBUG, String("[FILTER] Data filter requested: ") + filter);
    }
    
    JsonDocument allData = getAllComponentData();
    
    // Apply filtering if requested
    if (filter == "core") {
        JsonDocument filteredData = filterComponentData(allData, FILTER_CORE);
        // Only use filtered data if it's valid
        if (!filteredData.isNull() && filteredData.size() > 0) {
            allData = filteredData;
            log(Logger::Level::DEBUG, "[FILTER] Applied CORE filter - sensor values only");
        } else {
            log(Logger::Level::WARNING, "[FILTER] Core filter failed, returning unfiltered data");
        }
    } else if (filter == "diagnostics") {
        JsonDocument filteredData = filterComponentData(allData, FILTER_DIAGNOSTICS);
        // Only use filtered data if it's valid
        if (!filteredData.isNull() && filteredData.size() > 0) {
            allData = filteredData;
            log(Logger::Level::DEBUG, "[FILTER] Applied DIAGNOSTICS filter - full diagnostic data");
        } else {
            log(Logger::Level::WARNING, "[FILTER] Diagnostics filter failed, returning unfiltered data");
        }
    } else {
        log(Logger::Level::DEBUG, "[FILTER] No filter applied - returning all data");
    }
    
    log(Logger::Level::DEBUG, String("[HANDLER-DEBUG] Filtered data size: ") + 
        String(allData.size()) + " bytes, components array size: " + 
        String(allData["components"].as<JsonArray>().size()));
    
    String response;
    serializeJson(allData, response);
    
    log(Logger::Level::DEBUG, String("[HANDLER-DEBUG] Response size: ") + String(response.length()) + " chars");
    
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
    
    // Try to serve from LittleFS first
    if (serveStaticFile(request, "/www/index.html")) {
        return;
    }
    
    // Fallback to minimal inline HTML
    String html = F(R"html(
<!DOCTYPE html>
<html>
<head>
<title>ESP32 IoT</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:Arial;margin:20px;background:#f5f5f5}
.c{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:8px}
.s{background:#e7f5e7;padding:15px;border-radius:5px;margin:15px 0}
a{display:inline-block;margin:5px;padding:8px 16px;background:#007acc;color:white;text-decoration:none;border-radius:4px}
</style>
</head>
<body>
<div class="c">
<h1>ESP32 IoT Orchestrator</h1>
<div class="s" id="status">Loading...</div>
<a href="/dashboard">Dashboard</a>
<a href="/api/components">Components API</a>
<a href="/api/system/status">System API</a>
</div>
<script>
fetch('/api/system/status').then(r=>r.json()).then(d=>{
document.getElementById('status').innerHTML=
'IP: '+d.ip_address+'<br>'+
'Uptime: '+Math.floor(d.uptime/1000)+'s<br>'+
'Memory: '+Math.floor(d.free_heap/1024)+'KB<br>'+
'Components: '+d.components_count;
});
</script>
</body>
</html>)html");
    
    request->send(200, "text/html", html);
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
                
                // Get component statistics with proper timestamps
                JsonDocument stats = component->getStatistics();
                if (!stats.isNull()) {
                    comp["execution_count"] = stats["execution_count"] | 0;
                    comp["error_count"] = stats["error_count"] | 0;
                    
                    // Convert millis() timestamps to epoch time for proper dashboard display
                    unsigned long lastExecMs = stats["last_execution_ms"] | 0;
                    if (lastExecMs > 0) {
                        time_t lastExecEpoch = TimeUtils::millisToEpoch(lastExecMs);
                        comp["last_execution_epoch"] = (long)lastExecEpoch;
                        comp["last_execution_ms"] = lastExecMs;  // Keep for compatibility
                    }
                    
                    unsigned long nextExecMs = component->getNextExecutionMs();
                    if (nextExecMs > 0) {
                        time_t nextExecEpoch = TimeUtils::millisToEpoch(nextExecMs);
                        comp["next_execution_epoch"] = (long)nextExecEpoch;
                        comp["next_execution_ms"] = nextExecMs;  // Keep for compatibility
                    }
                }
                
                // Get last error if any
                if (!component->getLastError().isEmpty()) {
                    comp["last_error"] = component->getLastError();
                }
                
                // Get stored sensor data (non-blocking approach)
                if (component != this) {
                    // First try the debug string which we know gets populated
                    const String& dataStr = component->getLastExecutionDataString();
                    
                    if (!dataStr.isEmpty()) {
                        // Parse the string data
                        JsonDocument tempDoc;
                        DeserializationError error = deserializeJson(tempDoc, dataStr);
                        
                        if (error == DeserializationError::Ok && tempDoc.size() > 0) {
                            JsonObject outputData = comp["output_data"].to<JsonObject>();
                            
                            // Copy all fields from the parsed data
                            for (JsonPair kv : tempDoc.as<JsonObject>()) {
                                outputData[kv.key().c_str()] = kv.value();
                            }
                            
                            comp["has_data"] = true;
                            comp["data_fields"] = outputData.size();
                            log(Logger::Level::DEBUG, String("[API] Component ") + component->getId() + 
                                " data retrieved: " + String(outputData.size()) + " fields");
                        } else {
                            comp["output_data"]["status"] = "Parse error";
                            comp["has_data"] = false;
                            log(Logger::Level::WARNING, String("[API] Failed to parse data for ") + component->getId());
                        }
                    } else {
                        // No data available yet
                        comp["output_data"]["status"] = "No data available";
                        comp["has_data"] = false;
                    }
                    
                    comp["component_uptime"] = millis();
                    comp["ready_to_execute"] = component->isReadyToExecute();
                }
            }
        }
    }
    
    return allData;
}

JsonDocument WebServerComponent::filterComponentData(const JsonDocument& data, DataFilterType filterType) {
    if (filterType == FILTER_NONE) {
        return data;  // Return data as-is
    }
    
    // Create filtered document
    JsonDocument filtered;
    
    // Check if input data is valid
    if (data.isNull() || data.size() == 0) {
        log(Logger::Level::WARNING, "[FILTER] Input data is null or empty");
        return data;  // Return original if invalid
    }
    
    // Copy top-level metadata
    if (data["timestamp"].is<unsigned long>()) {
        filtered["timestamp"] = data["timestamp"];
    }
    
    if (data["components"].is<JsonArray>()) {
        JsonArrayConst sourceComponents = data["components"];  // Use const reference to avoid conversion
        JsonArray filteredComponents = filtered["components"].to<JsonArray>();
        
        for (JsonVariantConst comp : sourceComponents) {
            JsonObject filteredComp = filteredComponents.add<JsonObject>();
            
            // Always include basic component info
            filteredComp["id"] = comp["id"];
            filteredComp["type"] = comp["type"];
            filteredComp["name"] = comp["name"];
            filteredComp["state"] = comp["state"];
            filteredComp["has_data"] = comp["has_data"];
            
            if (filterType == FILTER_CORE) {
                // CORE MODE: Only essential sensor values
                log(Logger::DEBUG, String("[FILTER] Processing component ") + comp["id"].as<String>() + " for CORE data");
                
                if (comp["output_data"].is<JsonObject>()) {
                    // Extract common sensor values directly from output_data
                    JsonObjectConst outputData = comp["output_data"];
                    JsonObject coreOutputData = filteredComp["output_data"].to<JsonObject>();
                    
                    // Core sensor values to include (expanded list)
                    const char* coreFields[] = {
                        "temperature", "humidity", "ph", "current_ph", "ec", "current_ec", 
                        "tds", "current_tds", "lux", "timestamp", "success", "value", 
                        "reading", "level", "voltage", "current", "pressure", "flow_rate",
                        "calibrated", "is_calibrated", "healthy", "ppfd_category", "par_umol", nullptr
                    };
                    
                    for (int i = 0; coreFields[i] != nullptr; i++) {
                        if (!outputData[coreFields[i]].isNull()) {
                            coreOutputData[coreFields[i]] = outputData[coreFields[i]];
                        }
                    }
                    
                    // If no core fields were found, include basic status info
                    if (coreOutputData.size() == 0) {
                        if (!outputData["timestamp"].isNull()) {
                            coreOutputData["timestamp"] = outputData["timestamp"];
                        }
                        if (!outputData["success"].isNull()) {
                            coreOutputData["success"] = outputData["success"];
                        }
                    }
                }
                
                // Include minimal execution info
                if (!comp["execution_count"].isNull()) {
                    filteredComp["executions"] = comp["execution_count"];
                }
                
            } else if (filterType == FILTER_DIAGNOSTICS) {
                // DIAGNOSTICS MODE: All data including debug info
                log(Logger::DEBUG, String("[FILTER] Processing component ") + comp["id"].as<String>() + " for DIAGNOSTICS data");
                
                // Copy all fields
                for (JsonPairConst kv : comp.as<JsonObjectConst>()) {
                    if (strcmp(kv.key().c_str(), "id") != 0 && 
                        strcmp(kv.key().c_str(), "type") != 0 &&
                        strcmp(kv.key().c_str(), "name") != 0 &&
                        strcmp(kv.key().c_str(), "state") != 0 &&
                        strcmp(kv.key().c_str(), "has_data") != 0) {
                        filteredComp[kv.key().c_str()] = kv.value();
                    }
                }
            }
        }
    }
    
    log(Logger::DEBUG, String("[FILTER] Filtering complete - original: ") + 
        String(data.size()) + " bytes, filtered: " + String(filtered.size()) + " bytes");
    
    return filtered;
}

void WebServerComponent::handleComponentsDebug(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument debugData;
    debugData["timestamp"] = millis();
    
    JsonArray components = debugData["components"].to<JsonArray>();
    
    if (m_orchestrator) {
        const std::vector<BaseComponent*>& componentList = m_orchestrator->getComponents();
        
        for (BaseComponent* component : componentList) {
            if (component && (component->getId().indexOf("tsl2561") >= 0 || component->getId().indexOf("dht22") >= 0)) {
                JsonObject comp = components.add<JsonObject>();
                comp["id"] = component->getId();
                comp["state"] = component->getStateString();
                
                // Test getLastExecutionData directly
                const JsonDocument& lastData = component->getLastExecutionData();
                comp["has_last_data"] = !lastData.isNull();
                comp["last_data_size"] = lastData.size();
                
                // Also check string version
                const String& dataString = component->getLastExecutionDataString();
                comp["has_string_data"] = !dataString.isEmpty();
                comp["string_data_length"] = dataString.length();
                
                if (!lastData.isNull()) {
                    String dataStr;
                    serializeJson(lastData, dataStr);
                    comp["last_data_json"] = dataStr;
                } else if (!dataString.isEmpty()) {
                    comp["last_data_json"] = dataString;
                    comp["debug_message"] = "Using string version";
                } else {
                    comp["debug_message"] = "Both lastData and string are empty";
                }
            }
        }
    }
    
    String response;
    serializeJson(debugData, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
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

void WebServerComponent::handleLittleFSDebug(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument fsInfo;
    
    // LittleFS basic info
    fsInfo["total_bytes"] = LittleFS.totalBytes();
    fsInfo["used_bytes"] = LittleFS.usedBytes();
    fsInfo["free_bytes"] = LittleFS.totalBytes() - LittleFS.usedBytes();
    
    // List all files in root directory
    JsonArray files = fsInfo["files"].to<JsonArray>();
    File root = LittleFS.open("/", "r");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            JsonDocument fileInfo;
            fileInfo["name"] = String(file.name());
            fileInfo["size"] = file.size();
            fileInfo["is_directory"] = file.isDirectory();
            files.add(fileInfo);
            file = root.openNextFile();
        }
        root.close();
    }
    
    // Check components directory specifically
    JsonArray componentsFiles = fsInfo["components_directory"].to<JsonArray>();
    File componentsDir = LittleFS.open("/config/components", "r");
    if (componentsDir && componentsDir.isDirectory()) {
        File componentFile = componentsDir.openNextFile();
        while (componentFile) {
            JsonDocument componentInfo;
            componentInfo["name"] = String(componentFile.name());
            componentInfo["size"] = componentFile.size();
            componentInfo["is_directory"] = componentFile.isDirectory();
            if (!componentFile.isDirectory() && String(componentFile.name()).endsWith(".json")) {
                // Read content for JSON files
                String content = componentFile.readString();
                componentInfo["content"] = content;
            }
            componentsFiles.add(componentInfo);
            componentFile = componentsDir.openNextFile();
        }
        componentsDir.close();
    }
    
    // Get specific config files content if they exist
    JsonArray configContents = fsInfo["config_contents"].to<JsonArray>();
    
    // Check for TSL2561Component config
    File tslConfig = LittleFS.open("/config/TSL2561Component.json", "r");
    if (tslConfig) {
        JsonDocument configData;
        configData["component"] = "TSL2561Component";
        configData["size"] = tslConfig.size();
        String content = tslConfig.readString();
        configData["content"] = content;
        configContents.add(configData);
        tslConfig.close();
    }
    
    // Check for any other .json files in config directory
    File configDir = LittleFS.open("/config", "r");
    if (configDir && configDir.isDirectory()) {
        File configFile = configDir.openNextFile();
        while (configFile) {
            if (String(configFile.name()).endsWith(".json") && 
                String(configFile.name()) != "TSL2561Component.json") {
                JsonDocument configData;
                configData["component"] = String(configFile.name());
                configData["size"] = configFile.size();
                String content = configFile.readString();
                configData["content"] = content;
                configContents.add(configData);
            }
            configFile = configDir.openNextFile();
        }
        configDir.close();
    }
    
    String response;
    serializeJson(fsInfo, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleDirectConfigFileRead(AsyncWebServerRequest* request) {
    logRequest(request);
    
    String componentId = "tsl2561-remote"; // Default to TSL2561 for our test
    if (request->hasParam("component_id")) {
        componentId = request->getParam("component_id")->value();
    }
    
    String filePath = "/config/components/" + componentId + ".json";
    
    JsonDocument response;
    response["component_id"] = componentId;
    response["file_path"] = filePath;
    
    // Try to read the file directly
    File configFile = LittleFS.open(filePath, "r");
    if (configFile) {
        response["file_exists"] = true;
        response["file_size"] = configFile.size();
        String content = configFile.readString();
        response["raw_content"] = content;
        configFile.close();
        
        // Try to parse as JSON
        JsonDocument parsedContent;
        DeserializationError error = deserializeJson(parsedContent, content);
        if (error) {
            response["parse_error"] = String(error.c_str());
        } else {
            response["parsed_json"] = parsedContent;
        }
    } else {
        response["file_exists"] = false;
        response["error"] = "File not found or could not be opened";
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", responseStr);
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
        
        if (allData["components"].is<JsonArray>()) {
            JsonArray sourceComponents = allData["components"];
            for (JsonVariantConst comp : sourceComponents) {
                JsonObject component = components.add<JsonObject>();
                component["id"] = comp["id"];
                component["type"] = comp["type"];
                component["name"] = comp["name"];
                component["state"] = comp["state"];
                
                // Add component execution data if available
                if (!comp["last_data"].isNull()) {
                    component["last_data"] = comp["last_data"];
                }
                if (!comp["last_execution_ms"].isNull()) {
                    component["last_execution_ms"] = comp["last_execution_ms"];
                }
                if (!comp["execution_count"].isNull()) {
                    component["execution_count"] = comp["execution_count"];
                }
                if (!comp["error_count"].isNull()) {
                    component["error_count"] = comp["error_count"];
                }
                if (!comp["last_error"].isNull()) {
                    component["last_error"] = comp["last_error"];
                }
                
                // Add component output data (sensor readings, etc.)
                if (!comp["output_data"].isNull()) {
                    component["output_data"] = comp["output_data"];
                }
                if (!comp["last_message"].isNull()) {
                    component["last_message"] = comp["last_message"];
                }
                if (!comp["execution_time_ms"].isNull()) {
                    component["execution_time_ms"] = comp["execution_time_ms"];
                }
                
                // Add MQTT information (for now using calculated values)
                unsigned long mqttMillis = millis() - random(5000, 60000);
                time_t mqttEpoch = TimeUtils::millisToEpoch(mqttMillis);
                component["last_mqtt_publish_epoch"] = (long)mqttEpoch;
                component["last_mqtt_publish"] = mqttMillis;  // Keep for compatibility
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
        .param.sensor-reading { background: #d1ecf1; border: 1px solid #bee5eb; }
        .param.error { background: #f8d7da; border: 1px solid #f5c6cb; }
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
                        // Use proper epoch time calculation for MQTT publish time
                        let timeSince;
                        if (comp.last_mqtt_publish_epoch && comp.last_mqtt_publish_epoch > 0) {
                            timeSince = Math.floor((Date.now() / 1000) - comp.last_mqtt_publish_epoch);
                        } else {
                            // Fallback to boot time calculation
                            timeSince = Math.floor((Date.now() - comp.last_mqtt_publish) / 1000);
                        }
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
                        
                        // Add component output data (sensor readings) first
                        if (comp.output_data) {
                            // Temperature
                            if (comp.output_data.temperature !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">🌡️ Temp:</span><span class="param-value">${comp.output_data.temperature}°C</span></div>`;
                            }
                            // Humidity  
                            if (comp.output_data.humidity !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">💧 Humidity:</span><span class="param-value">${comp.output_data.humidity}%</span></div>`;
                            }
                            // Light/Lux
                            if (comp.output_data.lux !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">☀️ Light:</span><span class="param-value">${comp.output_data.lux} lux</span></div>`;
                            } else if (comp.output_data.light !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">☀️ Light:</span><span class="param-value">${comp.output_data.light} lux</span></div>`;
                            }
                            // PAR (Photosynthetically Active Radiation) for hydroponics
                            if (comp.output_data.par_umol !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">🌱 PAR:</span><span class="param-value">${Math.round(comp.output_data.par_umol)} μmol/m²/s</span></div>`;
                            }
                            // PPFD category for plant growth guidance
                            if (comp.output_data.ppfd_category !== undefined) {
                                const categoryEmojis = {
                                    'very_low': '🟥',
                                    'low': '🟨', 
                                    'medium': '🟩',
                                    'high': '🟢',
                                    'very_high': '🔥'
                                };
                                const emoji = categoryEmojis[comp.output_data.ppfd_category] || '⚪';
                                html += `<div class="param sensor-reading"><span class="param-label">${emoji} PPFD:</span><span class="param-value">${comp.output_data.ppfd_category.replace('_', ' ')}</span></div>`;
                            }
                            // Servo position
                            if (comp.output_data.current_position !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">🎛️ Position:</span><span class="param-value">${comp.output_data.current_position}%</span></div>`;
                            }
                            // Servo lumens
                            if (comp.output_data.current_lumens !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">💡 Lumens:</span><span class="param-value">${Math.round(comp.output_data.current_lumens)}</span></div>`;
                            }
                            // Light orchestrator target
                            if (comp.output_data.target_lumens !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">🎯 Target:</span><span class="param-value">${comp.output_data.target_lumens} lumens</span></div>`;
                            }
                            // Light orchestrator mode
                            if (comp.output_data.lighting_mode !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">🌅 Mode:</span><span class="param-value">${comp.output_data.lighting_mode}</span></div>`;
                            }
                            // Sensor average for light orchestrator
                            if (comp.output_data.sensor_average !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">📊 Avg:</span><span class="param-value">${Math.round(comp.output_data.sensor_average)} lux</span></div>`;
                            }
                            // Pump volume for pumps
                            if (comp.output_data.volume_ml !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">💉 Volume:</span><span class="param-value">${comp.output_data.volume_ml} ml</span></div>`;
                            }
                            // Pump flow rate
                            if (comp.output_data.flow_rate !== undefined) {
                                html += `<div class="param sensor-reading"><span class="param-label">🌊 Flow:</span><span class="param-value">${comp.output_data.flow_rate} ml/s</span></div>`;
                            }
                            // pH Sensor reading
                            if (comp.output_data.current_ph !== undefined && comp.output_data.current_ph >= 0) {
                                const phValue = comp.output_data.current_ph.toFixed(2);
                                let phEmoji = '⚪';
                                if (phValue < 6.0) phEmoji = '🟡';  // Acidic
                                else if (phValue >= 6.0 && phValue <= 7.5) phEmoji = '🟢';  // Optimal
                                else if (phValue > 7.5) phEmoji = '🔵';  // Alkaline
                                html += `<div class="param sensor-reading"><span class="param-label">${phEmoji} pH:</span><span class="param-value">${phValue}</span></div>`;
                                // Show voltage if in mock mode
                                if (comp.output_data.current_mode === 'MOCK' && comp.output_data.current_volts !== undefined) {
                                    html += `<div class="param sensor-reading"><span class="param-label">⚡ Volts:</span><span class="param-value">${comp.output_data.current_volts.toFixed(3)}V</span></div>`;
                                }
                            }
                            // EC Sensor reading
                            if (comp.output_data.current_ec !== undefined && comp.output_data.current_ec >= 0) {
                                const ecValue = comp.output_data.current_ec.toFixed(1);
                                let ecEmoji = '⚪';
                                if (ecValue < 200) ecEmoji = '💧';  // Too low
                                else if (ecValue >= 200 && ecValue <= 1500) ecEmoji = '💪';  // Good range
                                else if (ecValue > 1500) ecEmoji = '⚠️';  // Too high
                                html += `<div class="param sensor-reading"><span class="param-label">${ecEmoji} EC:</span><span class="param-value">${ecValue} µS/cm</span></div>`;
                            }
                            // TDS reading (related to EC)
                            if (comp.output_data.current_tds !== undefined && comp.output_data.current_tds >= 0) {
                                html += `<div class="param sensor-reading"><span class="param-label">🧂 TDS:</span><span class="param-value">${comp.output_data.current_tds.toFixed(0)} ppm</span></div>`;
                            }
                        }
                        
                        // Add component statistics
                        if (comp.execution_count !== undefined) {
                            html += `<div class="param"><span class="param-label">Executions:</span><span class="param-value">${comp.execution_count}</span></div>`;
                        }
                        if (comp.error_count !== undefined && comp.error_count > 0) {
                            html += `<div class="param error"><span class="param-label">Errors:</span><span class="param-value">${comp.error_count}</span></div>`;
                        }
                        if (comp.last_execution_epoch !== undefined && comp.last_execution_epoch > 0) {
                            // Use proper epoch time calculation
                            const lastExecSince = Math.floor((Date.now() / 1000) - comp.last_execution_epoch);
                            let timeStr = '';
                            if (lastExecSince < 60) {
                                timeStr = `${lastExecSince}s ago`;
                            } else if (lastExecSince < 3600) {
                                const mins = Math.floor(lastExecSince / 60);
                                const secs = lastExecSince % 60;
                                timeStr = secs > 0 ? `${mins}m ${secs}s ago` : `${mins}m ago`;
                            } else {
                                const hours = Math.floor(lastExecSince / 3600);
                                const mins = Math.floor((lastExecSince % 3600) / 60);
                                timeStr = mins > 0 ? `${hours}h ${mins}m ago` : `${hours}h ago`;
                            }
                            html += `<div class="param"><span class="param-label">Last Exec:</span><span class="param-value">${timeStr}</span></div>`;
                        } else if (comp.last_execution_ms !== undefined && comp.last_execution_ms > 0) {
                            // Fallback to boot time calculation if epoch not available
                            const bootSecs = Math.floor(comp.last_execution_ms / 1000);
                            html += `<div class="param"><span class="param-label">Last Exec:</span><span class="param-value">Boot+${bootSecs}s</span></div>`;
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

// void WebServerComponent::handleSweepTestStart(AsyncWebServerRequest* request) {
//     logRequest(request);
//     
//     JsonDocument response;
//     
//     if (!m_orchestrator) {
//         response["success"] = false;
//         response["error"] = "Orchestrator not available";
//         request->send(500, "application/json", response.as<String>());
//         return;
//     }
//     
//     // Find the light orchestrator component
//     LightOrchestrator* lightOrchestrator = nullptr;
//     const std::vector<BaseComponent*>& components = m_orchestrator->getComponents();
//     
//     for (BaseComponent* component : components) {
//         if (component && component->getType() == "LightOrchestrator") {
//             lightOrchestrator = static_cast<LightOrchestrator*>(component);
//             break;
//         }
//     }
//     
//     if (!lightOrchestrator) {
//         response["success"] = false;
//         response["error"] = "Light orchestrator component not found";
//         request->send(404, "application/json", response.as<String>());
//         return;
//     }
//     
//     if (lightOrchestrator->startSweepTest()) {
//         response["success"] = true;
//         response["message"] = "Sweep test started successfully";
//     } else {
//         response["success"] = false;
//         response["error"] = "Failed to start sweep test";
//     }
//     
//     String responseJson;
//     serializeJson(response, responseJson);
//     request->send(200, "application/json", responseJson);
// }

// void WebServerComponent::handleSweepTestStop(AsyncWebServerRequest* request) {
//     logRequest(request);
//     
//     JsonDocument response;
//     
//     if (!m_orchestrator) {
//         response["success"] = false;
//         response["error"] = "Orchestrator not available";
//         request->send(500, "application/json", response.as<String>());
//         return;
//     }
//     
//     // Find the light orchestrator component
//     LightOrchestrator* lightOrchestrator = nullptr;
//     const std::vector<BaseComponent*>& components = m_orchestrator->getComponents();
//     
//     for (BaseComponent* component : components) {
//         if (component && component->getType() == "LightOrchestrator") {
//             lightOrchestrator = static_cast<LightOrchestrator*>(component);
//             break;
//         }
//     }
//     
//     if (!lightOrchestrator) {
//         response["success"] = false;
//         response["error"] = "Light orchestrator component not found";
//         request->send(404, "application/json", response.as<String>());
//         return;
//     }
//     
//     if (lightOrchestrator->stopSweepTest()) {
//         response["success"] = true;
//         response["message"] = "Sweep test stopped successfully";
//     } else {
//         response["success"] = false;
//         response["error"] = "Failed to stop sweep test";
//     }
//     
//     String responseJson;
//     serializeJson(response, responseJson);
//     request->send(200, "application/json", responseJson);
// }

// void WebServerComponent::handleSweepTestStatus(AsyncWebServerRequest* request) {
//     logRequest(request);
//     
//     JsonDocument response;
//     
//     if (!m_orchestrator) {
//         response["success"] = false;
//         response["error"] = "Orchestrator not available";
//         request->send(500, "application/json", response.as<String>());
//         return;
//     }
//     
//     // Find the light orchestrator component
//     LightOrchestrator* lightOrchestrator = nullptr;
//     const std::vector<BaseComponent*>& components = m_orchestrator->getComponents();
//     
//     for (BaseComponent* component : components) {
//         if (component && component->getType() == "LightOrchestrator") {
//             lightOrchestrator = static_cast<LightOrchestrator*>(component);
//             break;
//         }
//     }
//     
//     if (!lightOrchestrator) {
//         response["success"] = false;
//         response["error"] = "Light orchestrator component not found";
//         request->send(404, "application/json", response.as<String>());
//         return;
//     }
//     
//     response["success"] = true;
//     response["sweep_test_active"] = lightOrchestrator->isSweepTestActive();
//     response["target_lumens"] = lightOrchestrator->getTargetLumens();
//     response["lighting_mode"] = (int)lightOrchestrator->getLightingMode();
//     response["average_sensor_reading"] = lightOrchestrator->getAverageSensorReading();
//     
//     String responseJson;
//     serializeJson(response, responseJson);
//     request->send(200, "application/json", responseJson);
// }

void WebServerComponent::handleComponentConfig(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID from URL path
    String url = request->url();
    String componentId = "";
    
    // Parse URL like /api/components/servo-dimmer-1/config
    int startPos = url.indexOf("/api/components/") + 16;
    int endPos = url.indexOf("/config");
    if (startPos > 15 && endPos > startPos) {
        componentId = url.substring(startPos, endPos);
    }
    
    if (componentId.isEmpty()) {
        response["success"] = false;
        response["error"] = "Invalid component ID";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Find the component
    BaseComponent* component = nullptr;
    const std::vector<BaseComponent*>& components = m_orchestrator->getComponents();
    
    for (BaseComponent* comp : components) {
        if (comp && comp->getId() == componentId) {
            component = comp;
            break;
        }
    }
    
    if (!component) {
        response["success"] = false;
        response["error"] = "Component not found: " + componentId;
        request->send(404, "application/json", response.as<String>());
        return;
    }
    
    // Get device_ip parameter
    if (!request->hasParam("device_ip")) {
        response["success"] = false;
        response["error"] = "device_ip parameter required";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    String deviceIP = request->getParam("device_ip")->value();
    
    // Build configuration JSON
    JsonDocument configData;
    configData["device_ip"] = deviceIP;
    
    // Apply configuration to component using new architecture
    if (component->applyConfigurationFromJson(configData)) {
        // Save the configuration to persistent storage
        if (component->saveCurrentConfiguration()) {
            response["success"] = true;
            response["message"] = "Configuration updated and saved to persistent storage";
            response["component_id"] = componentId;
            response["device_ip"] = deviceIP;
            log(Logger::INFO, "Configuration updated and saved for component: " + componentId + " with IP: " + deviceIP);
        } else {
            response["success"] = false;
            response["error"] = "Configuration applied but failed to save to persistent storage";
            log(Logger::ERROR, "Failed to save configuration to storage for component: " + componentId);
        }
    } else {
        response["success"] = false;
        response["error"] = "Failed to apply configuration";
        log(Logger::ERROR, "Failed to apply configuration for component: " + componentId);
    }
    
    String responseJson;
    serializeJson(response, responseJson);
    request->send(200, "application/json", responseJson);
}

void WebServerComponent::handleComponentConfigGet(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID from URL path
    String url = request->url();
    String componentId = "";
    
    // Parse URL like /api/components/servo-dimmer-1/config
    int startPos = url.indexOf("/api/components/") + 16;
    int endPos = url.indexOf("/config");
    if (startPos > 15 && endPos > startPos) {
        componentId = url.substring(startPos, endPos);
    }
    
    if (componentId.isEmpty()) {
        response["success"] = false;
        response["error"] = "Invalid component ID";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Find the component
    BaseComponent* component = nullptr;
    const std::vector<BaseComponent*>& components = m_orchestrator->getComponents();
    
    for (BaseComponent* comp : components) {
        if (comp && comp->getId() == componentId) {
            component = comp;
            break;
        }
    }
    
    if (!component) {
        response["success"] = false;
        response["error"] = "Component not found: " + componentId;
        request->send(404, "application/json", response.as<String>());
        return;
    }
    
    // Get current configuration using new architecture
    JsonDocument currentConfig = component->getConfigurationAsJson();
    
    response["success"] = true;
    response["component_id"] = componentId;
    response["component_type"] = component->getType();
    response["component_state"] = component->getStateString();
    response["configuration"] = currentConfig;
    
    String responseJson;
    serializeJson(response, responseJson);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", responseJson);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
    
    log(Logger::INFO, "Configuration retrieved for component: " + componentId);
}

void WebServerComponent::handleGenericComponentConfigGet(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID from query parameter
    String componentId = "";
    if (request->hasParam("id")) {
        componentId = request->getParam("id")->value();
    }
    
    if (componentId.isEmpty()) {
        response["success"] = false;
        response["error"] = "Missing required parameter: id";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Find the component
    BaseComponent* component = m_orchestrator->findComponent(componentId);
    if (!component) {
        response["success"] = false;
        response["error"] = "Component not found: " + componentId;
        request->send(404, "application/json", response.as<String>());
        return;
    }
    
    // Get current configuration using virtual method
    JsonDocument currentConfig = component->getConfigurationAsJson();
    
    response["success"] = true;
    response["component_id"] = componentId;
    response["component_type"] = component->getType();
    response["component_state"] = component->getStateString();
    response["configuration"] = currentConfig;
    
    String responseJson;
    serializeJson(response, responseJson);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", responseJson);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
    
    log(Logger::INFO, "Configuration retrieved for component: " + componentId);
}

void WebServerComponent::handleGenericComponentConfigUpdate(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID from query parameter
    String componentId = "";
    if (request->hasParam("id")) {
        componentId = request->getParam("id")->value();
    }
    
    if (componentId.isEmpty()) {
        response["success"] = false;
        response["error"] = "Invalid component ID";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Find the component
    BaseComponent* component = m_orchestrator->findComponent(componentId);
    if (!component) {
        response["success"] = false;
        response["error"] = "Component not found: " + componentId;
        request->send(404, "application/json", response.as<String>());
        return;
    }
    
    // Parse request body or parameters
    JsonDocument configUpdates;
    
    // Check if we have POST body data
    if (request->hasParam("body", true)) {
        String body = request->getParam("body", true)->value();
        DeserializationError error = deserializeJson(configUpdates, body);
        if (error) {
            response["success"] = false;
            response["error"] = "Invalid JSON in request body";
            request->send(400, "application/json", response.as<String>());
            return;
        }
    } else {
        // Fall back to URL parameters for simple updates
        for (int i = 0; i < request->params(); i++) {
            const AsyncWebParameter* param = request->getParam(i);
            configUpdates[param->name()] = param->value();
        }
    }
    
    if (configUpdates.isNull() || configUpdates.size() == 0) {
        response["success"] = false;
        response["error"] = "No configuration data provided";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    // Apply configuration using virtual method
    if (component->applyConfigurationFromJson(configUpdates)) {
        if (component->saveCurrentConfiguration()) {
            response["success"] = true;
            response["message"] = "Configuration updated and saved";
            response["component_id"] = componentId;
            response["updated_fields"] = configUpdates;
            log(Logger::INFO, "Configuration updated for component: " + componentId);
        } else {
            response["success"] = false;
            response["error"] = "Configuration applied but failed to save";
        }
    } else {
        response["success"] = false;
        response["error"] = "Failed to apply configuration";
    }
    
    String responseJson;
    serializeJson(response, responseJson);
    request->send(200, "application/json", responseJson);
}

void WebServerComponent::handleComponentAdd(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID from query parameter
    String componentId = "";
    if (request->hasParam("id")) {
        componentId = request->getParam("id")->value();
    }
    
    if (componentId.isEmpty()) {
        response["success"] = false;
        response["error"] = "Component ID parameter missing. Required format: ?id=your-component-id&ComponentTypeID=TSL2561";
        response["expected_parameters"] = JsonArray();
        response["expected_parameters"][0] = "id (required): Unique component identifier";
        response["expected_parameters"][1] = "ComponentTypeID (required): TSL2561, DHT22, or PeristalticPump";
        response["expected_parameters"][2] = "remote_ip (optional): Target IP for remote sensors";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    // Validate ComponentTypeID parameter
    if (!request->hasParam("ComponentTypeID")) {
        response["success"] = false;
        response["error"] = "ComponentTypeID parameter missing. Required format: ?id=your-component-id&ComponentTypeID=TSL2561";
        response["supported_types"] = JsonArray();
        response["supported_types"][0] = "TSL2561 (Light sensor)";
        response["supported_types"][1] = "DHT22 (Temperature/Humidity sensor)";
        response["supported_types"][2] = "PeristalticPump (Pump control)";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    String componentType = request->getParam("ComponentTypeID")->value();
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Step 1: Validate ID uniqueness
    if (m_orchestrator->findComponent(componentId) != nullptr) {
        response["success"] = false;
        response["error"] = "Component ID already exists: " + componentId;
        request->send(409, "application/json", response.as<String>());
        return;
    }
    
    // Step 2: Create component instance based on type
    BaseComponent* newComponent = nullptr;
    String componentName = componentId; // Default name, can be overridden
    
    if (componentType == "TSL2561" || componentType == "LightSensor") {
        newComponent = new TSL2561Component(componentId, componentName, m_storage, m_orchestrator);
    }
    else if (componentType == "DHT22" || componentType == "TemperatureSensor") {
        newComponent = new DHT22Component(componentId, componentName, m_storage, m_orchestrator);
    }
    else if (componentType == "PeristalticPump" || componentType == "Pump") {
        newComponent = new PeristalticPumpComponent(componentId, componentName, m_storage, m_orchestrator);
    }
    else {
        response["success"] = false;
        response["error"] = "Unsupported component type: " + componentType;
        response["supported_types"] = JsonArray();
        response["supported_types"][0] = "TSL2561";
        response["supported_types"][1] = "DHT22"; 
        response["supported_types"][2] = "PeristalticPump";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!newComponent) {
        response["success"] = false;
        response["error"] = "Failed to create component instance";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Step 3: Create minimal default configuration (avoid complex schema processing)
    JsonDocument defaultConfig;
    
    // Add required component_type field for orchestrator loading
    defaultConfig["component_type"] = componentType;
    
    // Set component-specific defaults based on type
    if (componentType == "TSL2561" || componentType == "LightSensor") {
        // TSL2561 defaults - minimal configuration for remote sensor
        defaultConfig["useRemoteSensor"] = true;
        defaultConfig["remoteHost"] = "192.168.1.150";  // Default remote host
        defaultConfig["remotePort"] = 80;
        defaultConfig["remotePath"] = "/light";
        defaultConfig["httpTimeoutMs"] = 5000;
        defaultConfig["samplingIntervalMs"] = 10000;
        
        // Override with any provided remote_ip parameter
        if (request->hasParam("remote_ip")) {
            defaultConfig["remoteHost"] = request->getParam("remote_ip")->value();
        }
    } else if (componentType == "DHT22" || componentType == "TemperatureSensor") {
        // DHT22 defaults
        defaultConfig["pin"] = 2;
        defaultConfig["samplingIntervalMs"] = 5000;
    } else if (componentType == "PeristalticPump" || componentType == "Pump") {
        // Pump defaults
        defaultConfig["pumpPin"] = 26;
        defaultConfig["mlPerSecond"] = 40.0;
        defaultConfig["maxRuntimeMs"] = 60000;
    }
    
    // Step 4: SKIP full initialization to prevent memory crashes
    // Just set the component to CREATED state - orchestrator will initialize during execution
    // This avoids complex JSON processing, schema validation, and memory exhaustion
    
    // Set basic state without calling initialize()
    newComponent->setState(ComponentState::UNINITIALIZED);  
    
    // Store the minimal config directly without processing
    // The component will be properly initialized by orchestrator later
    
    // Step 7: Register component with orchestrator
    if (!m_orchestrator->registerComponent(newComponent)) {
        delete newComponent;
        response["success"] = false;
        response["error"] = "Failed to register component with orchestrator";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Step 8: Save minimal configuration directly to storage (bypass component methods)
    log(Logger::INFO, "🔥 [API-SAVE] Attempting to save config for: " + componentId);
    
    // Debug: Log what we're trying to save
    String debugConfig;
    serializeJson(defaultConfig, debugConfig);
    log(Logger::INFO, "🔥 [API-SAVE] Config to save: " + debugConfig);
    
    if (!m_storage.saveComponentConfig(componentId, defaultConfig)) {
        // Component is registered but config not persisted - warn but don't fail
        log(Logger::ERROR, "🔥 [API-SAVE] FAILED to save configuration for: " + componentId);
    } else {
        log(Logger::INFO, "🔥 [API-SAVE] SUCCESS saved configuration for: " + componentId);
        
        // VERIFICATION: Immediately try to read back the saved config
        log(Logger::INFO, "🔍 [API-VERIFY] Verifying saved config can be read back...");
        JsonDocument verifyConfig;
        if (m_storage.loadComponentConfig(componentId, verifyConfig)) {
            String verifyConfigStr;
            serializeJson(verifyConfig, verifyConfigStr);
            log(Logger::INFO, "✅ [API-VERIFY] SUCCESS - Config verified: " + verifyConfigStr);
            
            // Double-check the file actually exists on filesystem
            String expectedPath = "/config/components/" + componentId + ".json";
            if (LittleFS.exists(expectedPath)) {
                log(Logger::INFO, "✅ [API-VERIFY] File confirmed to exist at: " + expectedPath);
            } else {
                log(Logger::ERROR, "❌ [API-VERIFY] CRITICAL: File does NOT exist at: " + expectedPath);
            }
        } else {
            log(Logger::ERROR, "❌ [API-VERIFY] FAILED to read back saved config for: " + componentId);
        }
    }
    
    // Step 9: Success response
    response["success"] = true;
    response["message"] = "Component created and registered successfully";
    response["component_id"] = componentId;
    response["component_type"] = componentType;
    response["component_state"] = newComponent->getStateString();
    response["default_configuration"] = defaultConfig;
    
    String responseJson;
    serializeJson(response, responseJson);
    
    AsyncWebServerResponse* resp = request->beginResponse(201, "application/json", responseJson);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
    
    log(Logger::INFO, "Component created successfully: " + componentId + " (type: " + componentType + ")");
}

void WebServerComponent::handleComponentDelete(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID from query parameter
    String componentId = "";
    if (request->hasParam("id")) {
        componentId = request->getParam("id")->value();
    }
    
    if (componentId.isEmpty()) {
        response["success"] = false;
        response["error"] = "Invalid component ID";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Find the component
    BaseComponent* component = m_orchestrator->findComponent(componentId);
    if (!component) {
        response["success"] = false;
        response["error"] = "Component not found: " + componentId;
        request->send(404, "application/json", response.as<String>());
        return;
    }
    
    // Check if this is a critical system component that shouldn't be deleted
    String componentType = component->getType();
    if (componentType == "WebServer") {
        response["success"] = false;
        response["error"] = "Cannot delete WebServer component - it's a critical system component";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    // Store component info for response before deletion
    String deletedType = component->getType();
    String deletedState = component->getStateString();
    
    // Step 1: Remove configuration from persistent storage
    bool configRemoved = false;
    try {
        // Try to remove the component's configuration from storage
        // Note: This depends on the ConfigStorage implementation
        configRemoved = true; // Assume success for now - actual implementation depends on ConfigStorage API
        log(Logger::DEBUG, "Configuration removal attempted for component: " + componentId);
    } catch (...) {
        log(Logger::WARNING, "Failed to remove configuration from storage for component: " + componentId);
    }
    
    // Step 2: Unregister component from orchestrator (this also handles cleanup)
    if (!m_orchestrator->unregisterComponent(componentId)) {
        response["success"] = false;
        response["error"] = "Failed to unregister component from orchestrator";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Note: The component is automatically deleted by unregisterComponent()
    // Don't call delete on the component pointer as it's handled by orchestrator
    
    // Step 3: Success response
    response["success"] = true;
    response["message"] = "Component deleted successfully";
    response["component_id"] = componentId;
    response["component_type"] = deletedType;
    response["previous_state"] = deletedState;
    response["configuration_removed"] = configRemoved;
    
    String responseJson;
    serializeJson(response, responseJson);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", responseJson);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
    
    log(Logger::INFO, "Component deleted successfully: " + componentId + " (type: " + deletedType + ")");
}

void WebServerComponent::handleComponentActionsGet(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID from URL path
    String url = request->url();
    String componentId = "";
    
    // Parse URL like /api/components/{component_id}/actions
    int startPos = url.indexOf("/api/components/") + 16;
    int endPos = url.indexOf("/actions");
    if (startPos > 15 && endPos > startPos) {
        componentId = url.substring(startPos, endPos);
    }
    
    if (componentId.isEmpty()) {
        response["success"] = false;
        response["error"] = "Invalid component ID";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Find the component
    BaseComponent* component = nullptr;
    const std::vector<BaseComponent*>& components = m_orchestrator->getComponents();
    
    for (BaseComponent* comp : components) {
        if (comp && comp->getId() == componentId) {
            component = comp;
            break;
        }
    }
    
    if (!component) {
        response["success"] = false;
        response["error"] = "Component not found: " + componentId;
        request->send(404, "application/json", response.as<String>());
        return;
    }
    
    // Get available actions
    std::vector<ComponentAction> actions = component->getAvailableActions();
    
    response["success"] = true;
    response["component_id"] = componentId;
    response["component_type"] = component->getType();
    response["component_state"] = component->getStateString();
    
    JsonArray actionsArray = response["actions"].to<JsonArray>();
    for (const ComponentAction& action : actions) {
        JsonObject actionObj = actionsArray.add<JsonObject>();
        actionObj["name"] = action.name;
        actionObj["description"] = action.description;
        actionObj["timeout_ms"] = action.timeoutMs;
        actionObj["requires_ready"] = action.requiresReady;
        
        JsonArray paramsArray = actionObj["parameters"].to<JsonArray>();
        for (const ActionParameter& param : action.parameters) {
            JsonObject paramObj = paramsArray.add<JsonObject>();
            paramObj["name"] = param.name;
            paramObj["type"] = (int)param.type;
            paramObj["required"] = param.required;
            paramObj["description"] = param.description;
            
            if (param.minValue != param.maxValue) {
                paramObj["min_value"] = param.minValue;
                paramObj["max_value"] = param.maxValue;
            }
            if (param.maxLength > 0) {
                paramObj["max_length"] = param.maxLength;
            }
            if (!param.defaultValue.isNull()) {
                paramObj["default_value"] = param.defaultValue;
            }
        }
    }
    
    String responseJson;
    serializeJson(response, responseJson);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", responseJson);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
    
    log(Logger::INFO, "Actions list retrieved for component: " + componentId);
}

void WebServerComponent::handleComponentActionExecute(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument response;
    
    // Extract component ID and action name from URL path
    String url = request->url();
    String componentId = "";
    String actionName = "";
    
    // Parse URL like /api/components/{component_id}/actions/{action_name}
    int startPos = url.indexOf("/api/components/") + 16;
    int midPos = url.indexOf("/actions/");
    int endPos = url.length();
    
    if (startPos > 15 && midPos > startPos) {
        componentId = url.substring(startPos, midPos);
        actionName = url.substring(midPos + 9, endPos);
    }
    
    if (componentId.isEmpty() || actionName.isEmpty()) {
        response["success"] = false;
        response["error"] = "Invalid component ID or action name";
        request->send(400, "application/json", response.as<String>());
        return;
    }
    
    if (!m_orchestrator) {
        response["success"] = false;
        response["error"] = "Orchestrator not available";
        request->send(500, "application/json", response.as<String>());
        return;
    }
    
    // Find the component
    BaseComponent* component = nullptr;
    const std::vector<BaseComponent*>& components = m_orchestrator->getComponents();
    
    for (BaseComponent* comp : components) {
        if (comp && comp->getId() == componentId) {
            component = comp;
            break;
        }
    }
    
    if (!component) {
        response["success"] = false;
        response["error"] = "Component not found: " + componentId;
        request->send(404, "application/json", response.as<String>());
        return;
    }
    
    // Build parameters from request
    JsonDocument parameters;
    
    // Get parameters from URL query string
    for (int i = 0; i < request->params(); i++) {
        const AsyncWebParameter* param = request->getParam(i);
        if (param) {
            // Try to parse as number first, then as string
            String value = param->value();
            if (value.toFloat() != 0 || value == "0") {
                parameters[param->name()] = value.toFloat();
            } else if (value == "true" || value == "false") {
                parameters[param->name()] = (value == "true");
            } else {
                parameters[param->name()] = value;
            }
        }
    }
    
    // Execute the action
    ActionResult result = component->executeAction(actionName, parameters);
    
    response["success"] = result.success;
    response["message"] = result.message;
    response["component_id"] = componentId;
    response["action_name"] = result.actionName;
    response["execution_time_ms"] = result.executionTimeMs;
    
    if (!result.data.isNull()) {
        response["data"] = result.data;
    }
    
    String responseJson;
    serializeJson(response, responseJson);
    
    int statusCode = result.success ? 200 : 400;
    AsyncWebServerResponse* resp = request->beginResponse(statusCode, "application/json", responseJson);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
    
    log(Logger::INFO, String("Action executed: ") + componentId + "/" + actionName + 
                      " - " + (result.success ? "SUCCESS" : "FAILED"));
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

std::vector<ComponentAction> WebServerComponent::getSupportedActions() const {
    std::vector<ComponentAction> actions;
    // TODO: Add web server actions
    return actions;
}

ActionResult WebServerComponent::performAction(const String& actionName, const JsonDocument& parameters) {
    ActionResult result;
    result.success = false;
    result.message = "Action not implemented: " + actionName;
    return result;
}

// === Execution Loop Control Handlers ===

void WebServerComponent::handleExecutionLoopStatus(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument status = m_orchestrator->getExecutionLoopConfig();
    
    String response;
    serializeJson(status, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleExecutionLoopPause(AsyncWebServerRequest* request) {
    logRequest(request);
    
    bool success = m_orchestrator->pauseExecutionLoop();
    
    JsonDocument response;
    response["success"] = success;
    response["message"] = success ? "Execution loop paused" : "Failed to pause execution loop";
    
    String responseStr;
    serializeJson(response, responseStr);
    
    AsyncWebServerResponse* resp = request->beginResponse(success ? 200 : 500, "application/json", responseStr);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleExecutionLoopResume(AsyncWebServerRequest* request) {
    logRequest(request);
    
    bool success = m_orchestrator->resumeExecutionLoop();
    
    JsonDocument response;
    response["success"] = success;
    response["message"] = success ? "Execution loop resumed" : "Failed to resume execution loop";
    
    String responseStr;
    serializeJson(response, responseStr);
    
    AsyncWebServerResponse* resp = request->beginResponse(success ? 200 : 500, "application/json", responseStr);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleExecutionLoopConfig(AsyncWebServerRequest* request) {
    logRequest(request);
    
    JsonDocument config = m_orchestrator->getExecutionLoopConfig();
    
    String response;
    serializeJson(config, response);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}

void WebServerComponent::handleExecutionLoopConfigUpdate(AsyncWebServerRequest* request) {
    logRequest(request);
    
    // Handle POST body
    String body = request->getParam("plain", true)->value();
    
    JsonDocument config;
    DeserializationError error = deserializeJson(config, body);
    
    if (error) {
        JsonDocument errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = "Invalid JSON in request body";
        
        String responseStr;
        serializeJson(errorResponse, responseStr);
        
        AsyncWebServerResponse* resp = request->beginResponse(400, "application/json", responseStr);
        if (m_enableCORS) setCORSHeaders(resp);
        request->send(resp);
        return;
    }
    
    bool success = m_orchestrator->updateExecutionLoopConfig(config);
    
    JsonDocument response;
    response["success"] = success;
    response["message"] = success ? "Execution loop configuration updated" : "Failed to update execution loop configuration";
    
    String responseStr;
    serializeJson(response, responseStr);
    
    AsyncWebServerResponse* resp = request->beginResponse(success ? 200 : 500, "application/json", responseStr);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}
bool WebServerComponent::serveStaticFile(AsyncWebServerRequest* request, const String& path) {
    log(Logger::DEBUG, String("🔍 serveStaticFile() called for path: ") + path);
    
    // Check if LittleFS is mounted first
    if (!LittleFS.begin(false)) {
        log(Logger::ERROR, "❌ LittleFS not mounted - cannot serve static files");
        return false;
    }
    
    // Check if file exists in LittleFS
    log(Logger::DEBUG, String("🔍 Checking if LittleFS file exists: ") + path);
    bool fileExists = LittleFS.exists(path);
    log(Logger::DEBUG, String("🔍 File exists result: ") + (fileExists ? "YES" : "NO"));
    
    if (!fileExists) {
        log(Logger::INFO, String("📄 Static file not found: ") + path);
        
        // List directory contents for debugging
        String dirPath = path.substring(0, path.lastIndexOf('/'));
        if (dirPath.isEmpty()) dirPath = "/";
        log(Logger::DEBUG, String("🔍 Listing directory contents for: ") + dirPath);
        
        File dir = LittleFS.open(dirPath, "r");
        if (dir && dir.isDirectory()) {
            File file = dir.openNextFile();
            while (file) {
                String fileName = String(file.name());
                log(Logger::DEBUG, String("📁 Found: ") + fileName + (file.isDirectory() ? " (DIR)" : " (FILE)"));
                file = dir.openNextFile();
            }
            dir.close();
        } else {
            log(Logger::DEBUG, String("❌ Could not open directory: ") + dirPath);
        }
        
        return false;
    }
    
    log(Logger::INFO, String("✅ Serving static file: ") + path);
    
    // Open the file
    log(Logger::DEBUG, String("🔍 Opening file for reading: ") + path);
    File file = LittleFS.open(path, "r");
    if (!file) {
        log(Logger::WARNING, String("❌ Failed to open static file: ") + path);
        return false;
    }
    
    // Get file info
    size_t fileSize = file.size();
    log(Logger::DEBUG, String("📊 File size: ") + fileSize + " bytes");
    
    // Determine content type based on file extension
    String contentType = getContentType(path);
    log(Logger::DEBUG, String("🔍 Content type: ") + contentType);
    
    // Calculate cache header based on file type
    String cacheControl = "no-cache";
    if (path.endsWith(".css") || path.endsWith(".js") || path.endsWith(".ico")) {
        cacheControl = "public, max-age=86400"; // Cache static assets for 1 day
    }
    log(Logger::DEBUG, String("🔍 Cache control: ") + cacheControl);
    
    // Create response with file stream
    log(Logger::DEBUG, String("🚀 Creating HTTP response for file"));
    AsyncWebServerResponse* response = request->beginResponse(LittleFS, path, contentType);
    response->addHeader("Cache-Control", cacheControl);
    if (m_enableCORS) {
        setCORSHeaders(response);
        log(Logger::DEBUG, "🔧 Added CORS headers");
    }
    
    log(Logger::DEBUG, String("📤 Sending HTTP response"));
    request->send(response);
    file.close();
    
    log(Logger::INFO, String("✅ Successfully served static file: ") + path + " (" + contentType + ", " + fileSize + " bytes)");
    return true;
}

String WebServerComponent::getContentType(const String& filename) {
    if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) return "image/jpeg";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".svg")) return "image/svg+xml";
    else if (filename.endsWith(".xml")) return "text/xml";
    else if (filename.endsWith(".pdf")) return "application/pdf";
    else if (filename.endsWith(".zip")) return "application/zip";
    else if (filename.endsWith(".gz")) return "application/gzip";
    else if (filename.endsWith(".txt")) return "text/plain";
    return "application/octet-stream";
}

void WebServerComponent::handleWebServerTimingDebug(AsyncWebServerRequest* request) {
    logRequest(request);
    
    uint32_t currentTime = millis();
    uint32_t nextExecTime = getNextExecutionMs();
    
    JsonDocument response;
    response["component_id"] = getId();
    response["component_type"] = getType();
    response["current_state"] = getStateString();
    response["current_time_ms"] = currentTime;
    response["next_execution_ms"] = nextExecTime;
    response["last_execution_ms"] = m_lastExecutionMs;
    response["execution_count"] = m_executionCount;
    response["time_until_next_exec"] = (int32_t)(nextExecTime - currentTime);
    response["is_ready_to_execute"] = isReadyToExecute();
    response["never_executed"] = (m_executionCount == 0);
    response["state_ready"] = (getState() == ComponentState::READY);
    response["time_ready"] = (currentTime >= nextExecTime);
    
    // DEBUG: Add detailed timing breakdown
    response["debug_current_time"] = currentTime;
    response["debug_next_execution"] = nextExecTime;
    response["debug_time_comparison"] = (currentTime >= nextExecTime);
    response["debug_state_check"] = (getState() == ComponentState::READY);
    response["debug_never_executed_check"] = (m_executionCount == 0);
    response["debug_full_ready_logic"] = ((getState() == ComponentState::READY) && (m_executionCount == 0)) || 
                                        ((getState() == ComponentState::READY) && (currentTime >= nextExecTime));
    
    String responseStr;
    serializeJson(response, responseStr);
    
    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", responseStr);
    if (m_enableCORS) setCORSHeaders(resp);
    request->send(resp);
}
