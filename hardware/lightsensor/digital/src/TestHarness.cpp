#include "TestHarness.h"

TestHarness::TestHarness() : _initialized(false) {
    // Set default test configuration
    _config.enable_i2c_tests = true;
    _config.enable_power_tests = true;
    _config.enable_register_tests = true;
    _config.enable_reading_tests = true;
    _config.enable_calibration_tests = true;
    _config.enable_stress_tests = false;  // Disabled by default for MVP
    _config.verbose_output = true;
    _config.test_timeout_ms = 5000;
    
    resetTestStats();
}

bool TestHarness::initialize() {
    printHeader("TSL2561 Test Harness Initialization");
    
    Serial.printf("I2C SDA Pin: %d\n", TSL2561_SDA_PIN);
    Serial.printf("I2C SCL Pin: %d\n", TSL2561_SCL_PIN);
    Serial.printf("I2C Frequency: %d Hz\n", TSL2561_I2C_FREQ);
    Serial.printf("Target I2C Address: 0x%02X\n", TSL2561_I2C_ADDRESS);
    
    // Scan for I2C devices first
    scanI2CAddresses();
    
    // Initialize sensor with default address
    if (!_sensor.begin(TSL2561_I2C_ADDRESS)) {
        Serial.println("ERROR: Failed to initialize TSL2561 sensor");
        return false;
    }
    
    Serial.printf("Device ID: 0x%02X\n", _sensor.getDeviceID());
    _initialized = true;
    
    Serial.println("Test Harness initialized successfully!");
    printSeparator();
    
    return true;
}

bool TestHarness::runAllTests() {
    if (!_initialized) {
        Serial.println("ERROR: Test harness not initialized");
        return false;
    }
    
    resetTestStats();
    _stats.start_time = millis();
    
    printHeader("Running Comprehensive TSL2561 Test Suite");
    
    // Core hardware tests
    if (_config.enable_i2c_tests) {
        logTest("I2C Communication", testI2CCommunication());
        logTest("Device Identification", testDeviceIdentification());
    }
    
    if (_config.enable_power_tests) {
        logTest("Power Management", testPowerManagement());
    }
    
    if (_config.enable_register_tests) {
        logTest("Register Access", testRegisterAccess());
    }
    
    if (_config.enable_reading_tests) {
        logTest("Sensor Readings", testSensorReadings());
        logTest("Gain Settings", testGainSettings());
        logTest("Integration Times", testIntegrationTimes());
        logTest("Auto Gain", testAutoGain());
        logTest("Saturation Detection", testSaturationDetection());
        logTest("Averaging", testAveraging());
    }
    
    if (_config.enable_calibration_tests) {
        logTest("Known Configuration", testKnownConfiguration());
        logTest("Calibration", testCalibration());
    }
    
    if (_config.enable_stress_tests) {
        logTest("Stress Conditions", testStressConditions());
    }
    
    _stats.end_time = millis();
    
    printTestResults();
    return (_stats.failed_tests == 0);
}

TestResult TestHarness::testI2CCommunication() {
    printHeader("I2C Communication Test");
    
    // Test basic I2C communication
    if (!_sensor.testI2CCommunication()) {
        Serial.println("FAIL: Basic I2C communication failed");
        return TEST_FAIL;
    }
    
    Serial.println("PASS: Basic I2C communication successful");
    
    // Test if device responds to expected address
    if (!_sensor.isConnected()) {
        Serial.println("FAIL: Device not responding at expected address");
        return TEST_FAIL;
    }
    
    Serial.println("PASS: Device responds at expected address");
    return TEST_PASS;
}

TestResult TestHarness::testDeviceIdentification() {
    printHeader("Device Identification Test");
    
    uint8_t device_id = _sensor.getDeviceID();
    Serial.printf("Device ID: 0x%02X\n", device_id);
    
    // TSL2561 should have device ID with pattern 0x5X
    if ((device_id & 0xF0) != 0x50) {
        Serial.printf("FAIL: Unexpected device ID. Expected 0x5X, got 0x%02X\n", device_id);
        return TEST_FAIL;
    }
    
    Serial.printf("PASS: Valid TSL2561 device ID detected (0x%02X)\n", device_id);
    return TEST_PASS;
}

TestResult TestHarness::testPowerManagement() {
    printHeader("Power Management Test");
    
    // Test power down
    if (!_sensor.powerDown()) {
        Serial.println("FAIL: Power down failed");
        return TEST_FAIL;
    }
    Serial.println("PASS: Power down successful");
    
    delay(100);
    
    // Test power up
    if (!_sensor.powerUp()) {
        Serial.println("FAIL: Power up failed");
        return TEST_FAIL;
    }
    Serial.println("PASS: Power up successful");
    
    delay(100);
    
    // Verify device still responds after power cycle
    if (!_sensor.isConnected()) {
        Serial.println("FAIL: Device not responding after power cycle");
        return TEST_FAIL;
    }
    
    Serial.println("PASS: Device responds after power cycle");
    return TEST_PASS;
}

