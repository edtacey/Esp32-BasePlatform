#!/bin/bash

# Build Only Script
# Compiles firmware and filesystem without uploading

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

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

build_firmware() {
    log "Building ESP32 firmware..."
    
    if pio run; then
        local binary_size=$(stat -c%s .pio/build/esp32dev/firmware.bin 2>/dev/null || echo "0")
        success "Firmware built successfully ($binary_size bytes)"
        return 0
    else
        error "Firmware build failed"
        return 1
    fi
}

build_filesystem() {
    log "Preparing LittleFS data..."
    
    # Create default configuration files
    mkdir -p data/config/components data/schemas/components
    
    cat > data/system.json << EOF
{
  "version": "1.0.0",
  "deviceId": "esp32-orchestrator-$(date +%s)",
  "name": "ESP32 IoT Orchestrator"
}
EOF
    
    log "Building LittleFS filesystem..."
    
    if pio run -t buildfs; then
        local fs_size=$(stat -c%s .pio/build/esp32dev/littlefs.bin 2>/dev/null || echo "0")
        success "LittleFS built successfully ($fs_size bytes)"
        return 0
    else
        error "LittleFS build failed"
        return 1
    fi
}

show_build_info() {
    echo ""
    echo -e "${BLUE}📊 Build Information${NC}"
    echo "===================="
    
    if [[ -f ".pio/build/esp32dev/firmware.bin" ]]; then
        local fw_size=$(stat -c%s .pio/build/esp32dev/firmware.bin)
        echo "Firmware: $fw_size bytes"
    else
        echo "Firmware: Not built"
    fi
    
    if [[ -f ".pio/build/esp32dev/littlefs.bin" ]]; then
        local fs_size=$(stat -c%s .pio/build/esp32dev/littlefs.bin)
        echo "Filesystem: $fs_size bytes"
    else
        echo "Filesystem: Not built"
    fi
    
    echo ""
    echo "Next steps:"
    echo "  ./deploy.sh --firmware    # Upload firmware"
    echo "  ./deploy.sh --filesystem  # Upload filesystem"
    echo "  ./deploy.sh --all         # Upload both + monitor"
    echo ""
}

main() {
    cd "$PROJECT_DIR"
    
    echo -e "${BLUE}🔨 ESP32 Build Only${NC}"
    echo ""
    
    # Parse arguments
    local clean=false
    if [[ "$1" == "--clean" ]] || [[ "$1" == "-c" ]]; then
        clean=true
    fi
    
    # Clean if requested
    if [[ "$clean" == "true" ]]; then
        log "Cleaning previous build..."
        pio run -t clean
        success "Build cleaned"
    fi
    
    # Build firmware
    if ! build_firmware; then
        exit 1
    fi
    
    # Build filesystem
    if ! build_filesystem; then
        exit 1
    fi
    
    success "All builds completed successfully"
    show_build_info
}

main "$@"