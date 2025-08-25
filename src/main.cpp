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
#include "utils/Logger.h"
#include "core/Orchestrator.h"

// Forward declarations
void setup();
void loop();

// Global orchestrator instance
Orchestrator orchestrator;

/**
 * @brief Arduino setup function
 */
void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);
    
    Logger::info("main", "ESP32 IoT Orchestrator - Baseline v1.0.0");
    Logger::info("main", "Starting system initialization...");
    
    // Initialize orchestrator
    if (!orchestrator.init()) {
        Logger::error("main", "Orchestrator initialization failed!");
        while (1) {
            delay(1000);
        }
    }
    
    Logger::info("main", "System initialization complete");
    Logger::info("main", "Entering main loop...");
}

/**
 * @brief Arduino main loop
 */
void loop() {
    // Run orchestrator
    orchestrator.loop();
    
    // Small delay to prevent watchdog issues
    delay(10);
}