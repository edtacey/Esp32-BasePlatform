#include <Arduino.h>
#include "TSL2561Driver.h"
#include "TestHarness.h"

TSL2561Driver sensor;
TestHarness testHarness;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Allow serial monitor to connect
    
    Serial.println("=== TSL2561 Test Harness MVP ===");
    Serial.println("Version: " TEST_HARNESS_VERSION);
    Serial.println("================================");
    
    // Initialize test harness
    testHarness.initialize();
    
    // Run comprehensive hardware validation
    testHarness.runAllTests();
}

void loop() {
    // Run continuous monitoring mode
    testHarness.runContinuousMode();
    delay(5000);  // 5-second intervals for continuous monitoring
}