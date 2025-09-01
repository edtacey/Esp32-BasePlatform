#pragma once

#include "BaseComponent.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// Forward declaration
class Orchestrator;

/**
 * @brief Lightweight web server component for REST API and web pages
 * 
 * Provides HTTP endpoints for component data, system logs, and simple web interface.
 * Serves both JSON API endpoints and basic HTML pages for system monitoring.
 */
class WebServerComponent : public BaseComponent {
public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for component access
     */
    WebServerComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator);
    
    /**
     * @brief Destructor
     */
    ~WebServerComponent() override;

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

private:
    // Configuration parameters
    uint16_t m_serverPort = 80;              // HTTP server port
    bool m_enableCORS = true;                // Enable CORS headers
    String m_serverName = "ESP32-Orchestrator"; // Server name header
    
    // Web server
    AsyncWebServer* m_webServer = nullptr;
    
    // Server state
    bool m_serverRunning = false;
    uint32_t m_requestCount = 0;
    uint32_t m_lastRequestTime = 0;
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    void setupRoutes();
    void setupAPIEndpoints();
    void setupWebPages();
    
    // API endpoint handlers
    void handleSystemStatus(AsyncWebServerRequest* request);
    void handleComponentData(AsyncWebServerRequest* request);
    void handleComponentList(AsyncWebServerRequest* request);
    void handleLogs(AsyncWebServerRequest* request);
    void handleLogFile(AsyncWebServerRequest* request);
    void handleMemoryInfo(AsyncWebServerRequest* request);
    void handleComponentsMqtt(AsyncWebServerRequest* request);
    void handleComponentsDebug(AsyncWebServerRequest* request);
    void handleSweepTestStart(AsyncWebServerRequest* request);
    void handleSweepTestStop(AsyncWebServerRequest* request);
    void handleSweepTestStatus(AsyncWebServerRequest* request);
    void handleComponentConfig(AsyncWebServerRequest* request);
    void handleComponentConfigGet(AsyncWebServerRequest* request);
    void handleGenericComponentConfigUpdate(AsyncWebServerRequest* request);
    void handleGenericComponentConfigGet(AsyncWebServerRequest* request);
    void handleComponentAdd(AsyncWebServerRequest* request);
    void handleComponentDelete(AsyncWebServerRequest* request);
    void handleComponentActionsGet(AsyncWebServerRequest* request);
    void handleComponentActionExecute(AsyncWebServerRequest* request);
    
    // Execution loop control handlers
    void handleExecutionLoopStatus(AsyncWebServerRequest* request);
    void handleExecutionLoopPause(AsyncWebServerRequest* request);
    void handleExecutionLoopResume(AsyncWebServerRequest* request);
    void handleExecutionLoopConfig(AsyncWebServerRequest* request);
    void handleExecutionLoopConfigUpdate(AsyncWebServerRequest* request);
    
    // Web page handlers  
    void handleHomePage(AsyncWebServerRequest* request);
    void handleComponentPage(AsyncWebServerRequest* request);
    void handleLogsPage(AsyncWebServerRequest* request);
    void handleDashboardPage(AsyncWebServerRequest* request);
    
    // Data filtering options
    enum DataFilterType {
        FILTER_NONE,         // All data (default)
        FILTER_CORE,         // Core sensor values only
        FILTER_DIAGNOSTICS   // Full diagnostic data
    };
    
    // Utility methods
    String getContentType(const String& filename);
    void setCORSHeaders(AsyncWebServerResponse* response);
    void logRequest(AsyncWebServerRequest* request);
    JsonDocument getAllComponentData();
    JsonDocument filterComponentData(const JsonDocument& data, DataFilterType filterType);
    bool serveStaticFile(AsyncWebServerRequest* request, const String& path);

    // === Action System (BaseComponent virtual methods) ===
    std::vector<ComponentAction> getSupportedActions() const override;
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;
};