TestResult TestHarness::testRegisterAccess() {
    printHeader("Register Access Test");
    
    if (!_sensor.validateRegisters()) {
        Serial.println("FAIL: Register validation failed");
        return TEST_FAIL;
    }
    
    Serial.println("PASS: Register access validated");
    
    // Test gain setting
    if (!_sensor.setGain(1) || !_sensor.setGain(16)) {
        Serial.println("FAIL: Gain setting failed");
        return TEST_FAIL;
    }
    
    Serial.println("PASS: Gain settings successful");
    
    // Reset to default gain
    _sensor.setGain(1);
    
    return TEST_PASS;
}

TestResult TestHarness::testSensorReadings() {
    printHeader("Sensor Reading Test");
    
    TSL2561Reading reading;
    
    // Take a basic reading
    if (!_sensor.readSensor(reading)) {
        Serial.println("FAIL: Failed to read sensor data");
        return TEST_FAIL;
    }
    
    Serial.printf("Channel 0: %u\n", reading.ch0);
    Serial.printf("Channel 1: %u\n", reading.ch1);
    Serial.printf("Lux: %.2f\n", reading.lux);
    
    // Validate reading makes sense
    if (reading.ch0 == 0 && reading.ch1 == 0) {
        Serial.println("WARNING: Both channels read zero - check lighting conditions");
        return TEST_WARNING;
    }
    
    if (reading.lux < 0) {
        Serial.println("WARNING: Negative lux reading - possible saturation");
        return TEST_WARNING;
    }
    
    Serial.println("PASS: Sensor readings successful");
    return TEST_PASS;
}

TestResult TestHarness::testGainSettings() {
    printHeader("Gain Settings Test");
    
    TSL2561Reading reading_1x, reading_16x;
    
    // Test 1x gain
    _sensor.setGain(1);
    delay(500);
    if (!_sensor.readSensor(reading_1x)) {
        Serial.println("FAIL: Failed to read with 1x gain");
        return TEST_FAIL;
    }
    
    Serial.printf("1x Gain - Ch0: %u, Ch1: %u, Lux: %.2f\n", 
                  reading_1x.ch0, reading_1x.ch1, reading_1x.lux);
    
    // Test 16x gain
    _sensor.setGain(16);
    delay(500);
    if (!_sensor.readSensor(reading_16x)) {
        Serial.println("FAIL: Failed to read with 16x gain");
        return TEST_FAIL;
    }
    
    Serial.printf("16x Gain - Ch0: %u, Ch1: %u, Lux: %.2f\n", 
                  reading_16x.ch0, reading_16x.ch1, reading_16x.lux);
    
    // Validate gain behavior (16x should give higher raw readings in low light)
    if (reading_1x.ch0 < 1000 && reading_16x.ch0 <= reading_1x.ch0) {
        Serial.println("WARNING: 16x gain not showing expected increase");
        return TEST_WARNING;
    }
    
    Serial.println("PASS: Gain settings working correctly");
    
    // Reset to 1x gain
    _sensor.setGain(1);
    return TEST_PASS;
}

TestResult TestHarness::testIntegrationTimes() {
    printHeader("Integration Time Test");
    
    // Test different integration times
    uint16_t times[] = {13, 101, 402};
    
    for (uint8_t i = 0; i < 3; i++) {
        uint8_t time_reg;
        switch (times[i]) {
            case 13:
                time_reg = 0;
                break;
            case 101:
                time_reg = 1;
                break;
            case 402:
            default:
                time_reg = 2;
                break;
        }
        
        if (!_sensor.setIntegrationTime(time_reg)) {
            Serial.printf("FAIL: Failed to set %ums integration time\n", times[i]);
            return TEST_FAIL;
        }
        
        TSL2561Reading reading;
        if (!_sensor.readSensor(reading)) {
            Serial.printf("FAIL: Failed to read with %ums integration time\n", times[i]);
            return TEST_FAIL;
        }
        
        Serial.printf("%ums Integration - Ch0: %u, Ch1: %u, Lux: %.2f\n", 
                      times[i], reading.ch0, reading.ch1, reading.lux);
    }
    
    Serial.println("PASS: Integration time settings successful");
    
    // Reset to default (402ms)
    _sensor.setIntegrationTime(2);
    return TEST_PASS;
}

