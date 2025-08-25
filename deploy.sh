#!/bin/bash

# ESP32 IoT Orchestrator - Deployment Script
# Handles firmware compilation, LittleFS deployment, ESP32 reset, and monitoring
# Supports optional operations and native Linux terminal management

set -e  # Exit on any error

# Configuration
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_DIR="$PROJECT_DIR/logs"
BACKUP_DIR="$PROJECT_DIR/backups"

# ESP32 Configuration
ESP32_PORT="/dev/ttyUSB0"      # Default ESP32 port
ESP32_BAUDRATE="115200"        # Serial monitor baud rate
ESP32_UPLOAD_SPEED="921600"    # Upload speed

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Terminal preferences (ordered by reliability)
TERMINAL_PREFERENCE=(
    "konsole"           # KDE - most reliable
    "xfce4-terminal"    # XFCE - very stable  
    "mate-terminal"     # MATE - reliable
    "xterm"             # Universal fallback
    "terminator"        # Feature-rich but can be unstable
    "gnome-terminal"    # Listed last due to mixed results
)

# ============================================================================
# Utility Functions
# ============================================================================

# Logging functions (adapted from enhanced deploy)
log() {
    echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOG_DIR/deploy_$TIMESTAMP.log"
}

success() {
    echo -e "${GREEN}✅${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] SUCCESS: $1" >> "$LOG_DIR/deploy_$TIMESTAMP.log"
}

warning() {
    echo -e "${YELLOW}⚠️${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] WARNING: $1" >> "$LOG_DIR/deploy_$TIMESTAMP.log"
}

error() {
    echo -e "${RED}❌${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ERROR: $1" >> "$LOG_DIR/deploy_$TIMESTAMP.log"
}

info() {
    echo -e "${CYAN}ℹ️${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] INFO: $1" >> "$LOG_DIR/deploy_$TIMESTAMP.log"
}

header() {
    echo ""
    echo -e "${PURPLE}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${PURPLE}║${NC} $1"
    echo -e "${PURPLE}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# ============================================================================
# Terminal Management Functions  
# ============================================================================

detect_best_terminal() {
    for terminal in "${TERMINAL_PREFERENCE[@]}"; do
        if command -v "$terminal" &> /dev/null; then
            echo "$terminal"
            return 0
        fi
    done
    echo "none"
    return 1
}

launch_terminal() {
    local title="$1"
    local command="$2"
    local working_dir="$3"
    local terminal="$4"
    
    # Use project directory if no working directory specified
    if [[ -z "$working_dir" ]]; then
        working_dir="$PROJECT_DIR"
    fi
    
    # Auto-detect terminal if not specified
    if [[ -z "$terminal" ]]; then
        terminal=$(detect_best_terminal)
    fi
    
    case "$terminal" in
        "konsole")
            konsole --title "$title" --workdir "$working_dir" \
                   -e bash -c "$command; echo 'Press any key to close...'; read" &
            ;;
        "xfce4-terminal")
            xfce4-terminal --title="$title" --working-directory="$working_dir" \
                          --command="bash -c '$command; echo \"Press any key to close...\"; read'" &
            ;;
        "mate-terminal")
            mate-terminal --title="$title" --working-directory="$working_dir" \
                         --command="bash -c '$command; echo \"Press any key to close...\"; read'" &
            ;;
        "xterm")
            xterm -title "$title" -e "cd '$working_dir' && $command; echo 'Press any key to close...'; read" &
            ;;
        "terminator")
            terminator --title="$title" --working-directory="$working_dir" \
                      -x bash -c "$command; echo 'Press any key to close...'; read" &
            ;;
        "gnome-terminal")
            gnome-terminal --title="$title" --working-directory="$working_dir" \
                          -- bash -c "$command; echo 'Press any key to close...'; read" &
            ;;
        *)
            error "No suitable terminal found. Available terminals:"
            for term in "${TERMINAL_PREFERENCE[@]}"; do
                if command -v "$term" &> /dev/null; then
                    echo "  ✅ $term"
                else
                    echo "  ❌ $term (not installed)"
                fi
            done
            return 1
            ;;
    esac
    
    return 0
}

