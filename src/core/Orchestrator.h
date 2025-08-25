/**
 * @file Orchestrator.h
 * @brief Main orchestrator for ESP32 IoT component management
 * 
 * Manages component lifecycle, execution scheduling, and system coordination.
 * Focuses on fundamental operations with schema-driven component management.
 */

#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include <Arduino.h>
#include <vector>
#include "../components/BaseComponent.h"
#include "../components/DHT22Component.h"
#include "../components/TSL2561Component.h"
#include "../storage/ConfigStorage.h"
#include "../utils/Logger.h"

/**
 * @brief Main system orchestrator
 * 
 * Coordinates component lifecycle, execution scheduling, and system management.
 * Provides a minimal but complete foundation for IoT component orchestration.
 */
class Orchestrator {
private:
    // Core components
    ConfigStorage m_storage;
    std::vector<BaseComponent*> m_components;
    
    // System state
    bool m_initialized = false;
    bool m_running = false;
    uint32_t m_startTime = 0;
    uint32_t m_lastSystemCheck = 0;
    
    // Configuration
    uint32_t m_systemCheckInterval = 30000;  // 30 seconds
    uint32_t m_maxComponents = 10;           // Maximum number of components
    
    // Statistics
    uint32_t m_totalExecutions = 0;
    uint32_t m_totalErrors = 0;
    uint32_t m_loopCount = 0;

public:
    /**
     * @brief Constructor
     */
    Orchestrator();
    
    /**
     * @brief Destructor
     */
    ~Orchestrator();

    /**
     * @brief Initialize the orchestrator system
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Main orchestrator loop (call from Arduino loop())
     */
    void loop();

    /**
     * @brief Shutdown the orchestrator
     */
    void shutdown();

    // === Component Management ===

    /**
     * @brief Register a component with the orchestrator
     * @param component Component instance to register
     * @return true if registration successful
     */
    bool registerComponent(BaseComponent* component);

    /**
     * @brief Unregister a component
     * @param componentId Component ID to unregister
     * @return true if unregistration successful
     */
    bool unregisterComponent(const String& componentId);

    /**
     * @brief Find component by ID
     * @param componentId Component ID to find
     * @return Component pointer or nullptr if not found
     */
    BaseComponent* findComponent(const String& componentId);
    
    /**
     * @brief Update next execution time for a specific component
     * @param componentId ID of component to update
     * @param timeToWakeUp New execution time in milliseconds
     * @return true if component was found and updated
     */
    bool updateNextCheck(const String& componentId, uint32_t timeToWakeUp);

    /**
     * @brief Get all registered components
     * @return Vector of component pointers
     */
    const std::vector<BaseComponent*>& getComponents() const { return m_components; }

    /**
     * @brief Get component count
     * @return Number of registered components
     */
    size_t getComponentCount() const { return m_components.size(); }

    // === System Status ===

    /**
     * @brief Check if orchestrator is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Check if orchestrator is running
     * @return true if running
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief Get system uptime in milliseconds
     * @return Uptime in milliseconds
     */
    uint32_t getUptime() const;

    /**
     * @brief Get system statistics
     * @return Statistics as JSON document
     */
    JsonDocument getSystemStats() const;

    /**
     * @brief Get system health status
     * @return Health status as JSON document
     */
    JsonDocument getHealthStatus() const;

    // === Configuration Management ===

    /**
     * @brief Get configuration storage instance
     * @return Reference to configuration storage
     */
    ConfigStorage& getConfigStorage() { return m_storage; }

    /**
     * @brief Load system configuration
     * @return true if configuration loaded successfully
     */
    bool loadSystemConfig();

    /**
     * @brief Save system configuration
     * @return true if configuration saved successfully
     */
    bool saveSystemConfig();

private:
    /**
     * @brief Initialize default components (DHT22 and TSL2561)
     * @return true if default components initialized
     */
    bool initializeDefaultComponents();

    /**
     * @brief Execute component scheduling and management
     */
    void executeComponentLoop();

    /**
     * @brief Perform system health checks
     */
    void performSystemCheck();

    /**
     * @brief Execute ready components
     * @return Number of components executed
     */
    int executeReadyComponents();

    /**
     * @brief Handle component execution result
     * @param component Component that was executed
     * @param result Execution result
     */
    void handleExecutionResult(BaseComponent* component, const ExecutionResult& result);

    /**
     * @brief Log system message
     * @param level Log level
     * @param message Message to log
     */
    void log(Logger::Level level, const String& message);

    /**
     * @brief Update system statistics
     */
    void updateStatistics();

    /**
     * @brief Check system resources
     * @return true if resources are healthy
     */
    bool checkSystemResources();
};

#endif // ORCHESTRATOR_H