TestResult TestHarness::testAutoGain() {
    printHeader("Auto Gain Test");
    
    TSL2561Reading reading;
    if (!_sensor.autoGainAdjust(reading)) {
        Serial.println("FAIL: Auto gain adjustment failed");
        return TEST_FAIL;
    }
    
    Serial.printf("Auto Gain Result - Gain: %ux, Ch0: %u, Ch1: %u, Lux: %.2f\n",
                  reading.gain, reading.ch0, reading.ch1, reading.lux);
    
    Serial.println("PASS: Auto gain adjustment successful");
    return TEST_PASS;
}

TestResult TestHarness::testSaturationDetection() {
    printHeader("Saturation Detection Test");
    
    // Test with current conditions
    TSL2561Reading reading;
    if (!_sensor.readSensor(reading)) {
        Serial.println("FAIL: Failed to read sensor for saturation test");
        return TEST_FAIL;
    }
    
    bool is_saturated = _sensor.detectSaturation(reading.ch0, reading.ch1);
    Serial.printf("Saturation Status: %s (Ch0: %u, Ch1: %u)\n", 
                  is_saturated ? "SATURATED" : "OK", reading.ch0, reading.ch1);
    
    Serial.println("PASS: Saturation detection functional");
    return TEST_PASS;
}

TestResult TestHarness::testAveraging() {
    printHeader("Averaging Test");
    
    TSL2561Reading averaged_reading;
    if (!_sensor.readAveraged(averaged_reading, 5)) {
        Serial.println("FAIL: Averaged reading failed");
        return TEST_FAIL;
    }
    
    Serial.printf("Averaged Reading (5 samples) - Ch0: %u, Ch1: %u, Lux: %.2f\n",
                  averaged_reading.ch0, averaged_reading.ch1, averaged_reading.lux);
    
    Serial.println("PASS: Averaging functionality successful");
    return TEST_PASS;
}

TestResult TestHarness::testKnownConfiguration() {
    printHeader("Known Configuration Test");
    
    // Test with known good configuration
    TSL2561Config known_config = getKnownGoodConfig();
    
    // Set known configuration
    _sensor.setGain(known_config.gain);
    _sensor.setIntegrationTime(2);  // 402ms
    
    delay(500);
    
    TSL2561Reading reading;
    if (!_sensor.readSensor(reading)) {
        Serial.println("FAIL: Failed to read with known configuration");
        return TEST_FAIL;
    }
    
    Serial.printf("Known Config Reading - Ch0: %u, Ch1: %u, Lux: %.2f\n",
                  reading.ch0, reading.ch1, reading.lux);
    
    // Basic validation - readings should be non-negative and reasonable
    if (reading.lux < 0 || reading.lux > 100000) {
        Serial.printf("WARNING: Unusual lux reading: %.2f\n", reading.lux);
        return TEST_WARNING;
    }
    
    Serial.println("PASS: Known configuration test successful");
    return TEST_PASS;
}

TestResult TestHarness::testCalibration() {
    printHeader("Calibration Test");
    
    // Perform basic calibration validation
    if (!_sensor.performSelfTest()) {
        Serial.println("FAIL: Self-test failed");
        return TEST_FAIL;
    }
    
    Serial.println("PASS: Self-test successful");
    
    // Take multiple readings to check consistency
    const uint8_t num_readings = 3;
    TSL2561Reading readings[num_readings];
    
    for (uint8_t i = 0; i < num_readings; i++) {
        if (!_sensor.readSensor(readings[i])) {
            Serial.printf("FAIL: Failed to read sample %d\n", i + 1);
            return TEST_FAIL;
        }
        delay(200);
    }
    
    // Check consistency (readings should be within reasonable range)
    float lux_variance = 0;
    float avg_lux = (readings[0].lux + readings[1].lux + readings[2].lux) / 3.0;
    
    for (uint8_t i = 0; i < num_readings; i++) {
        float diff = readings[i].lux - avg_lux;
        lux_variance += diff * diff;
    }
    lux_variance /= num_readings;
    
    Serial.printf("Average Lux: %.2f, Variance: %.2f\n", avg_lux, lux_variance);
    
    if (lux_variance > (avg_lux * 0.2)) {  // 20% variance threshold
        Serial.println("WARNING: High variance in readings");
        return TEST_WARNING;
    }
    
    Serial.println("PASS: Calibration test successful");
    return TEST_PASS;
}

