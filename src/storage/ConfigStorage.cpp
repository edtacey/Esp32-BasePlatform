/**
 * @file ConfigStorage.cpp
 * @brief ConfigStorage implementation
 */

#include "ConfigStorage.h"
#include <vector>

// Static path constants
const char* ConfigStorage::COMPONENT_CONFIG_PATH = "/config/components";
const char* ConfigStorage::COMPONENT_SCHEMA_PATH = "/schemas/components";
const char* ConfigStorage::SYSTEM_CONFIG_PATH = "/config/system.json";

bool ConfigStorage::init() {
    log("Initializing ConfigStorage...");
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {  // true = format on failure
        Logger::error("ConfigStorage", "Failed to initialize LittleFS");
        return false;
    }
    
    // Create directory structure
    // LittleFS supports real directories, so create them
    if (!LittleFS.exists("/config")) {
        LittleFS.mkdir("/config");
    }
    if (!LittleFS.exists("/config/components")) {
        LittleFS.mkdir("/config/components");
    }
    if (!LittleFS.exists("/config/schemas")) {
        LittleFS.mkdir("/config/schemas");
    }
    if (!LittleFS.exists("/data")) {
        LittleFS.mkdir("/data");
    }
    
    m_initialized = true;
    log("ConfigStorage initialized successfully");
    
    // Log storage info
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Logger::info("ConfigStorage", 
                 String("Storage: ") + usedBytes + "/" + totalBytes + " bytes used (" + 
                 (usedBytes * 100 / totalBytes) + "%)");
    
    return true;
}

bool ConfigStorage::saveComponentConfig(const String& componentId, const JsonDocument& config) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    String filePath = getComponentConfigPath(componentId);
    log("Saving component config: " + componentId + " -> " + filePath);
    
    return saveJsonToFile(filePath, config);
}

bool ConfigStorage::loadComponentConfig(const String& componentId, JsonDocument& config) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    String filePath = getComponentConfigPath(componentId);
    log("Loading component config: " + componentId + " <- " + filePath);
    
    return loadJsonFromFile(filePath, config);
}

bool ConfigStorage::hasComponentConfig(const String& componentId) {
    if (!m_initialized) return false;
    
    String filePath = getComponentConfigPath(componentId);
    return fileExists(filePath);
}

bool ConfigStorage::deleteComponentConfig(const String& componentId) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    String filePath = getComponentConfigPath(componentId);
    log("Deleting component config: " + componentId + " (" + filePath + ")");
    
    return deleteFile(filePath);
}

bool ConfigStorage::saveComponentSchema(const String& componentType, const JsonDocument& schema) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    String filePath = getComponentSchemaPath(componentType);
    log("Saving component schema: " + componentType + " -> " + filePath);
    
    return saveJsonToFile(filePath, schema);
}

bool ConfigStorage::loadComponentSchema(const String& componentType, JsonDocument& schema) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    String filePath = getComponentSchemaPath(componentType);
    log("Loading component schema: " + componentType + " <- " + filePath);
    
    return loadJsonFromFile(filePath, schema);
}

bool ConfigStorage::hasComponentSchema(const String& componentType) {
    if (!m_initialized) return false;
    
    String filePath = getComponentSchemaPath(componentType);
    return fileExists(filePath);
}

bool ConfigStorage::saveSystemConfig(const JsonDocument& config) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    log("Saving system config -> " + String(SYSTEM_CONFIG_PATH));
    return saveJsonToFile(SYSTEM_CONFIG_PATH, config);
}

bool ConfigStorage::loadSystemConfig(JsonDocument& config) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    log("Loading system config <- " + String(SYSTEM_CONFIG_PATH));
    return loadJsonFromFile(SYSTEM_CONFIG_PATH, config);
}

std::vector<String> ConfigStorage::listComponentConfigs() {
    std::vector<String> configs;
    
    if (!m_initialized) return configs;
    
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    
    String configPrefix = String(COMPONENT_CONFIG_PATH) + "/";
    
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith(configPrefix) && fileName.endsWith(".json")) {
            // Extract component ID from filename
            String componentId = fileName.substring(configPrefix.length());
            componentId = componentId.substring(0, componentId.length() - 5);  // Remove .json
            configs.push_back(componentId);
        }
        file = root.openNextFile();
    }
    
    return configs;
}

