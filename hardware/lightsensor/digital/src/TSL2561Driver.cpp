#include "TSL2561Driver.h"

TSL2561Driver::TSL2561Driver() : _address(TSL2561_ADDR_FLOAT), _initialized(false) {
    // Set default configuration
    _config.i2c_address = TSL2561_ADDR_FLOAT;
    _config.integration_time = 402;
    _config.gain = 1;
    _config.interval = 2000;
    _config.averaging_samples = 3;
    _config.auto_gain = true;
    _config.power_save = true;
}

bool TSL2561Driver::begin(uint8_t address) {
    _address = address;
    _config.i2c_address = address;
    
    Wire.begin(TSL2561_SDA_PIN, TSL2561_SCL_PIN);
    Wire.setClock(TSL2561_I2C_FREQ);
    
    // Test I2C communication
    if (!isConnected()) {
        Serial.println("TSL2561: I2C communication failed");
        return false;
    }
    
    // Power up the device
    if (!powerUp()) {
        Serial.println("TSL2561: Power up failed");
        return false;
    }
    
    delay(10);  // Allow device to stabilize
    
    // Set default timing and gain
    if (!setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS) || !setGain(TSL2561_GAIN_1X)) {
        Serial.println("TSL2561: Configuration failed");
        return false;
    }
    
    _initialized = true;
    Serial.println("TSL2561: Initialized successfully");
    return true;
}

bool TSL2561Driver::isConnected() {
    uint8_t id;
    if (!readRegister(TSL2561_REG_ID, id)) {
        return false;
    }
    
    // TSL2561 should return 0x50 (Part Number = 0x5, Rev Number = 0x0)
    // or similar values like 0x51, 0x52 for different revisions
    return (id & 0xF0) == 0x50;
}

uint8_t TSL2561Driver::getDeviceID() {
    uint8_t id = 0;
    readRegister(TSL2561_REG_ID, id);
    return id;
}

bool TSL2561Driver::powerUp() {
    return writeRegister(TSL2561_REG_CONTROL, TSL2561_POWER_UP);
}

bool TSL2561Driver::powerDown() {
    return writeRegister(TSL2561_REG_CONTROL, TSL2561_POWER_DOWN);
}

bool TSL2561Driver::setSleepMode(bool enable) {
    if (enable) {
        return powerDown();
    } else {
        return powerUp();
    }
}

bool TSL2561Driver::setGain(uint8_t gain) {
    uint8_t timing_reg;
    if (!readRegister(TSL2561_REG_TIMING, timing_reg)) {
        return false;
    }
    
    // Clear gain bits and set new gain
    timing_reg &= 0xEF;  // Clear bit 4 (gain bit)
    if (gain == 16) {
        timing_reg |= TSL2561_GAIN_16X;
        _config.gain = 16;
    } else {
        _config.gain = 1;
    }
    
    return writeRegister(TSL2561_REG_TIMING, timing_reg);
}

bool TSL2561Driver::setIntegrationTime(uint8_t time) {
    uint8_t timing_reg;
    if (!readRegister(TSL2561_REG_TIMING, timing_reg)) {
        return false;
    }
    
    // Clear integration time bits (0-1) and set new time
    timing_reg &= 0xFC;
    timing_reg |= (time & 0x03);
    
    // Update config based on time value
    switch (time) {
        case TSL2561_INTEGRATIONTIME_13MS:
            _config.integration_time = 13;
            break;
        case TSL2561_INTEGRATIONTIME_101MS:
            _config.integration_time = 101;
            break;
        case TSL2561_INTEGRATIONTIME_402MS:
        default:
            _config.integration_time = 402;
            break;
    }
    
    return writeRegister(TSL2561_REG_TIMING, timing_reg);
}

bool TSL2561Driver::readRawData(uint16_t& ch0, uint16_t& ch1) {
    if (!_initialized) {
        return false;
    }
    
    // Wait for integration time to complete
    delay(_config.integration_time + 10);
    
    uint8_t ch0_low, ch0_high, ch1_low, ch1_high;
    
    if (!readRegister(TSL2561_REG_DATA0LOW, ch0_low) ||
        !readRegister(TSL2561_REG_DATA0HIGH, ch0_high) ||
        !readRegister(TSL2561_REG_DATA1LOW, ch1_low) ||
        !readRegister(TSL2561_REG_DATA1HIGH, ch1_high)) {
        return false;
    }
    
    ch0 = (ch0_high << 8) | ch0_low;
    ch1 = (ch1_high << 8) | ch1_low;
    
    return true;
}

bool TSL2561Driver::readLux(float& lux) {
    uint16_t ch0, ch1;
    if (!readRawData(ch0, ch1)) {
        return false;
    }
    
    lux = calculateLux(ch0, ch1, _config.gain, _config.integration_time);
    return true;
}

bool TSL2561Driver::readSensor(TSL2561Reading& reading) {
    if (!readRawData(reading.ch0, reading.ch1)) {
        return false;
    }
    
    reading.gain = _config.gain;
    reading.integration_time = _config.integration_time;
    reading.saturated = detectSaturation(reading.ch0, reading.ch1);
    reading.lux = calculateLux(reading.ch0, reading.ch1, _config.gain, _config.integration_time);
    reading.timestamp = millis();
    
    return true;
}

