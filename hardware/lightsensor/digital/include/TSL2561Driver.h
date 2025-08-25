#pragma once

#include <Arduino.h>
#include <Wire.h>

// TSL2561 I2C Addresses
#define TSL2561_ADDR_LOW    0x29  // Address with ADDR pin low
#define TSL2561_ADDR_FLOAT  0x39  // Address with ADDR pin floating (default)
#define TSL2561_ADDR_HIGH   0x49  // Address with ADDR pin high

// TSL2561 Register Addresses
#define TSL2561_REG_CONTROL   0x80
#define TSL2561_REG_TIMING    0x81
#define TSL2561_REG_THRESH_LOW_LOW  0x82
#define TSL2561_REG_THRESH_LOW_HIGH 0x83
#define TSL2561_REG_THRESH_HIGH_LOW 0x84
#define TSL2561_REG_THRESH_HIGH_HIGH 0x85
#define TSL2561_REG_INTERRUPT 0x86
#define TSL2561_REG_ID        0x8A
#define TSL2561_REG_DATA0LOW  0x8C
#define TSL2561_REG_DATA0HIGH 0x8D
#define TSL2561_REG_DATA1LOW  0x8E
#define TSL2561_REG_DATA1HIGH 0x8F

// Control Register Values
#define TSL2561_POWER_UP    0x03
#define TSL2561_POWER_DOWN  0x00

// Timing Register Values
#define TSL2561_GAIN_1X     0x00
#define TSL2561_GAIN_16X    0x10
#define TSL2561_INTEGRATIONTIME_13MS  0x00
#define TSL2561_INTEGRATIONTIME_101MS 0x01
#define TSL2561_INTEGRATIONTIME_402MS 0x02

// Device Configuration
struct TSL2561Config {
    uint8_t i2c_address;
    uint16_t integration_time;
    uint8_t gain;
    uint16_t interval;
    uint8_t averaging_samples;
    bool auto_gain;
    bool power_save;
};

// Sensor Reading Structure
struct TSL2561Reading {
    uint16_t ch0;          // Full spectrum + IR
    uint16_t ch1;          // IR only
    float lux;             // Calculated lux value
    bool saturated;        // Saturation flag
    uint8_t gain;          // Current gain setting
    uint16_t integration_time;  // Current integration time
    unsigned long timestamp;    // Reading timestamp
};

class TSL2561Driver {
public:
    TSL2561Driver();
    
    // Initialization and configuration
    bool begin(uint8_t address = TSL2561_ADDR_FLOAT);
    bool isConnected();
    uint8_t getDeviceID();
    
    // Power management
    bool powerUp();
    bool powerDown();
    bool setSleepMode(bool enable);
    
    // Configuration
    bool setGain(uint8_t gain);
    bool setIntegrationTime(uint8_t time);
    bool setConfiguration(const TSL2561Config& config);
    TSL2561Config getConfiguration();
    
    // Reading functions
    bool readRawData(uint16_t& ch0, uint16_t& ch1);
    bool readLux(float& lux);
    bool readSensor(TSL2561Reading& reading);
    bool readAveraged(TSL2561Reading& reading, uint8_t samples = 3);
    
    // Advanced features
    bool autoGainAdjust(TSL2561Reading& reading);
    bool detectSaturation(uint16_t ch0, uint16_t ch1);
    float calculateLux(uint16_t ch0, uint16_t ch1, uint8_t gain, uint8_t integration_time);
    
    // Diagnostic functions
    bool performSelfTest();
    bool validateRegisters();
    bool testI2CCommunication();
    
    // Utility functions
    String getStatusString();
    void printConfiguration();
    void printReading(const TSL2561Reading& reading);

private:
    uint8_t _address;
    TSL2561Config _config;
    bool _initialized;
    
    // Low-level I2C functions
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t& value);
    bool readRegister16(uint8_t reg, uint16_t& value);
    
    // Internal calculation helpers
    float calculateLux_CS(uint16_t ch0, uint16_t ch1, uint8_t gain, uint8_t integration_time);
    float calculateLux_T(uint16_t ch0, uint16_t ch1, uint8_t gain, uint8_t integration_time);
    uint16_t getClipThreshold(uint8_t integration_time);
};