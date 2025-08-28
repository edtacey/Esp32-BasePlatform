/**
 * @file TimeUtils.cpp
 * @brief Time utilities implementation
 */

#include "TimeUtils.h"

// Static member initialization
time_t TimeUtils::s_bootTime = 0;

time_t TimeUtils::getEpochTime() {
    time_t now = time(nullptr);
    if (now > 0) {
        return now;  // NTP time available
    } else if (s_bootTime > 0) {
        // Fallback: boot time + elapsed seconds
        return s_bootTime + (millis() / 1000);
    } else {
        return 0;  // No time reference available
    }
}

String TimeUtils::getCurrentTimestamp() {
    time_t now = getEpochTime();
    if (now > 0) {
        struct tm* timeinfo = localtime(&now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
        return String(timeStr);
    } else {
        // Fallback to boot time
        return String("Boot+") + String(millis() / 1000) + "s";
    }
}

String TimeUtils::getTimeSince(time_t pastEpoch) {
    if (pastEpoch <= 0) {
        return "unknown";
    }
    
    time_t now = getEpochTime();
    if (now <= 0) {
        return "no time ref";
    }
    
    if (now < pastEpoch) {
        return "future time";
    }
    
    time_t diff = now - pastEpoch;
    
    // Format human-readable duration
    if (diff < 60) {
        return String(diff) + "s ago";
    } else if (diff < 3600) {  // < 1 hour
        int minutes = diff / 60;
        int seconds = diff % 60;
        if (seconds > 0) {
            return String(minutes) + "m " + String(seconds) + "s ago";
        } else {
            return String(minutes) + "m ago";
        }
    } else if (diff < 86400) {  // < 1 day
        int hours = diff / 3600;
        int minutes = (diff % 3600) / 60;
        if (minutes > 0) {
            return String(hours) + "h " + String(minutes) + "m ago";
        } else {
            return String(hours) + "h ago";
        }
    } else {  // >= 1 day
        int days = diff / 86400;
        int hours = (diff % 86400) / 3600;
        if (hours > 0) {
            return String(days) + "d " + String(hours) + "h ago";
        } else {
            return String(days) + "d ago";
        }
    }
}

unsigned long TimeUtils::getBootMillis() {
    return millis();
}

unsigned long TimeUtils::getBootSeconds() {
    return millis() / 1000;
}

bool TimeUtils::isNTPAvailable() {
    return time(nullptr) > 0;
}

time_t TimeUtils::millisToEpoch(unsigned long millisTime) {
    if (s_bootTime > 0) {
        return s_bootTime + (millisTime / 1000);
    }
    
    // Fallback: try to calculate from current time if NTP is available
    time_t now = time(nullptr);
    if (now > 0) {
        unsigned long currentMillis = millis();
        time_t calculatedBootTime = now - (currentMillis / 1000);
        return calculatedBootTime + (millisTime / 1000);
    }
    
    return 0;  // Cannot convert without time reference
}

void TimeUtils::setBootTime(time_t bootTime) {
    s_bootTime = bootTime;
}

time_t TimeUtils::getBootTime() {
    return s_bootTime;
}