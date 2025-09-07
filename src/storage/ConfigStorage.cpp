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
        Logger::error("ConfigStorage", "❌ [STORAGE] Storage not initialized");
        return false;
    }
    
    String filePath = getComponentConfigPath(componentId);
    Logger::info("ConfigStorage", "💾 [STORAGE-SAVE] Saving: " + componentId + " -> " + filePath);
    
    // Log the content being saved
    String configStr;
    serializeJson(config, configStr);
    Logger::info("ConfigStorage", "💾 [STORAGE-SAVE] Content: " + configStr);
    
    bool result = saveJsonToFile(filePath, config);
    
    if (result) {
        Logger::info("ConfigStorage", "✅ [STORAGE-SAVE] Successfully wrote: " + filePath);
    } else {
        Logger::error("ConfigStorage", "❌ [STORAGE-SAVE] Failed to write: " + filePath);
    }
    
    return result;
}

bool ConfigStorage::loadComponentConfig(const String& componentId, JsonDocument& config) {
    if (!m_initialized) {
        Logger::error("ConfigStorage", "❌ [STORAGE] Storage not initialized");
        return false;
    }
    
    String filePath = getComponentConfigPath(componentId);
    Logger::info("ConfigStorage", "📂 [STORAGE-LOAD] Loading: " + componentId + " <- " + filePath);
    
    // Check if file exists
    if (!fileExists(filePath)) {
        Logger::warning("ConfigStorage", "⚠️ [STORAGE-LOAD] File does not exist: " + filePath);
        return false;
    }
    
    bool result = loadJsonFromFile(filePath, config);
    
    if (result) {
        String loadedStr;
        serializeJson(config, loadedStr);
        Logger::info("ConfigStorage", "✅ [STORAGE-LOAD] Successfully loaded: " + loadedStr);
    } else {
        Logger::error("ConfigStorage", "❌ [STORAGE-LOAD] Failed to parse JSON from: " + filePath);
    }
    
    return result;
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
    
    // Open the components directory directly instead of root
    File componentsDir = LittleFS.open(COMPONENT_CONFIG_PATH);
    if (!componentsDir || !componentsDir.isDirectory()) {
        // Components directory doesn't exist or isn't a directory
        return configs;
    }
    
    File file = componentsDir.openNextFile();
    
    while (file) {
        String fileName = file.name();
        if (!file.isDirectory() && fileName.endsWith(".json")) {
            // Extract component ID from filename (remove .json extension)
            String componentId = fileName.substring(0, fileName.length() - 5);
            configs.push_back(componentId);
        }
        file = componentsDir.openNextFile();
    }
    
    componentsDir.close();
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
    Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Starting saveJsonToFile for: " + filePath);
    
    // Check if LittleFS is mounted
    if (!LittleFS.begin()) {
        Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] CRITICAL: LittleFS not mounted!");
        return false;
    }
    
    // Test JSON document validity
    if (doc.isNull()) {
        Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] ERROR: JsonDocument is null!");
        return false;
    }
    
    size_t docSize = measureJson(doc);
    Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] JSON size: " + String(docSize) + " bytes");
    
    // Debug JSON content (limited to prevent memory issues)
    if (docSize < 500) {
        String debugJson;
        serializeJson(doc, debugJson);
        Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] JSON content: " + debugJson);
    }
    
    // Ensure directory exists by creating parent directories
    String dirPath = filePath.substring(0, filePath.lastIndexOf('/'));
    Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Target directory: " + dirPath);
    
    if (dirPath.length() > 0) {
        // Check if directory exists
        if (LittleFS.exists(dirPath)) {
            Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Directory already exists: " + dirPath);
        } else {
            Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Creating directory: " + dirPath);
            if (!LittleFS.mkdir(dirPath)) {
                Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] Failed to create directory: " + dirPath);
                
                // Try creating parent directories recursively
                Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Attempting recursive directory creation");
                int lastSlash = dirPath.lastIndexOf('/');
                while (lastSlash > 0) {
                    String parentDir = dirPath.substring(0, lastSlash);
                    if (!LittleFS.exists(parentDir)) {
                        Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Creating parent: " + parentDir);
                        LittleFS.mkdir(parentDir);
                    }
                    lastSlash = dirPath.lastIndexOf('/', lastSlash - 1);
                }
                // Try creating the directory again
                Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Retrying directory creation: " + dirPath);
                LittleFS.mkdir(dirPath);
            } else {
                Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Directory created successfully: " + dirPath);
            }
        }
    }
    
    // Attempt to open file for writing
    Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Opening file for write: " + filePath);
    File file = LittleFS.open(filePath, "w");
    if (!file) {
        Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] FAILED to open file for writing: " + filePath);
        
        // Additional debugging - check available space
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] LittleFS space - Total: " + String(totalBytes) + ", Used: " + String(usedBytes) + ", Free: " + String(totalBytes - usedBytes));
        
        return false;
    }
    
    Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] File opened successfully, writing JSON...");
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] Bytes written: " + String(bytesWritten));
    
    if (bytesWritten == 0) {
        Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] ERROR: Zero bytes written to file!");
        return false;
    }
    
    // Verify file was actually created and has content
    if (LittleFS.exists(filePath)) {
        File verifyFile = LittleFS.open(filePath, "r");
        if (verifyFile) {
            size_t fileSize = verifyFile.size();
            verifyFile.close();
            Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] SUCCESS: File verified, size: " + String(fileSize) + " bytes");
        } else {
            Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] ERROR: File exists but cannot be opened for verification");
        }
    } else {
        Logger::error("ConfigStorage", "🔍 [SAVE-DEBUG] ERROR: File does not exist after write operation!");
        return false;
    }
    
    Logger::info("ConfigStorage", "🔍 [SAVE-DEBUG] saveJsonToFile completed successfully for: " + filePath);
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

// === Execution Loop Configuration ===

bool ConfigStorage::saveExecutionLoopConfig(const JsonDocument& config) {
    return saveJsonToFile("/system/execution_loop.json", config);
}

bool ConfigStorage::loadExecutionLoopConfig(JsonDocument& config) {
    return loadJsonFromFile("/system/execution_loop.json", config);
}