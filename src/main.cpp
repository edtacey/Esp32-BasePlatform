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
#include "utils/Logger.h"
#include "core/Orchestrator.h"

// WiFi credentials
const char* WIFI_SSID = "edtiot";
const char* WIFI_PASSWORD = "spindle1";

// Forward declarations
void setup();
void loop();
void connectWiFi();
void printHeartbeat();
void checkLittleFS();

// Global orchestrator instance
Orchestrator orchestrator;

// Heartbeat timing
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 5000; // 5 seconds

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
    
    // Connect to WiFi
    connectWiFi();
    
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
    
    // Create heartbeat message
    String heartbeat = String("❤️ HEARTBEAT | Uptime: ") + uptime + "s | " +
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
    Logger::info("main", "Checking LittleFS partition and mounting...");
    
    // Try to mount LittleFS
    bool mounted = LittleFS.begin(false);  // Don't format on failure initially
    
    if (mounted) {
        Logger::info("main", "LittleFS mounted successfully");
        
        // Get filesystem info
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        float usage = (float)usedBytes / totalBytes * 100.0f;
        
        Logger::info("main", String("LittleFS: ") + usedBytes/1024 + "KB/" + totalBytes/1024 + "KB used (" + String(usage, 1) + "%)");
        
        // Test write to verify functionality
        File testFile = LittleFS.open("/test.txt", "w");
        if (testFile) {
            testFile.println("ESP32 IoT Orchestrator LittleFS Test");
            testFile.close();
            Logger::info("main", "LittleFS write test: SUCCESS");
            
            // Clean up test file
            LittleFS.remove("/test.txt");
        } else {
            Logger::error("main", "LittleFS write test: FAILED");
        }
        
    } else {
        Logger::error("main", "LittleFS mount failed - attempting format...");
        
        // Try to format and mount
        if (LittleFS.begin(true)) {  // Format on failure
            Logger::info("main", "LittleFS formatted and mounted successfully");
            
            size_t totalBytes = LittleFS.totalBytes();
            Logger::info("main", String("LittleFS: Formatted ") + totalBytes/1024 + "KB available");
        } else {
            Logger::error("main", "LittleFS format and mount FAILED - check partition table!");
        }
    }
}