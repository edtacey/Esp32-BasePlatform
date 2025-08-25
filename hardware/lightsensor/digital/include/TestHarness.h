#pragma once

#include <Arduino.h>
#include "TSL2561Driver.h"

// Test Result Codes
enum TestResult {
    TEST_PASS = 0,
    TEST_FAIL = 1,
    TEST_WARNING = 2,
    TEST_SKIP = 3
};

// Test Configuration
struct TestConfig {
    bool enable_i2c_tests;
    bool enable_power_tests;
    bool enable_register_tests;
    bool enable_reading_tests;
    bool enable_calibration_tests;
    bool enable_stress_tests;
    bool verbose_output;
    uint16_t test_timeout_ms;
};

// Test Statistics
struct TestStats {
    uint16_t total_tests;
    uint16_t passed_tests;
    uint16_t failed_tests;
    uint16_t warning_tests;
    uint16_t skipped_tests;
    unsigned long start_time;
    unsigned long end_time;
};

class TestHarness {
public:
    TestHarness();
    
    // Initialization
    bool initialize();
    void setTestConfig(const TestConfig& config);
    TestConfig getTestConfig();
    
    // Main test execution
    bool runAllTests();
    void runContinuousMode();
    
    // Individual test categories
    TestResult testI2CCommunication();
    TestResult testDeviceIdentification();
    TestResult testPowerManagement();
    TestResult testRegisterAccess();
    TestResult testSensorReadings();
    TestResult testGainSettings();
    TestResult testIntegrationTimes();
    TestResult testAutoGain();
    TestResult testSaturationDetection();
    TestResult testAveraging();
    TestResult testKnownConfiguration();
    TestResult testCalibration();
    TestResult testStressConditions();
    
    // Diagnostic functions
    void scanI2CAddresses();
    void dumpAllRegisters();
    void validateWiring();
    void measurePerformance();
    
    // Utility functions
    void printTestResults();
    void printDetailedReport();
    void resetTestStats();
    String getTestResultString(TestResult result);
    
    // Known good configuration testing
    bool testReferenceConfiguration();
    bool validateExpectedReadings();
    
private:
    TSL2561Driver _sensor;
    TestConfig _config;
    TestStats _stats;
    bool _initialized;
    
    // Helper functions
    void logTest(const String& testName, TestResult result, const String& details = "");
    bool waitForStableReading(uint16_t max_attempts = 10);
    TestResult compareReadings(const TSL2561Reading& reading1, const TSL2561Reading& reading2, float tolerance = 0.1);
    void printSeparator(char character = '=', uint8_t length = 50);
    void printHeader(const String& title);
    
    // Test configuration presets
    TSL2561Config getKnownGoodConfig();
    TSL2561Config getTestConfig1x();
    TSL2561Config getTestConfig16x();
};