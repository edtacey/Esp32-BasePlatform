#!/bin/bash

# ESP32 Reset Script
# Provides multiple methods to reset ESP32 device

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuration
ESP32_PORT="${1:-/dev/ttyUSB0}"

log() {
    echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"
}

success() {
    echo -e "${GREEN}✅${NC} $1"
}

error() {
    echo -e "${RED}❌${NC} $1"
}

warning() {
    echo -e "${YELLOW}⚠️${NC} $1"
}

detect_esp32() {
    log "Detecting ESP32 on $ESP32_PORT..."
    
    if [[ ! -e "$ESP32_PORT" ]]; then
        error "ESP32 port not found: $ESP32_PORT"
        echo "Available ports:"
        ls /dev/tty* 2>/dev/null | grep -E "(USB|ACM)" | head -5 || echo "  None found"
        return 1
    fi
    
    # Test port accessibility
    if ! stty -F "$ESP32_PORT" speed 115200 &>/dev/null; then
        error "Cannot access ESP32 port: $ESP32_PORT"
        echo "Check permissions: sudo usermod -a -G dialout \$USER"
        return 1
    fi
    
    success "ESP32 detected on $ESP32_PORT"
    return 0
}

reset_via_esptool() {
    log "Resetting ESP32 via esptool..."
    
    if command -v esptool.py &> /dev/null; then
        if esptool.py --port "$ESP32_PORT" --baud 115200 run; then
            success "ESP32 reset via esptool"
            return 0
        else
            warning "esptool reset failed"
            return 1
        fi
    else
        warning "esptool.py not found"
        return 1
    fi
}

reset_via_platformio() {
    log "Resetting ESP32 via PlatformIO..."
    
    if command -v pio &> /dev/null; then
        # Use monitor command to trigger reset
        timeout 3 pio device monitor --port "$ESP32_PORT" --baud 115200 --quiet &>/dev/null || true
        success "ESP32 reset via PlatformIO"
        return 0
    else
        warning "PlatformIO not found"
        return 1
    fi
}

reset_via_dtr() {
    log "Resetting ESP32 via DTR signal..."
    
    # Use Python to toggle DTR line
    python3 << EOF
import serial
import time

try:
    ser = serial.Serial('$ESP32_PORT', 115200, timeout=1)
    ser.setDTR(False)
    time.sleep(0.1)
    ser.setDTR(True)
    time.sleep(0.1)
    ser.setDTR(False)
    ser.close()
    print("DTR reset completed")
except Exception as e:
    print(f"DTR reset failed: {e}")
    exit(1)
EOF
    
    if [[ $? -eq 0 ]]; then
        success "ESP32 reset via DTR signal"
        return 0
    else
        warning "DTR reset failed"
        return 1
    fi
}

show_reset_status() {
    log "Checking ESP32 status after reset..."
    
    # Brief monitor to check if ESP32 is responding
    timeout 5 pio device monitor --port "$ESP32_PORT" --baud 115200 --filter time 2>/dev/null | head -10 || true
    
    echo ""
    success "ESP32 reset completed"
    echo "Use './scripts/quick-monitor.sh' to view serial output"
}

main() {
    echo -e "${BLUE}🔄 ESP32 Reset Tool${NC}"
    echo ""
    
    # Detect ESP32
    if ! detect_esp32; then
        exit 1
    fi
    
    # Try multiple reset methods
    if reset_via_esptool; then
        show_reset_status
    elif reset_via_platformio; then
        show_reset_status
    elif reset_via_dtr; then
        show_reset_status
    else
        error "All reset methods failed"
        echo ""
        echo "Manual reset options:"
        echo "1. Press the RESET button on your ESP32 board"
        echo "2. Disconnect and reconnect USB cable"
        echo "3. Power cycle the ESP32"
        exit 1
    fi
}

main "$@"