TestResult TestHarness::testStressConditions() {
    printHeader("Stress Test");
    
    Serial.println("Running rapid read test...");
    
    // Rapid reading test
    const uint16_t rapid_reads = 20;
    uint16_t successful_reads = 0;
    
    for (uint16_t i = 0; i < rapid_reads; i++) {
        uint16_t ch0, ch1;
        if (_sensor.readRawData(ch0, ch1)) {
            successful_reads++;
        }
        delay(50);  // Minimal delay
    }
    
    float success_rate = (float)successful_reads / rapid_reads * 100.0;
    Serial.printf("Rapid read success rate: %.1f%% (%u/%u)\n", 
                  success_rate, successful_reads, rapid_reads);
    
    if (success_rate < 90.0) {
        Serial.println("FAIL: Low success rate in stress test");
        return TEST_FAIL;
    }
    
    Serial.println("PASS: Stress test successful");
    return TEST_PASS;
}

void TestHarness::runContinuousMode() {
    static unsigned long last_reading = 0;
    unsigned long current_time = millis();
    
    if (current_time - last_reading >= 5000) {  // Every 5 seconds
        TSL2561Reading reading;
        if (_sensor.readSensor(reading)) {
            Serial.printf("[MONITOR] Lux: %.2f | Ch0: %u | Ch1: %u | Gain: %ux | %s\n",
                          reading.lux, reading.ch0, reading.ch1, reading.gain,
                          reading.saturated ? "SAT" : "OK");
        } else {
            Serial.println("[MONITOR] Reading failed");
        }
        last_reading = current_time;
    }
}

void TestHarness::scanI2CAddresses() {
    Serial.println("Scanning I2C addresses...");
    
    uint8_t found_devices = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", address);
            found_devices++;
        }
    }
    
    if (found_devices == 0) {
        Serial.println("No I2C devices found!");
    } else {
        Serial.printf("Found %u I2C device(s)\n", found_devices);
    }
    printSeparator('-', 30);
}

void TestHarness::printTestResults() {
    printHeader("Test Results Summary");
    
    Serial.printf("Total Tests:    %u\n", _stats.total_tests);
    Serial.printf("Passed:         %u\n", _stats.passed_tests);
    Serial.printf("Failed:         %u\n", _stats.failed_tests);
    Serial.printf("Warnings:       %u\n", _stats.warning_tests);
    Serial.printf("Skipped:        %u\n", _stats.skipped_tests);
    Serial.printf("Duration:       %lu ms\n", _stats.end_time - _stats.start_time);
    
    float pass_rate = (_stats.total_tests > 0) ? 
                      ((float)_stats.passed_tests / _stats.total_tests * 100.0) : 0.0;
    Serial.printf("Pass Rate:      %.1f%%\n", pass_rate);
    
    if (_stats.failed_tests == 0) {
        Serial.println("STATUS: ALL TESTS PASSED! ✓");
    } else {
        Serial.printf("STATUS: %u TEST(S) FAILED! ✗\n", _stats.failed_tests);
    }
    
    printSeparator('=', 50);
}

void TestHarness::logTest(const String& testName, TestResult result, const String& details) {
    _stats.total_tests++;
    
    switch (result) {
        case TEST_PASS:
            _stats.passed_tests++;
            if (_config.verbose_output) {
                Serial.printf("[PASS] %s\n", testName.c_str());
            }
            break;
        case TEST_FAIL:
            _stats.failed_tests++;
            Serial.printf("[FAIL] %s\n", testName.c_str());
            break;
        case TEST_WARNING:
            _stats.warning_tests++;
            Serial.printf("[WARN] %s\n", testName.c_str());
            break;
        case TEST_SKIP:
            _stats.skipped_tests++;
            if (_config.verbose_output) {
                Serial.printf("[SKIP] %s\n", testName.c_str());
            }
            break;
    }
    
    if (details.length() > 0) {
        Serial.printf("       %s\n", details.c_str());
    }
}

TSL2561Config TestHarness::getKnownGoodConfig() {
    TSL2561Config config;
    config.i2c_address = TSL2561_I2C_ADDRESS;
    config.integration_time = 402;
    config.gain = 1;
    config.interval = 2000;
    config.averaging_samples = 3;
    config.auto_gain = true;
    config.power_save = false;
    return config;
}

void TestHarness::printHeader(const String& title) {
    printSeparator('=', 50);
    Serial.printf(" %s\n", title.c_str());
    printSeparator('=', 50);
}

void TestHarness::printSeparator(char character, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        Serial.print(character);
    }
    Serial.println();
}

void TestHarness::resetTestStats() {
    _stats.total_tests = 0;
    _stats.passed_tests = 0;
    _stats.failed_tests = 0;
    _stats.warning_tests = 0;
    _stats.skipped_tests = 0;
    _stats.start_time = 0;
    _stats.end_time = 0;
}