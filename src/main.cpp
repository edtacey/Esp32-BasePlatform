/**
 * @file main.cpp
 * @brief ESP32 IoT Orchestrator - Baseline Implementation
 * 
 * Minimal orchestrator focusing on:
 * - Component schema management
 * - Default sensor configurations
 * - Persistent storage
 */

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <time.h>
#include <WiFiUdp.h>
#include "utils/Logger.h"
#include "utils/TimeUtils.h"
#include "core/Orchestrator.h"

// WiFi credentials
const char* WIFI_SSID = "edtiot";
const char* WIFI_PASSWORD = "spindle1";

// Forward declarations
void setup();
void loop();
void connectWiFi();
void initializeNTP();
void printHeartbeat();
void checkLittleFS();

// Global orchestrator instance
Orchestrator orchestrator;

// Heartbeat timing
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 5000; // 5 seconds

// NTP configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;     // UTC offset
const int daylightOffset_sec = 0; // No daylight saving
time_t bootTime = 0;              // System boot time in epoch seconds

/**
 * @brief Arduino setup function
 */
void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);
    
    Logger::info("main", "ESP32 IoT Orchestrator - Baseline v1.0.0");
    Logger::info("main", "Starting system initialization...");
    
    // Check LittleFS partition and mounting
    checkLittleFS();
    
    // Enable file logging to LittleFS
    Logger::enableFileLogging(true, 50);  // 50KB max log file
    
    // Connect to WiFi
    connectWiFi();
    
    // Initialize NTP time synchronization
    if (WiFi.status() == WL_CONNECTED) {
        initializeNTP();
    }
    
    // Initialize orchestrator
    if (!orchestrator.init()) {
        Logger::error("main", "Orchestrator initialization failed!");
        while (1) {
            delay(1000);
        }
    }
    
    Logger::info("main", "System initialization complete");
    Logger::info("main", "Entering main loop...");
    
    // Print initial heartbeat
    printHeartbeat();
}

/**
 * @brief Arduino main loop
 */
void loop() {
    // Run orchestrator
    orchestrator.loop();
    
    // Check if it's time for heartbeat
    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        printHeartbeat();
        lastHeartbeat = now;
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}

/**
 * @brief Connect to WiFi network
 */
void connectWiFi() {
    Logger::info("main", "Connecting to WiFi...");
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Logger::info("main", String("WiFi connected! IP: ") + WiFi.localIP().toString());
        Logger::info("main", String("RSSI: ") + WiFi.RSSI() + " dBm");
    } else {
        Logger::error("main", "WiFi connection failed - continuing without network");
    }
}

/**
 * @brief Print system heartbeat with memory and network info
 */
void printHeartbeat() {
    // Memory info
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t usedHeap = totalHeap - freeHeap;
    float memoryUsage = (float)usedHeap / totalHeap * 100.0f;
    
    // Network info
    String ipAddress = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "No WiFi";
    int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    
    // Uptime
    unsigned long uptime = millis() / 1000;
    
    // Create heartbeat message with timestamp
    String timestamp = TimeUtils::getCurrentTimestamp();
    String heartbeat = String("❤️ HEARTBEAT | Time: ") + timestamp + " | Uptime: " + uptime + "s | " +
                      "Memory: " + String(memoryUsage, 1) + "% (" + usedHeap/1024 + "KB/" + totalHeap/1024 + "KB) | " +
                      "IP: " + ipAddress;
    
    if (WiFi.status() == WL_CONNECTED) {
        heartbeat += " | RSSI: " + String(rssi) + "dBm";
    }
    
    Logger::info("HEARTBEAT", heartbeat);
}

/**
 * @brief Check LittleFS partition and mounting status
 */