# ============================================================================
# ESP32 Detection and Management
# ============================================================================

detect_esp32_port() {
    local ports=()
    
    # Check common ESP32 ports
    for port in /dev/ttyUSB* /dev/ttyACM* /dev/cu.SLAB_USBtoUART* /dev/cu.wchusbserial*; do
        if [[ -e "$port" ]]; then
            ports+=("$port")
        fi
    done
    
    if [[ ${#ports[@]} -eq 0 ]]; then
        error "No ESP32 device found. Please check USB connection."
        return 1
    elif [[ ${#ports[@]} -eq 1 ]]; then
        ESP32_PORT="${ports[0]}"
        info "ESP32 found on port: $ESP32_PORT"
        return 0
    else
        warning "Multiple serial ports found:"
        for i in "${!ports[@]}"; do
            echo "  $((i+1)). ${ports[$i]}"
        done
        
        echo -n "Select port (1-${#ports[@]}): "
        read -r selection
        
        if [[ "$selection" =~ ^[0-9]+$ ]] && [[ "$selection" -ge 1 ]] && [[ "$selection" -le ${#ports[@]} ]]; then
            ESP32_PORT="${ports[$((selection-1))]}"
            info "Selected port: $ESP32_PORT"
            return 0
        else
            error "Invalid selection"
            return 1
        fi
    fi
}

reset_esp32() {
    log "Resetting ESP32..."
    
    if [[ ! -e "$ESP32_PORT" ]]; then
        error "ESP32 port not found: $ESP32_PORT"
        return 1
    fi
    
    # Use esptool to reset
    if command -v esptool.py &> /dev/null; then
        esptool.py --port "$ESP32_PORT" --baud 115200 run
        success "ESP32 reset via esptool"
    else
        # Alternative: Use PlatformIO
        "$PIO_CMD" device monitor --port "$ESP32_PORT" --baud "$ESP32_BAUDRATE" --echo --filter send_on_enter &
        local monitor_pid=$!
        sleep 1
        kill $monitor_pid 2>/dev/null || true
        success "ESP32 reset via monitor cycle"
    fi
    
    return 0
}

# ============================================================================
# Build and Deploy Functions
# ============================================================================

build_firmware() {
    header "BUILDING FIRMWARE"
    
    log "Cleaning previous build..."
    $PIO_CMD run -t clean
    
    log "Building ESP32 firmware..."
    if $PIO_CMD run; then
        success "Firmware build completed successfully"
        
        # Show build statistics
        local binary_size=$(stat -c%s .pio/build/esp32dev/firmware.bin 2>/dev/null || echo "unknown")
        info "Firmware size: $binary_size bytes"
        
        return 0
    else
        error "Firmware build failed"
        return 1
    fi
}

upload_firmware() {
    header "UPLOADING FIRMWARE"
    
    if [[ ! -f ".pio/build/esp32dev/firmware.bin" ]]; then
        error "Firmware binary not found. Run build first."
        return 1
    fi
    
    log "Uploading firmware to ESP32..."
    if $PIO_CMD run -t upload; then
        success "Firmware uploaded successfully"
        return 0
    else
        error "Firmware upload failed"
        return 1
    fi
}

build_filesystem() {
    header "BUILDING LITTLEFS FILESYSTEM"
    
    # Create data directory if it doesn't exist
    mkdir -p data
    
    # Create default configuration files
    log "Creating default configuration files..."
    
    # System configuration
    cat > data/system.json << EOF
{
  "version": "1.0.0",
  "deviceId": "esp32-orchestrator-$(date +%s)",
  "name": "ESP32 IoT Orchestrator",
  "timezone": "UTC",
  "logging": {
    "level": "INFO",
    "destinations": ["serial"]
  }
}
EOF
    
    # Component schemas directory (will be populated by firmware)
    mkdir -p data/schemas/components
    mkdir -p data/config/components
    
    info "LittleFS filesystem prepared"
    
    # Build filesystem
    log "Building LittleFS filesystem..."
    if $PIO_CMD run -t buildfs; then
        success "LittleFS filesystem built successfully"
        
        # Show filesystem statistics
        if [[ -f ".pio/build/esp32dev/littlefs.bin" ]]; then
            local fs_size=$(stat -c%s .pio/build/esp32dev/littlefs.bin 2>/dev/null || echo "unknown")
            info "Filesystem size: $fs_size bytes"
        fi
        
        return 0
    else
        error "LittleFS build failed"
        return 1
    fi
}

upload_filesystem() {
    header "UPLOADING LITTLEFS FILESYSTEM"
    
    if [[ ! -f ".pio/build/esp32dev/littlefs.bin" ]]; then
        error "LittleFS binary not found. Run buildfs first."
        return 1
    fi
    
    log "Uploading LittleFS filesystem to ESP32..."
    if $PIO_CMD run -t uploadfs; then
        success "LittleFS filesystem uploaded successfully"
        return 0
    else
        error "LittleFS upload failed"
        return 1
    fi
}

monitor_esp32() {
    local terminal="$1"
    local title="ESP32 Monitor - $(date '+%H:%M:%S')"
    
    # Try PlatformIO monitor first, fall back to simple serial monitor
    local monitor_cmd="'$PIO_CMD' device monitor --port '$ESP32_PORT' --baud '$ESP32_BAUDRATE' --filter esp32_exception_decoder --filter colorize --filter time"
    local fallback_cmd="stty -F '$ESP32_PORT' raw '$ESP32_BAUDRATE'; echo 'ESP32 Serial Monitor - Press Ctrl+C to exit'; cat '$ESP32_PORT'"
    
    log "Starting ESP32 monitor in new terminal..."
    info "Monitor will open in $terminal terminal"
    info "Port: $ESP32_PORT, Baud: $ESP32_BAUDRATE"
    
    # Try PlatformIO monitor first
    if launch_terminal "$title" "$monitor_cmd" "$PROJECT_DIR" "$terminal"; then
        success "ESP32 monitor launched successfully (PlatformIO)"
        return 0
    else
        warning "PlatformIO monitor failed, trying fallback serial monitor..."
        if launch_terminal "$title - Serial" "$fallback_cmd" "$PROJECT_DIR" "$terminal"; then
            success "ESP32 monitor launched successfully (fallback)"
            return 0
        else
            error "Failed to launch ESP32 monitor"
            return 1
        fi
    fi
}

# ============================================================================
# Backup and Recovery Functions
# ============================================================================

backup_current_firmware() {
    header "BACKING UP CURRENT FIRMWARE"
    
    # Create backup directory
    mkdir -p "$BACKUP_DIR"
    
    local backup_file="$BACKUP_DIR/firmware_backup_$TIMESTAMP.bin"
    
    log "Reading current firmware from ESP32..."
    if esptool.py --port "$ESP32_PORT" --baud 921600 read_flash 0x10000 0x140000 "$backup_file"; then
        success "Firmware backup saved: $backup_file"
        
        # Also backup the build artifacts if they exist
        if [[ -f ".pio/build/esp32dev/firmware.bin" ]]; then
            cp ".pio/build/esp32dev/firmware.bin" "$BACKUP_DIR/firmware_build_$TIMESTAMP.bin"
            info "Build artifacts backed up"
        fi
        
        return 0
    else
        warning "Firmware backup failed (continuing anyway)"
        return 1
    fi
}

create_deployment_report() {
    local report_file="$LOG_DIR/deployment_report_$TIMESTAMP.txt"
    
    cat > "$report_file" << EOF
ESP32 IoT Orchestrator - Deployment Report
==========================================
Date: $(date)
Project: $PROJECT_DIR
ESP32 Port: $ESP32_PORT

Build Information:
EOF
    
    if [[ -f ".pio/build/esp32dev/firmware.bin" ]]; then
        echo "  Firmware Size: $(stat -c%s .pio/build/esp32dev/firmware.bin) bytes" >> "$report_file"
    fi
    
    if [[ -f ".pio/build/esp32dev/littlefs.bin" ]]; then
        echo "  Filesystem Size: $(stat -c%s .pio/build/esp32dev/littlefs.bin) bytes" >> "$report_file"
    fi
    
    echo "" >> "$report_file"
    echo "Deployment Steps Completed:" >> "$report_file"
    echo "$(cat "$LOG_DIR/deploy_$TIMESTAMP.log")" >> "$report_file"
    
    info "Deployment report saved: $report_file"
}

# ============================================================================
# Main Functions
# ============================================================================

show_usage() {
    echo ""
    echo -e "${PURPLE}ESP32 IoT Orchestrator - Deployment Tool${NC}"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "OPTIONS:"
    echo "  -f, --firmware          Build and upload firmware only"
    echo "  -s, --filesystem        Build and upload LittleFS only"
    echo "  -m, --monitor           Launch ESP32 monitor only"
    echo "  -r, --reset             Reset ESP32 only"
    echo "  -b, --backup            Backup current firmware"
    echo "  -a, --all               Full deployment (firmware + filesystem + monitor)"
    echo "  -c, --clean             Clean build first"
    echo "  -p, --port <port>       Specify ESP32 port (default: $ESP32_PORT)"
    echo "  -t, --terminal <term>   Specify terminal for monitor"
    echo "  -h, --help              Show this help"
    echo ""
    echo "EXAMPLES:"
    echo "  $0 --all                      # Full deployment with monitoring"
    echo "  $0 --firmware --monitor       # Build/upload firmware and monitor"
    echo "  $0 --filesystem --reset       # Upload filesystem and reset"
    echo "  $0 --monitor --terminal xterm # Monitor with specific terminal"
    echo "  $0 --clean --all              # Clean build + full deployment"
    echo ""
    echo "TERMINAL PREFERENCE (best to worst):"
    for terminal in "${TERMINAL_PREFERENCE[@]}"; do
        if command -v "$terminal" &> /dev/null; then
            echo -e "  ${GREEN}✅ $terminal${NC}"
        else
            echo -e "  ${RED}❌ $terminal${NC} (not installed)"
        fi
    done
    echo ""
}

initialize_environment() {
    header "INITIALIZING DEPLOYMENT ENVIRONMENT"
    
    # Create necessary directories
    mkdir -p "$LOG_DIR" "$BACKUP_DIR" "data/config/components" "data/schemas/components"
    
    # Detect ESP32
    if ! detect_esp32_port; then
        error "ESP32 detection failed"
        exit 1
    fi
    
    # Detect terminal
    local terminal=$(detect_best_terminal)
    if [[ "$terminal" == "none" ]]; then
        warning "No suitable terminal found for monitoring"
        warning "Install one of: ${TERMINAL_PREFERENCE[*]}"
    else
        info "Will use $terminal for monitoring"
    fi
    
    # Check PlatformIO
    if [ -f "/home/edt/.platformio/penv/bin/pio" ]; then
        PIO_CMD="/home/edt/.platformio/penv/bin/pio"
    elif command -v pio &> /dev/null; then
        PIO_CMD="pio"
    else
        error "PlatformIO not found. Please install PlatformIO."
        exit 1
    fi
    
    success "Environment initialized"
}

# ============================================================================
# Main Deployment Logic
# ============================================================================

deploy_firmware() {
    if ! build_firmware; then
        return 1
    fi
    
    if ! upload_firmware; then
        return 1
    fi
    
    return 0
}

deploy_filesystem() {
    if ! build_filesystem; then
        return 1
    fi
    
    if ! upload_filesystem; then
        return 1
    fi
    
    return 0
}

full_deployment() {
    header "FULL ESP32 DEPLOYMENT"
    
    local start_time=$(date +%s)
    
    # Optional backup
    if [[ "$BACKUP_ENABLED" == "true" ]]; then
        backup_current_firmware
    fi
    
    # Build and upload firmware
    if ! deploy_firmware; then
        error "Firmware deployment failed"
        return 1
    fi
    
    # Build and upload filesystem
    if ! deploy_filesystem; then
        error "Filesystem deployment failed"
        return 1
    fi
    
    # Reset ESP32
    reset_esp32
    
    # Wait a moment for ESP32 to boot
    sleep 2
    
    # Launch monitor if requested
    if [[ "$MONITOR_ENABLED" == "true" ]]; then
        local terminal=$(detect_best_terminal)
        monitor_esp32 "$terminal"
    fi
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    success "Full deployment completed in ${duration} seconds"
    create_deployment_report
    
    return 0
}

# ============================================================================
# Main Script Logic
# ============================================================================

main() {
    # Default options
    local firmware_only=false
    local filesystem_only=false
    local monitor_only=false
    local reset_only=false
    local backup_only=false
    local full_deploy=false
    local clean_build=false
    local custom_terminal=""
    
    BACKUP_ENABLED=false
    MONITOR_ENABLED=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -f|--firmware)
                firmware_only=true
                shift
                ;;
            -s|--filesystem)
                filesystem_only=true
                shift
                ;;
            -m|--monitor)
                monitor_only=true
                MONITOR_ENABLED=true
                shift
                ;;
            -r|--reset)
                reset_only=true
                shift
                ;;
            -b|--backup)
                backup_only=true
                BACKUP_ENABLED=true
                shift
                ;;
            -a|--all)
                full_deploy=true
                MONITOR_ENABLED=true
                BACKUP_ENABLED=true
                shift
                ;;
            -c|--clean)
                clean_build=true
                shift
                ;;
            -p|--port)
                ESP32_PORT="$2"
                shift 2
                ;;
            -t|--terminal)
                custom_terminal="$2"
                shift 2
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Show banner
    header "ESP32 IoT Orchestrator - Deployment Tool v1.0.0"
    
    # Initialize environment
    initialize_environment
    
    # Handle clean build
    if [[ "$clean_build" == "true" ]]; then
        log "Cleaning previous build..."
        $PIO_CMD run -t clean
        success "Build cleaned"
    fi
    
    # Execute requested operations
    if [[ "$backup_only" == "true" ]]; then
        backup_current_firmware
    elif [[ "$firmware_only" == "true" ]]; then
        deploy_firmware
        if [[ "$MONITOR_ENABLED" == "true" ]]; then
            reset_esp32
            sleep 2
            monitor_esp32 "${custom_terminal:-$(detect_best_terminal)}"
        fi
    elif [[ "$filesystem_only" == "true" ]]; then
        deploy_filesystem
        if [[ "$MONITOR_ENABLED" == "true" ]]; then
            reset_esp32
            sleep 2
            monitor_esp32 "${custom_terminal:-$(detect_best_terminal)}"
        fi
    elif [[ "$monitor_only" == "true" ]]; then
        monitor_esp32 "${custom_terminal:-$(detect_best_terminal)}"
    elif [[ "$reset_only" == "true" ]]; then
        reset_esp32
    elif [[ "$full_deploy" == "true" ]]; then
        full_deployment
    else
        # Interactive mode
        header "INTERACTIVE DEPLOYMENT MODE"
        echo "What would you like to do?"
        echo ""
        echo "1. Full deployment (firmware + filesystem + monitor)"
        echo "2. Build and upload firmware only"
        echo "3. Build and upload filesystem only"
        echo "4. Monitor ESP32 only"
        echo "5. Reset ESP32 only"
        echo "6. Backup current firmware"
        echo "7. Exit"
        echo ""
        echo -n "Select option (1-7): "
        read -r choice
        
        case $choice in
            1)
                MONITOR_ENABLED=true
                BACKUP_ENABLED=true
                full_deployment
                ;;
            2)
                deploy_firmware
                ;;
            3)
                deploy_filesystem
                ;;
            4)
                monitor_esp32 "${custom_terminal:-$(detect_best_terminal)}"
                ;;
            5)
                reset_esp32
                ;;
            6)
                backup_current_firmware
                ;;
            7)
                info "Exiting..."
                exit 0
                ;;
            *)
                error "Invalid choice"
                exit 1
                ;;
        esac
    fi
    
    # Final status
    if [[ $? -eq 0 ]]; then
        success "Deployment operation completed successfully"
        info "Check logs in: $LOG_DIR/deploy_$TIMESTAMP.log"
    else
        error "Deployment operation failed"
        info "Check logs in: $LOG_DIR/deploy_$TIMESTAMP.log"
        exit 1
    fi
}

# ============================================================================
# Script Entry Point
# ============================================================================

# Trap signals for cleanup
trap 'error "Script interrupted"; exit 1' INT TERM

# Change to project directory
cd "$PROJECT_DIR"

# Run main function
main "$@"