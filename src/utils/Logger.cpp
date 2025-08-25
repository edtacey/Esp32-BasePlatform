/**
 * @file Logger.cpp
 * @brief Logger implementation
 */

#include "Logger.h"

// Static member initialization
Logger::Level Logger::s_currentLevel = Logger::INFO;
bool Logger::s_initialized = false;

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