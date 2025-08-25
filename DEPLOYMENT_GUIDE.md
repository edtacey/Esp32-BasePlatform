# ESP32 IoT Orchestrator - Deployment Guide

## Overview

This deployment system provides robust, automated deployment of the ESP32 IoT Orchestrator firmware with native Linux terminal support, backup capabilities, and comprehensive monitoring.

## Quick Start

### 1. One-Command Full Deployment
```bash
./deploy.sh --all
```
This will:
- Build firmware and LittleFS filesystem
- Upload both to ESP32
- Reset the device
- Launch serial monitor in optimal terminal
- Create automatic backups

### 2. Interactive Mode
```bash
./deploy.sh
```
Provides menu-driven deployment options.

## Deployment Scripts

### Main Deployment Script (`deploy.sh`)

**Full Deployment:**
```bash
./deploy.sh --all                    # Complete deployment + monitor
./deploy.sh --all --clean            # Clean build + full deployment
./deploy.sh --all --port /dev/ttyUSB1  # Specify custom port
```

**Selective Deployment:**
```bash
./deploy.sh --firmware              # Firmware only
./deploy.sh --filesystem            # LittleFS only  
./deploy.sh --firmware --monitor    # Firmware + monitor
./deploy.sh --reset --monitor       # Reset + monitor
```

**Backup Operations:**
```bash
./deploy.sh --backup                # Backup current firmware
```

### Helper Scripts

#### Quick Monitor (`scripts/quick-monitor.sh`)
```bash
./scripts/quick-monitor.sh                    # Default port
./scripts/quick-monitor.sh /dev/ttyUSB1       # Custom port
```

#### Build Only (`scripts/build-only.sh`)
```bash
./scripts/build-only.sh              # Build firmware + filesystem
./scripts/build-only.sh --clean      # Clean build
```

#### ESP32 Reset (`scripts/esp32-reset.sh`)
```bash
./scripts/esp32-reset.sh             # Reset with multiple methods
./scripts/esp32-reset.sh /dev/ttyUSB1  # Custom port
```

#### Development Tools (`scripts/development-tools.sh`)
```bash
./scripts/development-tools.sh deps  # Check dependencies
./scripts/development-tools.sh info  # Project information
./scripts/development-tools.sh clean # Clean artifacts
./scripts/development-tools.sh test  # Run tests
```

## Terminal Support

### Supported Terminals (Reliability Order)

1. **Konsole** (KDE) - Most reliable, excellent ESP32 support
2. **XFCE4 Terminal** - Very stable, good performance
3. **MATE Terminal** - Reliable, lightweight
4. **XTerm** - Universal fallback, always available
5. **Terminator** - Feature-rich but can be unstable
6. **GNOME Terminal** - Mixed results, listed last

### Terminal Installation
```bash
# Install recommended terminal (choose one)
sudo apt install konsole           # KDE (recommended)
sudo apt install xfce4-terminal    # XFCE  
sudo apt install mate-terminal     # MATE
sudo apt install xterm             # Universal fallback

# Install all for maximum compatibility
sudo apt install konsole xfce4-terminal mate-terminal xterm
```

### Terminal Features

Each terminal window includes:
- **ESP32 Exception Decoder** - Automatic stack trace decoding
- **Colorized Output** - Syntax highlighting for logs
- **Timestamps** - Precise timing information
- **Persistent Window** - Stays open after ESP32 disconnection

## File Structure

### Build Artifacts
```
.pio/build/esp32dev/
├── firmware.bin        # Main ESP32 firmware
├── littlefs.bin       # LittleFS filesystem image
├── partitions.bin     # Partition table
└── bootloader.bin     # ESP32 bootloader
```

### Data Files (LittleFS)
```
data/
├── system.json                    # System configuration
├── config/
│   └── components/               # Component configurations
│       ├── dht22-1.json         # DHT22 sensor config  
│       └── tsl2561-1.json       # TSL2561 sensor config
└── schemas/
    └── components/               # Component schemas
        ├── DHT22.json           # DHT22 schema definition
        └── TSL2561.json         # TSL2561 schema definition
```

### Logs and Backups
```
logs/
├── deploy_YYYYMMDD_HHMMSS.log   # Deployment logs
└── monitor_YYYYMMDD_HHMMSS.log  # Monitor session logs

backups/
├── firmware_backup_YYYYMMDD_HHMMSS.bin    # ESP32 firmware backups
└── firmware_build_YYYYMMDD_HHMMSS.bin     # Build artifact backups
```

## ESP32 Connection

### Port Detection
The scripts automatically detect ESP32 ports:
- `/dev/ttyUSB*` - USB-to-serial adapters
- `/dev/ttyACM*` - Native USB devices

### Manual Port Selection
```bash
# List available ports
ls /dev/tty* | grep -E "(USB|ACM)"

# Use specific port
./deploy.sh --all --port /dev/ttyUSB1
```

### Permission Issues
If you get permission denied errors:
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Logout and login again, or:
newgrp dialout