bool TSL2561Driver::readAveraged(TSL2561Reading& reading, uint8_t samples) {
    if (samples == 0) samples = 1;
    
    float total_lux = 0;
    uint32_t total_ch0 = 0, total_ch1 = 0;
    uint8_t valid_readings = 0;
    
    for (uint8_t i = 0; i < samples; i++) {
        TSL2561Reading temp_reading;
        if (readSensor(temp_reading)) {
            total_lux += temp_reading.lux;
            total_ch0 += temp_reading.ch0;
            total_ch1 += temp_reading.ch1;
            valid_readings++;
            
            if (i < samples - 1) {
                delay(100);  // Small delay between readings
            }
        }
    }
    
    if (valid_readings == 0) {
        return false;
    }
    
    reading.lux = total_lux / valid_readings;
    reading.ch0 = total_ch0 / valid_readings;
    reading.ch1 = total_ch1 / valid_readings;
    reading.gain = _config.gain;
    reading.integration_time = _config.integration_time;
    reading.saturated = detectSaturation(reading.ch0, reading.ch1);
    reading.timestamp = millis();
    
    return true;
}

bool TSL2561Driver::autoGainAdjust(TSL2561Reading& reading) {
    if (!_config.auto_gain) {
        return readSensor(reading);
    }
    
    // Start with 1x gain
    setGain(1);
    if (!readSensor(reading)) {
        return false;
    }
    
    // If reading is too low and not saturated, try 16x gain
    if (reading.ch0 < 100 && !reading.saturated) {
        setGain(16);
        delay(_config.integration_time + 10);
        return readSensor(reading);
    }
    
    return true;
}

bool TSL2561Driver::detectSaturation(uint16_t ch0, uint16_t ch1) {
    uint16_t clip_threshold = getClipThreshold(_config.integration_time);
    return (ch0 >= clip_threshold || ch1 >= clip_threshold);
}

float TSL2561Driver::calculateLux(uint16_t ch0, uint16_t ch1, uint8_t gain, uint8_t integration_time) {
    // Check for saturation first
    if (detectSaturation(ch0, ch1)) {
        return -1.0;  // Saturated reading
    }
    
    // Scale readings based on integration time and gain
    float scale_factor = 1.0;
    
    // Integration time scaling
    switch (integration_time) {
        case 13:
            scale_factor = 402.0 / 13.7;  // Scale to 402ms equivalent
            break;
        case 101:
            scale_factor = 402.0 / 101.0;
            break;
        case 402:
        default:
            scale_factor = 1.0;
            break;
    }
    
    // Gain scaling
    if (gain == 16) {
        scale_factor /= 16.0;
    }
    
    // Apply scaling
    float scaled_ch0 = ch0 * scale_factor;
    float scaled_ch1 = ch1 * scale_factor;
    
    // Calculate ratio
    float ratio = 0;
    if (scaled_ch0 != 0) {
        ratio = scaled_ch1 / scaled_ch0;
    }
    
    float lux = 0;
    
    // TSL2561 lux calculation formula (simplified)
    if (ratio <= 0.125) {
        lux = 0.0304 * scaled_ch0 - 0.062 * scaled_ch0 * pow(ratio, 1.4);
    } else if (ratio <= 0.25) {
        lux = 0.0224 * scaled_ch0 - 0.031 * scaled_ch1;
    } else if (ratio <= 0.375) {
        lux = 0.0128 * scaled_ch0 - 0.0153 * scaled_ch1;
    } else if (ratio <= 0.5) {
        lux = 0.00146 * scaled_ch0 - 0.00112 * scaled_ch1;
    } else if (ratio <= 0.61) {
        lux = 0.00032 * scaled_ch0 - 0.00026 * scaled_ch1;
    } else if (ratio <= 0.80) {
        lux = 0.00018 * scaled_ch0 - 0.00015 * scaled_ch1;
    } else if (ratio <= 1.30) {
        lux = 0.00006 * scaled_ch0 - 0.00005 * scaled_ch1;
    } else {
        lux = 0;
    }
    
    return max(0.0f, lux);
}

uint16_t TSL2561Driver::getClipThreshold(uint8_t integration_time) {
    switch (integration_time) {
        case 13:
            return 4900;
        case 101:
            return 37000;
        case 402:
        default:
            return 65000;
    }
}

bool TSL2561Driver::performSelfTest() {
    if (!isConnected()) {
        return false;
    }
    
    // Test power cycling
    if (!powerDown() || !powerUp()) {
        return false;
    }
    
    delay(10);
    
    // Test register access
    return validateRegisters();
}

bool TSL2561Driver::validateRegisters() {
    uint8_t control_reg, timing_reg, id_reg;
    
    if (!readRegister(TSL2561_REG_CONTROL, control_reg) ||
        !readRegister(TSL2561_REG_TIMING, timing_reg) ||
        !readRegister(TSL2561_REG_ID, id_reg)) {
        return false;
    }
    
    // Validate register values are reasonable
    return (control_reg == TSL2561_POWER_UP) && ((id_reg & 0xF0) == 0x50);
}

bool TSL2561Driver::testI2CCommunication() {
    Wire.beginTransmission(_address);
    return (Wire.endTransmission() == 0);
}

void TSL2561Driver::printReading(const TSL2561Reading& reading) {
    Serial.println("=== TSL2561 Reading ===");
    Serial.printf("Channel 0 (Full): %u\n", reading.ch0);
    Serial.printf("Channel 1 (IR):   %u\n", reading.ch1);
    Serial.printf("Lux Value:        %.2f\n", reading.lux);
    Serial.printf("Gain:             %ux\n", reading.gain);
    Serial.printf("Integration:      %ums\n", reading.integration_time);
    Serial.printf("Saturated:        %s\n", reading.saturated ? "YES" : "NO");
    Serial.printf("Timestamp:        %lu ms\n", reading.timestamp);
    Serial.println("======================");
}

bool TSL2561Driver::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

bool TSL2561Driver::readRegister(uint8_t reg, uint8_t& value) {
    Wire.beginTransmission(_address);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    if (Wire.requestFrom(_address, (uint8_t)1) != 1) {
        return false;
    }
    
    value = Wire.read();
    return true;
}