void checkLittleFS() {
    Logger::info("main", "=== LittleFS PARTITION DETECTION & VERIFICATION ===");
    
    // Check partition table info
    Logger::info("main", "Checking partition table for LittleFS/SPIFFS partition...");
    
    // Try to mount LittleFS
    Logger::info("main", "Attempting LittleFS.begin(false) - no format on fail...");
    bool mounted = LittleFS.begin(false);  // Don't format on failure initially
    
    if (mounted) {
        Logger::info("main", "✅ LittleFS mounted successfully!");
        
        // Get filesystem info
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        float usage = (float)usedBytes / totalBytes * 100.0f;
        
        Logger::info("main", String("📊 LittleFS Stats: ") + usedBytes/1024 + "KB/" + totalBytes/1024 + "KB used (" + String(usage, 1) + "%)");
        Logger::info("main", String("📊 Total bytes: ") + totalBytes + ", Used bytes: " + usedBytes);
        
        // Check if key directories exist
        Logger::info("main", "🔍 Checking filesystem structure...");
        Logger::info("main", String("📁 /www exists: ") + (LittleFS.exists("/www") ? "YES" : "NO"));
        Logger::info("main", String("📁 /config exists: ") + (LittleFS.exists("/config") ? "YES" : "NO"));
        Logger::info("main", String("📁 /system exists: ") + (LittleFS.exists("/system") ? "YES" : "NO"));
        Logger::info("main", String("📁 /logs exists: ") + (LittleFS.exists("/logs") ? "YES" : "NO"));
        
        // Check specific web files
        Logger::info("main", String("📄 /www/index.html exists: ") + (LittleFS.exists("/www/index.html") ? "YES" : "NO"));
        Logger::info("main", String("📄 /www/dashboard.html exists: ") + (LittleFS.exists("/www/dashboard.html") ? "YES" : "NO"));
        Logger::info("main", String("📄 /www/style.css exists: ") + (LittleFS.exists("/www/style.css") ? "YES" : "NO"));
        Logger::info("main", String("📄 /www/app.js exists: ") + (LittleFS.exists("/www/app.js") ? "YES" : "NO"));
        
        // Test write/read operations
        Logger::info("main", "🧪 Testing LittleFS write/read operations...");
        File testFile = LittleFS.open("/test.txt", "w");
        if (testFile) {
            String testData = "ESP32 IoT Orchestrator LittleFS Test - " + String(millis());
            testFile.println(testData);
            testFile.close();
            Logger::info("main", "✅ LittleFS write test: SUCCESS");
            
            // Test read
            testFile = LittleFS.open("/test.txt", "r");
            if (testFile) {
                String readData = testFile.readString();
                testFile.close();
                Logger::info("main", String("✅ LittleFS read test: SUCCESS - Data: ") + readData.substring(0, 50));
                
                // Clean up test file
                if (LittleFS.remove("/test.txt")) {
                    Logger::info("main", "✅ LittleFS delete test: SUCCESS");
                } else {
                    Logger::error("main", "❌ LittleFS delete test: FAILED");
                }
            } else {
                Logger::error("main", "❌ LittleFS read test: FAILED");
            }
        } else {
            Logger::error("main", "❌ LittleFS write test: FAILED - Cannot create test file");
        }
        
        Logger::info("main", "=== LittleFS VERIFICATION COMPLETE ===");
        
    } else {
        Logger::error("main", "❌ LittleFS mount failed - attempting format...");
        
        // Try to format and mount
        Logger::info("main", "🔄 Attempting LittleFS.begin(true) - format on fail...");
        if (LittleFS.begin(true)) {  // Format on failure
            Logger::info("main", "✅ LittleFS formatted and mounted successfully!");
            
            size_t totalBytes = LittleFS.totalBytes();
            Logger::info("main", String("📊 LittleFS: Formatted ") + totalBytes/1024 + "KB available");
            Logger::info("main", "⚠️  Warning: Filesystem was formatted - all data lost!");
        } else {
            Logger::error("main", "❌ CRITICAL: LittleFS format and mount FAILED - check partition table!");
            Logger::error("main", "❌ Partition 'spiffs' may not exist or be corrupted");
        }
    }
}

/**
 * @brief Initialize NTP time synchronization
 */
void initializeNTP() {
    Logger::info("main", "Initializing NTP time synchronization...");
    
    // Configure NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait for time synchronization
    Logger::info("main", "Waiting for NTP time sync...");
    
    int attempts = 0;
    while (!time(nullptr) && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    time_t now = time(nullptr);
    if (now > 0) {
        // Calculate actual boot time by subtracting uptime
        unsigned long uptimeSeconds = millis() / 1000;
        time_t actualBootTime = now - uptimeSeconds;
        
        bootTime = actualBootTime;  // Record actual boot time
        TimeUtils::setBootTime(actualBootTime);  // Share with TimeUtils
        
        // Format and display current time
        struct tm* timeinfo = localtime(&now);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", timeinfo);
        
        Logger::info("main", String("NTP sync successful! Current time: ") + timeStr);
        Logger::info("main", String("System boot time: ") + actualBootTime + " (epoch seconds)");
        Logger::info("main", String("Uptime at NTP sync: ") + uptimeSeconds + " seconds");
    } else {
        Logger::error("main", "NTP sync failed - using millis() for timing");
        bootTime = 0;  // Indicate NTP failed
    }
}

