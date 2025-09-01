#pragma once

#include "BaseComponent.h"
#include <WiFiClient.h>
#include <HTTPClient.h>

/**
 * @brief IP-based servo dimmer component for lighting control
 * 
 * Controls a remote servo dimmer via HTTP API to manage lighting intensity.
 * Provides position control, lumen calculation, and orchestrated movement scheduling.
 */
class ServoDimmerComponent : public BaseComponent {
public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for schedule notifications
     */
    ServoDimmerComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator = nullptr);
    
    /**
     * @brief Destructor
     */
    ~ServoDimmerComponent() override;

    // Required BaseComponent implementations
    JsonDocument getDefaultSchema() const override;
    bool initialize(const JsonDocument& config) override;
    ExecutionResult execute() override;
    void cleanup() override;

    // Public API for inter-component communication
    /**
     * @brief Set target lumens for lighting control
     * @param lumens Target lumen output
     * @return true if command accepted
     */
    bool setLumens(float lumens);
    
    /**
     * @brief Set servo position directly (0-100%)
     * @param position Target position percentage
     * @return true if command accepted
     */
    bool setPosition(int position);
    
    /**
     * @brief Get current position
     * @return Current servo position (0-100%)
     */
    int getCurrentPosition() const { return m_currentPosition; }
    
    /**
     * @brief Get target position
     * @return Target servo position (0-100%)
     */
    int getTargetPosition() const { return m_targetPosition; }
    
    /**
     * @brief Get calculated lumens at current position
     * @return Current lumen output
     */
    float getCurrentLumens() const;

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
    String m_deviceIP = "192.168.1.161";        // IP address of servo dimmer device
    uint16_t m_devicePort = 80;                  // HTTP port
    float m_baseLumens = 1000.0f;               // Base lumens at 100% position
    uint32_t m_checkIntervalMs = 30000;         // Position check/update interval (30s)
    uint32_t m_httpTimeoutMs = 5000;            // HTTP request timeout
    bool m_enableMovement = true;               // Allow position changes
    uint16_t m_configVersion = 1;               // Configuration version for migration
    
    // Servo state
    int m_currentPosition = 0;                   // Current servo position (0-100%)
    int m_targetPosition = 0;                    // Next position to move to
    bool m_movementInProgress = false;           // Movement operation active
    uint32_t m_lastStatusCheck = 0;             // Last status query timestamp
    uint32_t m_lastMovementTime = 0;            // Last successful movement
    
    // Device status
    String m_deviceWifiStatus = "Unknown";       // Device WiFi status
    int m_deviceRSSI = 0;                       // Device signal strength
    uint32_t m_deviceUptime = 0;                // Device uptime seconds
    bool m_deviceOnline = false;                 // Device reachability
    uint32_t m_communicationErrors = 0;         // HTTP error counter
    
    // HTTP client
    HTTPClient m_httpClient;
    WiFiClient m_wifiClient;
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    bool queryDeviceStatus();
    bool sendPositionCommand(int position);
    float calculateLumensFromPosition(int position) const;
    int calculatePositionFromLumens(float lumens) const;
    bool isTimeToCheck() const;
    void updateMovementSchedule();
    String buildStatusURL() const;
    String buildSetURL() const;
    void handleHttpError(int httpCode, const String& operation);
    bool validatePosition(int position) const;

    // === Action System (BaseComponent virtual methods) ===
    std::vector<ComponentAction> getSupportedActions() const override;
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;
};