# Check permissions
ls -l /dev/ttyUSB0
```

## Configuration Management

### Default Behavior
1. **First Boot**: Components use hardcoded defaults
   - DHT22: Pin 15, 5-second interval, Celsius
   - TSL2561: I2C SDA=21/SCL=22, addr=0x39, 2-second interval

2. **Schema Creation**: Components auto-save their schemas to LittleFS
3. **Persistent Config**: Future boots load saved configurations
4. **Fallback**: Always fall back to defaults if saved config is invalid

### Configuration Files

#### System Configuration (`data/system.json`)
```json
{
  "version": "1.0.0",
  "deviceId": "esp32-orchestrator-1640995200",
  "name": "ESP32 IoT Orchestrator",
  "timezone": "UTC",
  "logging": {
    "level": "INFO",
    "destinations": ["serial"]
  }
}
```

#### Component Configuration Example
```json
{
  "pin": 15,
  "samplingIntervalMs": 5000,
  "fahrenheit": false,
  "sensorName": "Room Temperature"
}
```

## Monitoring and Debugging

### Serial Monitor Features
- **Real-time logging** with timestamps
- **ESP32 exception decoder** for crash analysis
- **Colorized output** for easy reading
- **Persistent terminals** that survive disconnections

### Expected Serial Output
```
=== ESP32 IoT Orchestrator Logger Initialized ===
Log level: INFO
================================================

[00:00:00.123][INFO][main] ESP32 IoT Orchestrator - Baseline v1.0.0
[00:00:00.234][INFO][Orchestrator] Initializing ESP32 IoT Orchestrator...
[00:00:00.345][INFO][ConfigStorage] ConfigStorage initialized successfully
[00:00:00.456][INFO][DHT22:dht22-1] DHT22 initialized on pin 15, interval=5000ms
[00:00:00.567][INFO][TSL2561:tsl2561-1] TSL2561 initialized - I2C addr=0x39, SDA=21, SCL=22
[00:00:00.678][INFO][Orchestrator] System started with 2 components
[00:00:05.123][INFO][DHT22:dht22-1] T=22.5°C, H=45.2%
[00:00:07.234][INFO][TSL2561:tsl2561-1] Lux=150.2, BB=1024, IR=256
```

### Debug Mode
Enable verbose logging by changing in `src/main.cpp`:
```cpp
Logger::init(Logger::DEBUG);  // Instead of Logger::INFO
```

## Troubleshooting

### Common Issues

#### Build Failures
```bash
# Check dependencies
./scripts/development-tools.sh deps

# Clean and rebuild
./deploy.sh --clean --all

# Check library compatibility
pio lib list
```

#### Upload Failures
```bash
# Check ESP32 connection
./scripts/development-tools.sh info

# Try different upload speed
# Edit platformio.ini: upload_speed = 115200

# Reset ESP32 manually before upload
./scripts/esp32-reset.sh
```

#### Monitor Issues
```bash
# Test different terminals
./scripts/quick-monitor.sh

# Check port permissions
ls -l /dev/ttyUSB0
sudo usermod -a -G dialout $USER

# Manual monitor
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

#### Component Initialization Failures
- **DHT22**: Check wiring, add pull-up resistor
- **TSL2561**: Verify I2C connections, check address
- **SPIFFS**: Check partition table, may need format

### Recovery Procedures

#### Restore from Backup
```bash
# List available backups
ls -la backups/

# Use esptool to restore
esptool.py --port /dev/ttyUSB0 --baud 921600 \
           write_flash 0x10000 backups/firmware_backup_YYYYMMDD_HHMMSS.bin
```

#### Factory Reset
```bash
# Erase flash completely
esptool.py --port /dev/ttyUSB0 erase_flash

# Redeploy everything
./deploy.sh --all
```

#### Format Configuration Storage
The system will auto-format SPIFFS if corruption is detected, or manually trigger via code modification.

## Development Workflow

### Typical Development Cycle
```bash
# 1. Make code changes
vim src/components/DHT22Component.cpp

# 2. Build and test locally
./scripts/build-only.sh

# 3. Deploy and monitor
./deploy.sh --firmware --monitor

# 4. Test functionality
# Monitor output in terminal window

# 5. Deploy filesystem if configs changed
./deploy.sh --filesystem --reset
```

### Rapid Development
```bash
# Quick firmware iteration
./deploy.sh --firmware --monitor

# Quick filesystem iteration  
./deploy.sh --filesystem --reset --monitor

# Reset and monitor only (no rebuild)
./deploy.sh --reset --monitor
```

## Advanced Features

### Custom Port Management
```bash
# Auto-detect and select from multiple ports
./deploy.sh --all

# Force specific port
./deploy.sh --all --port /dev/ttyACM0

# Use with different baud rates (edit ESP32_BAUDRATE in script)
```

### Terminal Customization
```bash
# Use specific terminal
./deploy.sh --monitor --terminal konsole
./deploy.sh --monitor --terminal xterm

# Launch multiple monitors (different terminals)
./scripts/quick-monitor.sh /dev/ttyUSB0 &
./scripts/quick-monitor.sh /dev/ttyUSB1 &
```

### Logging and Monitoring
- All operations logged to `logs/deploy_TIMESTAMP.log`
- Deployment reports generated automatically
- Monitor sessions can be saved independently

---

**Version**: 1.0.0  
**Compatibility**: Linux (Ubuntu, Debian, Fedora, Arch)  
**Requirements**: PlatformIO, Python3, Terminal Emulator