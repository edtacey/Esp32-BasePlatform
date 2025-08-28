/**
 * @file TimeUtils.h
 * @brief Time utilities for ESP32 IoT Orchestrator
 * 
 * Provides NTP-synchronized time functions for proper timestamps
 * and duration calculations throughout the system.
 */

#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>
#include <time.h>

class TimeUtils {
public:
    /**
     * @brief Get current epoch timestamp (seconds since 1970)
     * @return Current epoch time, or 0 if NTP not available
     */
    static time_t getEpochTime();
    
    /**
     * @brief Get current timestamp as ISO 8601 string
     * @return Formatted timestamp (YYYY-MM-DD HH:MM:SS)
     */
    static String getCurrentTimestamp();
    
    /**
     * @brief Get human-readable time since a past epoch timestamp
     * @param pastEpoch Past timestamp in epoch seconds
     * @return Human readable duration (e.g., "2m 30s ago", "1h ago")
     */
    static String getTimeSince(time_t pastEpoch);
    
    /**
     * @brief Get milliseconds since system boot
     * @return Milliseconds since boot (same as millis())
     */
    static unsigned long getBootMillis();
    
    /**
     * @brief Get seconds since system boot
     * @return Seconds since boot
     */
    static unsigned long getBootSeconds();
    
    /**
     * @brief Check if NTP time is available
     * @return true if NTP synchronized, false if using boot time
     */
    static bool isNTPAvailable();
    
    /**
     * @brief Convert millis() timestamp to epoch time
     * @param millisTime Timestamp from millis()
     * @return Epoch timestamp, or 0 if conversion not possible
     */
    static time_t millisToEpoch(unsigned long millisTime);
    
    /**
     * @brief Set the boot time (called by main.cpp after NTP sync)
     * @param bootTime Epoch time when system booted
     */
    static void setBootTime(time_t bootTime);
    
    /**
     * @brief Get the stored boot time (for debugging)
     * @return Boot time epoch, or 0 if not set
     */
    static time_t getBootTime();

private:
    static time_t s_bootTime;  // Set by main.cpp during NTP initialization
};


#endif // TIME_UTILS_H