/**
 * @file Logger.h
 * @brief Simple logging utility for ESP32 IoT Orchestrator
 * 
 * Provides centralized logging with levels and component identification.
 * Keeps logging simple for the baseline implementation.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

/**
 * @brief Simple logger class for debugging and monitoring
 * 
 * Static class providing logging functionality with different levels.
 * All logs go to Serial for now - file logging can be added later.
 */
class Logger {
public:
    /**
     * @brief Log levels
     */
    enum Level {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

private:
    static Level s_currentLevel;
    static bool s_initialized;
    static bool s_fileLoggingEnabled;
    static uint32_t s_maxLogFileSize;
    static String s_logFilePath;

public:
    /**
     * @brief Initialize the logger
     * @param level Minimum log level to output
     */
    static void init(Level level = INFO);

    /**
     * @brief Set the log level
     * @param level New minimum log level
     */
    static void setLevel(Level level);
    
    /**
     * @brief Enable or disable file logging to LittleFS
     * @param enabled True to enable file logging
     * @param maxLogFileSizeKB Maximum log file size in KB (default 100KB)
     */
    static void enableFileLogging(bool enabled, uint32_t maxLogFileSizeKB = 100);

    /**
     * @brief Log a debug message
     * @param component Component name
     * @param message Log message
     */
    static void debug(const char* component, const String& message);

    /**
     * @brief Log an info message
     * @param component Component name
     * @param message Log message
     */
    static void info(const char* component, const String& message);

    /**
     * @brief Log a warning message
     * @param component Component name
     * @param message Log message
     */
    static void warning(const char* component, const String& message);

    /**
     * @brief Log an error message
     * @param component Component name
     * @param message Log message
     */
    static void error(const char* component, const String& message);

    /**
     * @brief Log a critical message
     * @param component Component name
     * @param message Log message
     */
    static void critical(const char* component, const String& message);

private:
    /**
     * @brief Internal logging function
     * @param level Log level
     * @param component Component name
     * @param message Log message
     */
    static void log(Level level, const char* component, const String& message);

    /**
     * @brief Get level string
     * @param level Log level
     * @return Level as string
     */
    static const char* levelToString(Level level);
    
    /**
     * @brief Write log entry to file if file logging is enabled
     * @param level Log level
     * @param component Component name
     * @param message Log message
     */
    static void writeToFile(Level level, const char* component, const String& message);
    
    /**
     * @brief Rotate log file if it exceeds maximum size
     */
    static void rotateLogFileIfNeeded();
};

#endif // LOGGER_H