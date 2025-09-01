#pragma once

#include "BaseComponent.h"
#include <PubSubClient.h>
#include <WiFi.h>

// Forward declaration
class Orchestrator;

/**
 * @brief MQTT broadcast component for publishing component data
 * 
 * Publishes all registered component output data to MQTT topics
 * using configurable server settings and topic structure.
 */
class MqttBroadcastComponent : public BaseComponent {
public:
    /**
     * @brief Constructor
     * @param id Unique component identifier
     * @param name Human-readable component name
     * @param storage Reference to ConfigStorage instance
     * @param orchestrator Reference to orchestrator for component access
     */
    MqttBroadcastComponent(const String& id, const String& name, ConfigStorage& storage, Orchestrator* orchestrator);
    
    /**
     * @brief Destructor
     */
    ~MqttBroadcastComponent() override;

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
    String m_mqttServer = "192.168.1.80";    // MQTT server IP/hostname
    uint16_t m_mqttPort = 1883;              // MQTT server port
    uint32_t m_publishCycleSec = 15;         // Publish interval in seconds
    String m_mqttRoot = "/EspOrch";          // Base MQTT topic root
    String m_mqttUsername = "";              // MQTT username (optional)
    String m_mqttPassword = "";              // MQTT password (optional)
    
    // MQTT client
    WiFiClient m_wifiClient;
    PubSubClient m_mqttClient;
    
    // Connection state
    bool m_mqttConnected = false;
    uint32_t m_lastReconnectAttempt = 0;
    uint32_t m_reconnectInterval = 5000;     // 5 seconds between reconnect attempts
    
    // Publishing state
    uint32_t m_lastPublish = 0;
    
    // Private methods
    bool applyConfiguration(const JsonDocument& config);
    bool connectToMqtt();
    void publishComponentData();
    void publishSingleComponent(BaseComponent* component);
    String getComponentTopic(const String& componentId) const;
    bool ensureMqttConnection();

    // === Action System (BaseComponent virtual methods) ===
    std::vector<ComponentAction> getSupportedActions() const override;
    ActionResult performAction(const String& actionName, const JsonDocument& parameters) override;
};