/**
 * @file ConfigStorage.h
 * @brief Persistent configuration storage for component schemas and settings
 * 
 * Manages component configurations and schemas using SPIFFS filesystem.
 * Provides schema persistence and component configuration management.
 */

#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "../utils/Logger.h"

/**
 * @brief Persistent storage manager for component configurations and schemas
 * 
 * Handles saving and loading of:
 * - Component configurations
 * - Component schemas
 * - System settings
 * 
 * Uses SPIFFS filesystem for persistence across reboots.
 */
class ConfigStorage {
private:
    bool m_initialized = false;
    static const size_t MAX_FILE_SIZE = 8192;  // 8KB max file size
    static const char* COMPONENT_CONFIG_PATH;
    static const char* COMPONENT_SCHEMA_PATH;
    static const char* SYSTEM_CONFIG_PATH;

public:
    /**
     * @brief Initialize the storage system
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Check if storage is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return m_initialized; }

    // === Component Configuration Management ===

    /**
     * @brief Save component configuration
     * @param componentId Component identifier
     * @param config Configuration document
     * @return true if saved successfully
     */
    bool saveComponentConfig(const String& componentId, const JsonDocument& config);

    /**
     * @brief Load component configuration
     * @param componentId Component identifier
     * @param config Output configuration document
     * @return true if loaded successfully
     */
    bool loadComponentConfig(const String& componentId, JsonDocument& config);

    /**
     * @brief Check if component configuration exists
     * @param componentId Component identifier
     * @return true if configuration exists
     */
    bool hasComponentConfig(const String& componentId);

    /**
     * @brief Delete component configuration
     * @param componentId Component identifier
     * @return true if deleted successfully
     */
    bool deleteComponentConfig(const String& componentId);

    // === Component Schema Management ===

    /**
     * @brief Save component schema
     * @param componentType Component type string
     * @param schema Schema document
     * @return true if saved successfully
     */
    bool saveComponentSchema(const String& componentType, const JsonDocument& schema);

    /**
     * @brief Load component schema
     * @param componentType Component type string
     * @param schema Output schema document
     * @return true if loaded successfully
     */
    bool loadComponentSchema(const String& componentType, JsonDocument& schema);

    /**
     * @brief Check if component schema exists
     * @param componentType Component type string
     * @return true if schema exists
     */
    bool hasComponentSchema(const String& componentType);

    // === System Configuration Management ===

    /**
     * @brief Save system configuration
     * @param config System configuration document
     * @return true if saved successfully
     */
    bool saveSystemConfig(const JsonDocument& config);

    /**
     * @brief Load system configuration
     * @param config Output system configuration document
     * @return true if loaded successfully
     */
    bool loadSystemConfig(JsonDocument& config);

    // === File System Management ===

    /**
     * @brief List all stored component configurations
     * @return Array of component IDs
     */
    std::vector<String> listComponentConfigs();

    /**
     * @brief List all stored component schemas
     * @return Array of component types
     */
    std::vector<String> listComponentSchemas();

    /**
     * @brief Get storage statistics
     * @return Statistics as JSON document
     */
    JsonDocument getStorageStats();

    /**
     * @brief Format the storage (delete all files)
     * @return true if formatting successful
     */
    bool formatStorage();

    // === Execution Loop Configuration ===

    /**
     * @brief Save execution loop configuration
     * @param config Execution loop configuration document
     * @return true if saved successfully
     */
    bool saveExecutionLoopConfig(const JsonDocument& config);

    /**
     * @brief Load execution loop configuration
     * @param config Output configuration document
     * @return true if loaded successfully
     */
    bool loadExecutionLoopConfig(JsonDocument& config);

private:
    /**
     * @brief Save JSON document to file
     * @param filePath File path to save to
     * @param doc JSON document to save
     * @return true if saved successfully
     */
    bool saveJsonToFile(const String& filePath, const JsonDocument& doc);

    /**
     * @brief Load JSON document from file
     * @param filePath File path to load from
     * @param doc Output JSON document
     * @return true if loaded successfully
     */
    bool loadJsonFromFile(const String& filePath, JsonDocument& doc);

    /**
     * @brief Check if file exists
     * @param filePath File path to check
     * @return true if file exists
     */
    bool fileExists(const String& filePath);

    /**
     * @brief Delete file
     * @param filePath File path to delete
     * @return true if deleted successfully
     */
    bool deleteFile(const String& filePath);

    /**
     * @brief Get component config file path
     * @param componentId Component identifier
     * @return File path string
     */
    String getComponentConfigPath(const String& componentId);

    /**
     * @brief Get component schema file path
     * @param componentType Component type string
     * @return File path string
     */
    String getComponentSchemaPath(const String& componentType);

    /**
     * @brief Log storage operations
     * @param message Log message
     */
    void log(const String& message);
};

#endif // CONFIG_STORAGE_H