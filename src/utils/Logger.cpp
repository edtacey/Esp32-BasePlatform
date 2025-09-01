/**
 * @file Logger.cpp
 * @brief Logger implementation
 */

#include "Logger.h"
#include <LittleFS.h>

// Static member initialization
Logger::Level Logger::s_currentLevel = Logger::INFO;
bool Logger::s_initialized = false;
bool Logger::s_fileLoggingEnabled = false;
uint32_t Logger::s_maxLogFileSize = 100 * 1024;  // 100KB default
String Logger::s_logFilePath = "/logs/system.log";

void Logger::init(Level level) {
    s_currentLevel = level;
    s_initialized = true;
    
    // Log initialization
    Serial.println();
    Serial.println("=== ESP32 IoT Orchestrator Logger Initialized ===");
    Serial.printf("Log level: %s\n", levelToString(s_currentLevel));
    Serial.println("================================================");
    Serial.println();
}

void Logger::setLevel(Level level) {
    s_currentLevel = level;
}

void Logger::enableFileLogging(bool enabled, uint32_t maxLogFileSizeKB) {
    s_fileLoggingEnabled = enabled;
    s_maxLogFileSize = maxLogFileSizeKB * 1024;
    
    if (enabled) {
        // Create logs directory if it doesn't exist
        if (!LittleFS.exists("/logs")) {
            LittleFS.mkdir("/logs");
        }
        Serial.printf("File logging enabled: %s (max %uKB)\n", s_logFilePath.c_str(), maxLogFileSizeKB);
    } else {
        Serial.println("File logging disabled");
    }
}

void Logger::debug(const char* component, const String& message) {
    log(DEBUG, component, message);
}

void Logger::info(const char* component, const String& message) {
    log(INFO, component, message);
}

void Logger::warning(const char* component, const String& message) {
    log(WARNING, component, message);
}

void Logger::error(const char* component, const String& message) {
    log(ERROR, component, message);
}

void Logger::critical(const char* component, const String& message) {
    log(CRITICAL, component, message);
}

void Logger::log(Level level, const char* component, const String& message) {
    // Compile-time optimization: skip debug/info logs in production
    #ifdef NDEBUG
        if (level < WARNING) return;  // Only log WARNING and above in production
    #endif
    
    // Check if we should log this level
    if (level < s_currentLevel) {
        return;
    }
    
    // Auto-initialize if needed
    if (!s_initialized) {
        init();
    }
    
    // Get timestamp
    uint32_t timestamp = millis();
    uint32_t seconds = timestamp / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    // Format: [HH:MM:SS.mmm][LEVEL][Component] Message
    Serial.printf("[%02u:%02u:%02u.%03u][%s][%s] %s\n",
                  hours % 24,
                  minutes % 60, 
                  seconds % 60,
                  timestamp % 1000,
                  levelToString(level),
                  component,
                  message.c_str());
    
    // Also write to file if file logging is enabled
    if (s_fileLoggingEnabled) {
        writeToFile(level, component, message);
    }
}

const char* Logger::levelToString(Level level) {
    switch (level) {
        case DEBUG:    return "DEBUG";
        case INFO:     return "INFO ";
        case WARNING:  return "WARN ";
        case ERROR:    return "ERROR";
        case CRITICAL: return "CRIT ";
        default:       return "?????";
    }
}

void Logger::writeToFile(Level level, const char* component, const String& message) {
    // Rotate log file if needed
    rotateLogFileIfNeeded();
    
    // Get timestamp
    uint32_t timestamp = millis();
    uint32_t seconds = timestamp / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    // Open log file in append mode
    File logFile = LittleFS.open(s_logFilePath, "a");
    if (!logFile) {
        // If we can't open the file, try to create it
        logFile = LittleFS.open(s_logFilePath, "w");
        if (!logFile) {
            return; // Unable to create log file
        }
    }
    
    // Write log entry to file (same format as serial)
    logFile.printf("[%02u:%02u:%02u.%03u][%s][%s] %s\n",
                   hours % 24,
                   minutes % 60, 
                   seconds % 60,
                   timestamp % 1000,
                   levelToString(level),
                   component,
                   message.c_str());
    
    logFile.close();
}

void Logger::rotateLogFileIfNeeded() {
    // Check if log file exists and get its size
    if (!LittleFS.exists(s_logFilePath)) {
        return; // File doesn't exist, no need to rotate
    }
    
    File logFile = LittleFS.open(s_logFilePath, "r");
    if (!logFile) {
        return; // Can't open file
    }
    
    size_t fileSize = logFile.size();
    logFile.close();
    
    // If file exceeds maximum size, rotate it
    if (fileSize >= s_maxLogFileSize) {
        String backupPath = s_logFilePath + ".old";
        
        // Remove old backup if it exists
        if (LittleFS.exists(backupPath)) {
            LittleFS.remove(backupPath);
        }
        
        // Rename current log to backup
        LittleFS.rename(s_logFilePath, backupPath);
        
        // Log the rotation to serial (can't log to file since we just rotated)
        Serial.printf("Log file rotated: %s -> %s\n", s_logFilePath.c_str(), backupPath.c_str());
    }
}