std::vector<String> ConfigStorage::listComponentSchemas() {
    std::vector<String> schemas;
    
    if (!m_initialized) return schemas;
    
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    
    String schemaPrefix = String(COMPONENT_SCHEMA_PATH) + "/";
    
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith(schemaPrefix) && fileName.endsWith(".json")) {
            // Extract component type from filename
            String componentType = fileName.substring(schemaPrefix.length());
            componentType = componentType.substring(0, componentType.length() - 5);  // Remove .json
            schemas.push_back(componentType);
        }
        file = root.openNextFile();
    }
    
    return schemas;
}

JsonDocument ConfigStorage::getStorageStats() {
    JsonDocument stats;
    
    if (!m_initialized) {
        stats["initialized"] = false;
        return stats;
    }
    
    stats["initialized"] = true;
    stats["totalBytes"] = LittleFS.totalBytes();
    stats["usedBytes"] = LittleFS.usedBytes();
    stats["freeBytes"] = LittleFS.totalBytes() - LittleFS.usedBytes();
    stats["usagePercent"] = (LittleFS.usedBytes() * 100) / LittleFS.totalBytes();
    
    // Count files
    int fileCount = 0;
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        fileCount++;
        file = root.openNextFile();
    }
    stats["fileCount"] = fileCount;
    
    return stats;
}

bool ConfigStorage::formatStorage() {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "Storage not initialized");
        return false;
    }
    
    Logger::warning("ConfigStorage", "Formatting storage - all data will be lost!");
    
    bool result = LittleFS.format();
    if (result) {
        log("Storage formatted successfully");
    } else {
        Logger::error("ConfigStorage", "Storage formatting failed");
    }
    
    return result;
}

// Private methods

bool ConfigStorage::saveJsonToFile(const String& filePath, const JsonDocument& doc) {
    // Ensure directory exists by creating parent directories
    String dirPath = filePath.substring(0, filePath.lastIndexOf('/'));
    if (dirPath.length() > 0) {
        // Create directory if it doesn't exist
        if (!LittleFS.exists(dirPath)) {
            if (!LittleFS.mkdir(dirPath)) {
                // Try creating parent directories recursively
                int lastSlash = dirPath.lastIndexOf('/');
                while (lastSlash > 0) {
                    String parentDir = dirPath.substring(0, lastSlash);
                    if (!LittleFS.exists(parentDir)) {
                        LittleFS.mkdir(parentDir);
                    }
                    lastSlash = dirPath.lastIndexOf('/', lastSlash - 1);
                }
                // Try creating the directory again
                LittleFS.mkdir(dirPath);
            }
        }
    }
    
    File file = LittleFS.open(filePath, "w");
    if (!file) {
        Logger::error("ConfigStorage", "Failed to create file: " + filePath);
        return false;
    }
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    if (bytesWritten == 0) {
        Logger::error("ConfigStorage", "Failed to write JSON to file: " + filePath);
        return false;
    }
    
    Logger::debug("ConfigStorage", "Saved " + String(bytesWritten) + " bytes to " + filePath);
    return true;
}

bool ConfigStorage::loadJsonFromFile(const String& filePath, JsonDocument& doc) {
    if (!fileExists(filePath)) {
        Logger::debug("ConfigStorage", "File does not exist: " + filePath);
        return false;
    }
    
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        Logger::error("ConfigStorage", "Failed to open file: " + filePath);
        return false;
    }
    
    size_t fileSize = file.size();
    if (fileSize > MAX_FILE_SIZE) {
        Logger::error("ConfigStorage", "File too large: " + filePath + " (" + fileSize + " bytes)");
        file.close();
        return false;
    }
    
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Logger::error("ConfigStorage", "JSON parsing error in " + filePath + ": " + String(error.c_str()));
        return false;
    }
    
    Logger::debug("ConfigStorage", "Loaded " + String(fileSize) + " bytes from " + filePath);
    return true;
}

bool ConfigStorage::fileExists(const String& filePath) {
    return LittleFS.exists(filePath);
}

bool ConfigStorage::deleteFile(const String& filePath) {
    if (!fileExists(filePath)) {
        Logger::debug("ConfigStorage", "File does not exist (cannot delete): " + filePath);
        return false;
    }
    
    bool result = LittleFS.remove(filePath);
    if (!result) {
        Logger::error("ConfigStorage", "Failed to delete file: " + filePath);
    }
    
    return result;
}

String ConfigStorage::getComponentConfigPath(const String& componentId) {
    return String(COMPONENT_CONFIG_PATH) + "/" + componentId + ".json";
}

String ConfigStorage::getComponentSchemaPath(const String& componentType) {
    return String(COMPONENT_SCHEMA_PATH) + "/" + componentType + ".json";
}

void ConfigStorage::log(const String& message) {
    Logger::debug("ConfigStorage", message);
}