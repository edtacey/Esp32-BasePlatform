#!/bin/bash

# Enhanced ESP32 Deploy Script with Backup/Restore and Monitor Options
# Usage: ./enhanced-deploy.sh [OPTIONS]
#   -r, --reset      Reset device only (no build/upload)
#   -f, --fs-only    Upload filesystem only (no firmware build/upload)
#   -s, --serial     Show serial monitor (works with reset or deploy)  
#   -m, --monitor    Monitor only (no deploy, no reset)
#   -b, --backup     Backup components before filesystem upload
#   -h, --help       Show this help message

set -e
PROJECT_DIR="/home/edt/ccode/esp32/BaseLine-B"
ESP32_PORT="/dev/ttyUSB0"
LOGS_DIR="$PROJECT_DIR/runtime-logs"
ENV_FILE="$PROJECT_DIR/.env"

# Create logs directory if it doesn't exist
mkdir -p "$LOGS_DIR"

# Load or create .env file for ESP32 IP management
load_env_file() {
    if [ -f "$ENV_FILE" ]; then
        source "$ENV_FILE"
        log_message "Loaded ESP32_IP from .env: ${ESP32_IP:-'not set'}"
    else
        log_message "Creating new .env file"
        echo "# ESP32 Configuration" > "$ENV_FILE"
        echo "ESP32_IP=" >> "$ENV_FILE"
    fi
}

# Update ESP32 IP in .env file
update_esp32_ip() {
    local new_ip="$1"
    if [ ! -z "$new_ip" ]; then
        if grep -q "^ESP32_IP=" "$ENV_FILE"; then
            sed -i "s/^ESP32_IP=.*/ESP32_IP=$new_ip/" "$ENV_FILE"
        else
            echo "ESP32_IP=$new_ip" >> "$ENV_FILE"
        fi
        ESP32_IP="$new_ip"
        log_message "Updated ESP32_IP in .env: $new_ip"
    fi
}

# Test if ESP32 IP is live via ping
test_esp32_connectivity() {
    local ip="$1"
    if [ -z "$ip" ]; then
        return 1
    fi
    
    log_message "Testing connectivity to ESP32 at $ip"
    if ping -c 1 -W 2 "$ip" > /dev/null 2>&1; then
        log_message "✅ ESP32 at $ip is responding to ping"
        return 0
    else
        log_message "❌ ESP32 at $ip is not responding to ping"
        return 1
    fi
}

# Generate log filename with timestamp
LOG_TIMESTAMP=$(date '+%Y%m%d_%H%M%S')
SESSION_LOG="$LOGS_DIR/esp32_session_${LOG_TIMESTAMP}.log"

# Default options
RESET_ONLY=false
FS_ONLY=false
SHOW_SERIAL=false
MONITOR_ONLY=false
DEPLOY=true
BACKUP_COMPONENTS=false

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Logging function
log_message() {
    local message="$1"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] $message" >> "$SESSION_LOG"
    echo -e "$message"
}

# Initialize session log
log_message "=== ESP32 Enhanced Deploy Session Started ==="
log_message "Session ID: $LOG_TIMESTAMP"
log_message "Project Directory: $PROJECT_DIR"
log_message "ESP32 Port: $ESP32_PORT"
log_message "Log File: $SESSION_LOG"

# Load ESP32 IP configuration
load_env_file

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--reset)
            RESET_ONLY=true
            DEPLOY=false
            shift
            ;;
        -f|--fs-only)
            FS_ONLY=true
            DEPLOY=false
            shift
            ;;
        -s|--serial)
            SHOW_SERIAL=true
            shift
            ;;
        -m|--monitor)
            MONITOR_ONLY=true
            SHOW_SERIAL=true
            DEPLOY=false
            shift
            ;;
        -b|--backup)
            BACKUP_COMPONENTS=true
            shift
            ;;
        -h|--help)
            echo -e "${BLUE}Enhanced ESP32 Deploy Script${NC}"
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -r, --reset      Reset device only (no build/upload)"
            echo "  -f, --fs-only    Upload filesystem only (no firmware build/upload)"
            echo "  -s, --serial     Show serial monitor after operation"
            echo "  -m, --monitor    Monitor only (no deploy, no reset)"
            echo "  -b, --backup     Backup components before filesystem upload"
            echo "  -h, --help       Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0               # Full deploy with upload"
            echo "  $0 -r            # Reset device only"
            echo "  $0 -f            # Upload web files only (no firmware)"
            echo "  $0 -f -s         # Upload web files and show serial monitor"
            echo "  $0 -r -s         # Reset device and show serial monitor"
            echo "  $0 -s            # Deploy and show serial monitor"
            echo "  $0 -m            # Just monitor (no changes to device)"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}==========================================="
echo "     Enhanced ESP32 Deploy Script"
echo "===========================================${NC}"

cd "$PROJECT_DIR"
export PATH="/home/edt/.platformio/penv/bin:$PATH"

# Comprehensive port cleanup function
# Component backup and restore functions
backup_components_from_esp32() {
    local esp32_ip="$1"
    local backup_dir="$PROJECT_DIR/backups"
    local backup_file="$backup_dir/components_backup_${LOG_TIMESTAMP}.json"
    
    if [ -z "$esp32_ip" ]; then
        echo -e "${YELLOW}⚠️  No ESP32 IP provided, attempting to discover...${NC}"
        esp32_ip=$(discover_esp32_ip)
        if [ -z "$esp32_ip" ]; then
            echo -e "${RED}❌ Cannot backup components - ESP32 IP not found${NC}"
            return 1
        fi
    fi
    
    echo -e "${BLUE}💾 Backing up components from ESP32 at $esp32_ip...${NC}"
    
    # Create backup directory if it doesn't exist
    mkdir -p "$backup_dir"
    
    # Download component backup from ESP32
    if curl -s --connect-timeout 10 --max-time 30 \
        "http://$esp32_ip/api/components/backup" \
        -o "$backup_file"; then
        
        # Validate backup file
        if [ -s "$backup_file" ] && python3 -m json.tool "$backup_file" >/dev/null 2>&1; then
            local component_count=$(python3 -c "
import json
try:
    with open('$backup_file', 'r') as f:
        data = json.load(f)
        print(data.get('component_count', 0))
except:
    print(0)
")
            echo -e "${GREEN}✅ Component backup successful: $component_count components saved to $backup_file${NC}"
            
            # Copy backup to runtime directory for automatic restore
            local runtime_backup="$PROJECT_DIR/data/runtime/components.json"
            cp "$backup_file" "$runtime_backup"
            echo -e "${GREEN}✅ Backup copied to runtime directory for automatic restore${NC}"
            
            log_message "Component backup successful: $backup_file ($component_count components)"
            return 0
        else
            echo -e "${RED}❌ Component backup failed - invalid response from ESP32${NC}"
            rm -f "$backup_file"
            return 1
        fi
    else
        echo -e "${RED}❌ Component backup failed - could not connect to ESP32 at $esp32_ip${NC}"
        return 1
    fi
}

restore_components_to_esp32() {
    local esp32_ip="$1"
    local backup_file="$2"
    
    if [ -z "$esp32_ip" ]; then
        echo -e "${YELLOW}⚠️  No ESP32 IP provided, attempting to discover...${NC}"
        esp32_ip=$(discover_esp32_ip)
        if [ -z "$esp32_ip" ]; then
            echo -e "${RED}❌ Cannot restore components - ESP32 IP not found${NC}"
            return 1
        fi
    fi
    
    if [ ! -f "$backup_file" ]; then
        echo -e "${RED}❌ Backup file not found: $backup_file${NC}"
        return 1
    fi
    
    echo -e "${BLUE}🔄 Restoring components to ESP32 at $esp32_ip...${NC}"
    
    # Upload backup data to ESP32 restore endpoint
    if curl -s --connect-timeout 10 --max-time 30 \
        -H "Content-Type: application/json" \
        -X POST \
        --data-binary "@$backup_file" \
        "http://$esp32_ip/api/components/restore" \
        -o /tmp/restore_response_${LOG_TIMESTAMP}.json; then
        
        # Check restore response
        local success=$(python3 -c "
import json
try:
    with open('/tmp/restore_response_${LOG_TIMESTAMP}.json', 'r') as f:
        data = json.load(f)
        if data.get('success'):
            print('true')
            print('Restored:', data.get('restored_count', 0))
            print('Failed:', data.get('failed_count', 0))
        else:
            print('false')
            print('Error:', data.get('message', 'Unknown error'))
except Exception as e:
    print('false')
    print('Parse error:', e)
")
        
        if echo "$success" | head -1 | grep -q "true"; then
            echo -e "${GREEN}✅ Component restore successful${NC}"
            echo "$success" | tail -n +2
            log_message "Component restore successful from $backup_file"
            rm -f /tmp/restore_response_${LOG_TIMESTAMP}.json
            return 0
        else
            echo -e "${RED}❌ Component restore failed${NC}"
            echo "$success" | tail -n +2
            return 1
        fi
    else
        echo -e "${RED}❌ Component restore failed - could not connect to ESP32 at $esp32_ip${NC}"
        return 1
    fi
}

discover_esp32_ip() {
    # Try to discover ESP32 IP from serial logs or network scan
    local recent_log=$(find "$LOGS_DIR" -name "esp32_serial_*.log" -type f -mmin -10 | head -1)
    
    if [ -f "$recent_log" ]; then
        local ip=$(grep -oE "IP address: ([0-9]{1,3}\.){3}[0-9]{1,3}" "$recent_log" | tail -1 | cut -d' ' -f3)
        if [ ! -z "$ip" ]; then
            echo "$ip"
            return 0
        fi
    fi
    
    # Fallback to common IP ranges (basic network scan)
    for ip in 192.168.1.{150..160} 192.168.0.{150..160}; do
        if timeout 2 curl -s "http://$ip/api/status" >/dev/null 2>&1; then
            echo "$ip"
            return 0
        fi
    done
    
    return 1
}

cleanup_port_completely() {
    echo -e "${BLUE}🔌 Performing comprehensive port cleanup...${NC}"
    
    # Kill all processes using the ESP32 port
    pkill -f "$ESP32_PORT" 2>/dev/null || true
    pkill -f "python3.*serial" 2>/dev/null || true
    pkill -f "python3.*monitor" 2>/dev/null || true
    pkill -f "platformio.*monitor" 2>/dev/null || true
    pkill -f "esptool" 2>/dev/null || true
    
    # Kill by port name more aggressively
    lsof "$ESP32_PORT" 2>/dev/null | awk 'NR>1 {print $2}' | xargs -r kill -9 2>/dev/null || true
    
    # Wait for processes to fully terminate
    sleep 2
    
    # Verify port is available
    if lsof "$ESP32_PORT" >/dev/null 2>&1; then
        echo -e "${YELLOW}⚠️  Port still in use, forcing cleanup...${NC}"
        lsof "$ESP32_PORT" 2>/dev/null | awk 'NR>1 {print $2}' | xargs -r kill -9 2>/dev/null || true
        sleep 1
    fi
    
    echo -e "${GREEN}✅ Port cleanup completed${NC}"
}

# Check if ESP32 is connected
if [ ! -e "$ESP32_PORT" ]; then
    echo -e "${RED}❌ ESP32 not found at $ESP32_PORT${NC}"
    echo "Make sure ESP32 is connected and USB device is attached to WSL"
    exit 1
fi

# Always perform port cleanup before any operations
cleanup_port_completely

if [ "$MONITOR_ONLY" = true ]; then
    echo -e "${BLUE}🖥️  Monitor-only mode - No device changes${NC}"
    # Skip to monitoring section
elif [ "$FS_ONLY" = true ]; then
    echo -e "${BLUE}📁 Filesystem-only mode - Uploading web interface files...${NC}"
    
    # Backup components before filesystem upload if requested
    if [ "$BACKUP_COMPONENTS" = true ]; then
        echo -e "${BLUE}💾 Backing up components before filesystem upload...${NC}"
        ESP32_IP=$(discover_esp32_ip)
        if [ ! -z "$ESP32_IP" ]; then
            if backup_components_from_esp32 "$ESP32_IP"; then
                echo -e "${GREEN}✅ Component backup completed before filesystem upload${NC}"
            else
                echo -e "${YELLOW}⚠️  Component backup failed, continuing with filesystem upload...${NC}"
            fi
        else
            echo -e "${YELLOW}⚠️  Could not discover ESP32 IP for backup, continuing with filesystem upload...${NC}"
        fi
    fi
    
    # Ensure port is completely free before filesystem upload
    cleanup_port_completely
    
    # Additional wait for port stability
    echo -e "${BLUE}⏳ Waiting for port to stabilize...${NC}"
    sleep 3
    
    # Upload LittleFS filesystem (web interface files) only
    if ! platformio run --environment esp32dev --target uploadfs --upload-port "$ESP32_PORT"; then
        echo -e "${RED}❌ LittleFS upload failed!${NC}"
        echo -e "${YELLOW}💡 Trying cleanup and retry...${NC}"
        cleanup_port_completely
        sleep 5
        
        # Retry once after cleanup
        echo -e "${BLUE}🔄 Retrying LittleFS upload...${NC}"
        if ! platformio run --environment esp32dev --target uploadfs --upload-port "$ESP32_PORT"; then
            echo -e "${RED}❌ LittleFS upload failed after retry!${NC}"
            exit 1
        fi
    fi
    echo -e "${GREEN}✅ Web interface files uploaded successfully!${NC}"
elif [ "$RESET_ONLY" = true ]; then
    echo -e "${YELLOW}🔄 Resetting ESP32 device...${NC}"
    
    # Multiple reset methods for better compatibility
    python3 -c "
import serial
import time
import subprocess

def reset_method_1():
    \"\"\"Hardware reset using DTR/RTS signals\"\"\"
    try:
        ser = serial.Serial('$ESP32_PORT', 115200, timeout=1)
        print('Method 1: Hardware reset via DTR/RTS...')
        ser.setDTR(False)
        ser.setRTS(True)
        time.sleep(0.1)
        ser.setRTS(False)
        time.sleep(0.1)
        ser.setDTR(True)
        ser.close()
        return True
    except Exception as e:
        print(f'Method 1 failed: {e}')
        return False

def reset_method_2():
    \"\"\"Reset using esptool\"\"\"
    try:
        print('Method 2: Reset via esptool...')
        result = subprocess.run([
            'python3', '-m', 'esptool', 
            '--port', '$ESP32_PORT', 
            '--chip', 'esp32', 
            '--before', 'default_reset',
            'chip_id'
        ], capture_output=True, timeout=10)
        return result.returncode == 0
    except Exception as e:
        print(f'Method 2 failed: {e}')
        return False

def reset_method_3():
    \"\"\"Reset using platformio\"\"\"
    try:
        print('Method 3: Reset via platformio...')
        result = subprocess.run([
            'platformio', 'device', 'monitor', 
            '--port', '$ESP32_PORT',
            '--baud', '115200',
            '--rts', '0',
            '--dtr', '0'
        ], capture_output=True, timeout=5)
        return True
    except Exception as e:
        print(f'Method 3 failed: {e}')
        return False

# Try reset methods in order
success = False
for method in [reset_method_1, reset_method_2]:
    if method():
        success = True
        print('✅ Reset successful!')
        break
    time.sleep(0.5)

if not success:
    print('❌ All reset methods failed')
    exit(1)
"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ ESP32 reset successful!${NC}"
    else
        echo -e "${RED}❌ ESP32 reset failed!${NC}"
        exit 1
    fi
else
    # Build and upload firmware (dev environment only)
    echo -e "${BLUE}🔧 Building and uploading firmware...${NC}"
    
    # Test ESP32 connectivity and backup if live
    if [ ! -z "$ESP32_IP" ]; then
        if test_esp32_connectivity "$ESP32_IP"; then
            echo -e "${BLUE}💾 ESP32 is live, backing up components before deployment...${NC}"
            if backup_components_from_esp32 "$ESP32_IP"; then
                echo -e "${GREEN}✅ Component backup completed before deployment${NC}"
            else
                echo -e "${YELLOW}⚠️  Component backup failed, continuing with deployment...${NC}"
            fi
        else
            echo -e "${YELLOW}⚠️  ESP32 at $ESP32_IP not responding, skipping backup${NC}"
        fi
    else
        echo -e "${YELLOW}⚠️  No ESP32 IP configured, skipping backup${NC}"
    fi
    
    # Ensure port is completely free before upload
    cleanup_port_completely
    
    # Additional wait to ensure port is stable
    echo -e "${BLUE}⏳ Waiting for port to stabilize...${NC}"
    sleep 3
    
    if ! platformio run --environment esp32dev --target upload --upload-port "$ESP32_PORT"; then
        echo -e "${RED}❌ Firmware upload failed!${NC}"
        echo -e "${YELLOW}💡 Port may still be in use. Trying additional cleanup...${NC}"
        cleanup_port_completely
        sleep 5
        
        # Retry once after cleanup
        echo -e "${BLUE}🔄 Retrying firmware upload...${NC}"
        if ! platformio run --environment esp32dev --target upload --upload-port "$ESP32_PORT"; then
            echo -e "${RED}❌ Firmware upload failed after retry!${NC}"
            exit 1
        fi
    fi
    echo -e "${GREEN}✅ Firmware upload successful!${NC}"
    
    # Upload LittleFS filesystem (web interface files)
    echo -e "${BLUE}📁 Uploading web interface files (LittleFS)...${NC}"
    
    # Ensure port is completely free before filesystem upload
    cleanup_port_completely
    
    # Additional wait between uploads
    echo -e "${BLUE}⏳ Waiting before filesystem upload...${NC}"
    sleep 3
    
    if ! platformio run --environment esp32dev --target uploadfs --upload-port "$ESP32_PORT"; then
        echo -e "${YELLOW}⚠️  LittleFS upload failed - web interface may not work${NC}"
        echo -e "${YELLOW}    The API will still be available at /api/ endpoints${NC}"
        echo -e "${YELLOW}💡 Trying cleanup and retry...${NC}"
        cleanup_port_completely
        sleep 5
        
        # Retry once after cleanup
        echo -e "${BLUE}🔄 Retrying LittleFS upload...${NC}"
        if ! platformio run --environment esp32dev --target uploadfs --upload-port "$ESP32_PORT"; then
            echo -e "${YELLOW}⚠️  LittleFS upload failed after retry - continuing without web interface${NC}"
        else
            echo -e "${GREEN}✅ Web interface files uploaded successfully on retry!${NC}"
        fi
    else
        echo -e "${GREEN}✅ Web interface files uploaded successfully!${NC}"
    fi
fi

# Start serial monitoring AFTER uploads are complete
start_background_serial_monitor() {
    if [ "$SHOW_SERIAL" = true ] || [ "$1" = "force" ]; then
        echo -e "${BLUE}📺 Starting background serial monitor...${NC}"
        
        # Ensure all uploads are complete and port is free
        echo -e "${BLUE}⏳ Waiting for ESP32 to reset and stabilize...${NC}"
        sleep 5
        
        # Kill any existing serial monitors
        cleanup_port_completely
        
        # Create dedicated serial monitor script
        MONITOR_SCRIPT="/tmp/esp32_monitor_${LOG_TIMESTAMP}.py"
        cat > "$MONITOR_SCRIPT" << 'EOF'
import serial
import sys
import time
import datetime
import signal
import os

# Global flag for clean shutdown
running = True

def signal_handler(signum, frame):
    global running
    running = False
    print('\n[SERIAL] Monitor stopped')
    sys.exit(0)

signal.signal(signal.SIGTERM, signal_handler)
signal.signal(signal.SIGINT, signal_handler)

def monitor_serial(port):
    global running
    try:
        print(f'[SERIAL] === ESP32 Serial Monitor Started ===')
        print(f'[SERIAL] Port: {port} | Baud: 115200')
        print(f'[SERIAL] Time: {datetime.datetime.now().strftime("%H:%M:%S")}')
        print('[SERIAL] Press Ctrl+C to stop')
        print('[SERIAL] ==========================================')
        
        ser = serial.Serial(port, 115200, timeout=0.5)
        
        while running:
            try:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
                        
                        # Color code important messages
                        if any(word in line.lower() for word in ['error', 'exception', 'failed']):
                            print(f'\033[1;31m[{timestamp}] {line}\033[0m')
                        elif any(word in line.lower() for word in ['ip address', 'connected', 'wifi']):
                            print(f'\033[1;33m[{timestamp}] {line}\033[0m')  
                        elif any(word in line.lower() for word in ['setup', 'starting', 'ready']):
                            print(f'\033[1;32m[{timestamp}] {line}\033[0m')
                        else:
                            print(f'[{timestamp}] {line}')
                        
                        # Flush output immediately
                        sys.stdout.flush()
                            
                time.sleep(0.01)  # Small delay to prevent CPU spinning
                
            except serial.SerialException:
                if running:
                    time.sleep(0.1)
                continue
                
    except Exception as e:
        print(f'[SERIAL] Error: {e}')

if __name__ == '__main__':
    monitor_serial(sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyUSB0')
EOF

        # WSL-specific terminal options
        if command -v wt.exe >/dev/null 2>&1; then
            # Windows Terminal (primary WSL option)
            echo -e "${BLUE}🪟 Opening Windows Terminal tab...${NC}"
            wt.exe new-tab --title "ESP32 Serial Monitor" -- wsl python3 "$MONITOR_SCRIPT" "$ESP32_PORT" &
            SERIAL_MONITOR_PID=$!
            echo -e "${GREEN}✅ Serial monitor opened in new Windows Terminal tab${NC}"
        elif command -v cmd.exe >/dev/null 2>&1; then
            # Fallback to Windows Command Prompt
            echo -e "${BLUE}💻 Opening Windows Command Prompt...${NC}"
            cmd.exe /c start "ESP32 Serial Monitor" cmd /k "wsl python3 $MONITOR_SCRIPT $ESP32_PORT" &
            SERIAL_MONITOR_PID=$!
            echo -e "${GREEN}✅ Serial monitor opened in new Command Prompt window${NC}"
        elif command -v powershell.exe >/dev/null 2>&1; then
            # PowerShell option
            echo -e "${BLUE}⚡ Opening PowerShell window...${NC}"
            powershell.exe -Command "Start-Process powershell -ArgumentList '-NoExit', '-Command', 'wsl python3 $MONITOR_SCRIPT $ESP32_PORT' -WindowStyle Normal" &
            SERIAL_MONITOR_PID=$!
            echo -e "${GREEN}✅ Serial monitor opened in new PowerShell window${NC}"
        else
            # In-terminal fallback with clear visual separation
            echo -e "${YELLOW}⚠️  No separate window option available in WSL${NC}"
            echo -e "${BLUE}📺 Starting serial monitor in current terminal...${NC}"
            echo -e "${YELLOW}    Press Ctrl+C to stop monitoring and return to script${NC}"
            echo ""
            python3 "$MONITOR_SCRIPT" "$ESP32_PORT"
            return 0
        fi
        sleep 1  # Give it time to initialize
    fi
}

# Wait for ESP32 to reboot (unless monitor-only)
if [ "$MONITOR_ONLY" = false ]; then
    # Start serial monitor FIRST, then wait for boot
    start_background_serial_monitor force
    echo -e "${YELLOW}⏳ Waiting for ESP32 to boot...${NC}"
    sleep 3
elif [ "$SHOW_SERIAL" = true ]; then
    start_background_serial_monitor
fi

# Enhanced cleanup function for serial monitor
cleanup_serial_monitor() {
    echo -e "${YELLOW}🔌 Stopping all serial monitors...${NC}"
    
    # Kill specific PID if available
    if [ ! -z "$SERIAL_MONITOR_PID" ]; then
        kill $SERIAL_MONITOR_PID 2>/dev/null || true
        wait $SERIAL_MONITOR_PID 2>/dev/null || true
    fi
    
    # Comprehensive cleanup of all monitor processes
    cleanup_port_completely
    
    # Clean up temporary files
    rm -f /tmp/esp32_monitor_*.py 2>/dev/null || true
    rm -f /tmp/serial_monitor_*.sh 2>/dev/null || true
}

# Set up cleanup trap
trap cleanup_serial_monitor EXIT
    
    # Create a dedicated serial log file for this session
    SERIAL_LOG="$LOGS_DIR/esp32_serial_${LOG_TIMESTAMP}.log"
    log_message "Serial output will be saved to: $SERIAL_LOG"
    
    # Try different monitoring approaches
    if command -v wt.exe >/dev/null 2>&1; then
        # Windows Terminal approach with proper error handling
        log_message "${GREEN}Attempting to open monitor in new Windows Terminal tab...${NC}"
        
        # Create a temporary script file to avoid command line parsing issues
        TEMP_SCRIPT="/tmp/esp32_monitor_${LOG_TIMESTAMP}.sh"
        cat > "$TEMP_SCRIPT" << 'EOF'
#!/bin/bash
cd "$1"
python3 -c "
import serial
import time
import datetime
import sys

def log_to_file(msg, log_file):
    with open(log_file, 'a') as f:
        timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        f.write(f'[{timestamp}] {msg}\n')

def log_with_timestamp(msg, log_file=''):
    timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
    output = f'[{timestamp}] {msg}'
    print(output)
    if log_file:
        log_to_file(msg, log_file)

serial_log = '$2'
esp32_port = '$3'

print('=== ESP32 Serial Monitor ===')
print(f'Port: {esp32_port} | Baud: 115200')
print(f'Started: {datetime.datetime.now().strftime(\"%Y-%m-%d %H:%M:%S\")}')
print(f'Log File: {serial_log}')
print('='*50)

try:
    ser = serial.Serial(esp32_port, 115200, timeout=1)
    log_with_timestamp('Serial connection established', serial_log)
    
    line_buffer = ''
    
    while True:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            
            for char in data:
                if char == '\n':
                    if line_buffer.strip():
                        line = line_buffer.strip()
                        
                        # Categorize and colorize messages
                        if any(keyword in line.lower() for keyword in ['error', 'failed', 'exception']):
                            colored_line = f'\033[1;31m[{datetime.datetime.now().strftime(\"%H:%M:%S\")}] {line}\033[0m'
                        elif any(keyword in line.lower() for keyword in ['wifi', 'connected', 'ip']):
                            colored_line = f'\033[1;33m[{datetime.datetime.now().strftime(\"%H:%M:%S\")}] {line}\033[0m'
                        elif any(keyword in line.lower() for keyword in ['boot', 'ready', 'start']):
                            colored_line = f'\033[1;32m[{datetime.datetime.now().strftime(\"%H:%M:%S\")}] {line}\033[0m'
                        elif '[' in line and ']' in line:
                            colored_line = f'\033[1;34m[{datetime.datetime.now().strftime(\"%H:%M:%S\")}] {line}\033[0m'
                        else:
                            colored_line = f'[{datetime.datetime.now().strftime(\"%H:%M:%S\")}] {line}'
                        
                        print(colored_line)
                        log_to_file(line, serial_log)
                        
                    line_buffer = ''
                else:
                    line_buffer += char
        
        time.sleep(0.05)
        
except KeyboardInterrupt:
    log_with_timestamp('Monitor stopped by user', serial_log)
except Exception as e:
    log_with_timestamp(f'Monitor error: {e}', serial_log)
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        log_with_timestamp('Serial connection closed', serial_log)
"
EOF
        chmod +x "$TEMP_SCRIPT"
        
        # Try to launch Windows Terminal
        if wt.exe --window 0 new-tab \
            --profile "Ubuntu" \
            --title "ESP32 Monitor - $LOG_TIMESTAMP" \
            --tabColor "#00FF00" \
            bash "$TEMP_SCRIPT" "$PROJECT_DIR" "$SERIAL_LOG" "$ESP32_PORT" 2>/dev/null; then
            
            log_message "${GREEN}📺 Monitor opened in new terminal tab${NC}"
            log_message "Serial output being logged to: $SERIAL_LOG"
        else
            log_message "${YELLOW}⚠️  Windows Terminal failed, falling back to direct monitoring${NC}"
            rm -f "$TEMP_SCRIPT"
            # Fall back to direct monitoring
            SHOW_DIRECT_MONITOR=true
        fi
        
        # Also provide a quick status check
        log_message "${BLUE}📊 Quick status check:${NC}"
        python3 -c "
import requests
import time
time.sleep(2)  # Wait for boot
try:
    response = requests.get('http://192.168.1.157/api/status', timeout=3)
    data = response.json()
    print(f'✅ Device online - Memory: {data[\"memory\"]}KB, Uptime: {data[\"uptime\"]}s')
except Exception as e:
    print(f'⚠️  Device not responding yet: {e}')
" | tee -a "$SESSION_LOG"
        
    else
        SHOW_DIRECT_MONITOR=true
    fi
    
    # Direct monitoring fallback or when Windows Terminal is not available
    if [ "$SHOW_DIRECT_MONITOR" = true ] || [ ! command -v wt.exe >/dev/null 2>&1 ]; then
        log_message "${GREEN}Starting enhanced serial monitor with logging (Press Ctrl+C to stop)...${NC}"
        log_message "Serial output will be saved to: $SERIAL_LOG"
        
        python3 -c "
import serial
import time
import datetime

def log_to_file(msg, log_file):
    with open(log_file, 'a') as f:
        timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        f.write(f'[{timestamp}] {msg}\n')

def log_with_timestamp(msg, color='', log_file='$SERIAL_LOG'):
    timestamp = datetime.datetime.now().strftime('%H:%M:%S.%f')[:-3]
    output = f'[{timestamp}] {msg}'
    
    if color:
        print(f'{color}{output}\033[0m')
    else:
        print(output)
    
    # Always log to file (without color codes)
    log_to_file(msg, log_file)

print('=== ESP32 Enhanced Serial Monitor with Logging ===')
print(f'Port: $ESP32_PORT | Baud: 115200')
print(f'Started: {datetime.datetime.now().strftime(\"%Y-%m-%d %H:%M:%S\")}')
print(f'Log File: $SERIAL_LOG')
print('='*60)

try:
    ser = serial.Serial('$ESP32_PORT', 115200, timeout=1)
    log_with_timestamp('Serial connection established', '\033[1;32m')
    
    line_buffer = ''
    boot_messages = []
    
    while True:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            
            for char in data:
                if char == '\n':
                    if line_buffer.strip():
                        line = line_buffer.strip()
                        
                        # Categorize and colorize messages
                        if any(keyword in line.lower() for keyword in ['error', 'failed', 'exception']):
                            log_with_timestamp(line, '\033[1;31m')  # Red for errors
                        elif any(keyword in line.lower() for keyword in ['wifi', 'connected', 'ip']):
                            log_with_timestamp(line, '\033[1;33m')  # Yellow for network
                        elif any(keyword in line.lower() for keyword in ['boot', 'ready', 'start']):
                            log_with_timestamp(line, '\033[1;32m')  # Green for boot
                            boot_messages.append(line)
                        elif '[' in line and ']' in line:
                            log_with_timestamp(line, '\033[1;34m')  # Blue for tagged messages
                        else:
                            log_with_timestamp(line)
                    line_buffer = ''
                else:
                    line_buffer += char
        
        time.sleep(0.05)
        
except KeyboardInterrupt:
    print('\n\033[1;31m[MONITOR] Stopped by user\033[0m')
    log_to_file('Monitor stopped by user', '$SERIAL_LOG')
    if boot_messages:
        print('\n\033[1;36m=== Boot Messages Summary ===\033[0m')
        for msg in boot_messages[-5:]:  # Last 5 boot messages
            print(f'\033[0;36m{msg}\033[0m')
except Exception as e:
    error_msg = f'Monitor error: {e}'
    print(f'\033[1;31m[ERROR] {error_msg}\033[0m')
    log_to_file(error_msg, '$SERIAL_LOG')
finally:
    if 'ser' in locals() and ser.is_open:
# Serial monitoring cleanup handled by trap

echo ""
log_message "${GREEN}✅ Operation complete!${NC}"

if [ "$MONITOR_ONLY" = true ]; then
    log_message "${BLUE}Monitoring session completed${NC}"
elif [ "$RESET_ONLY" = true ]; then
    log_message "${BLUE}Device reset completed${NC}"
elif [ "$FS_ONLY" = true ]; then
    log_message "${BLUE}Filesystem upload completed${NC}"
else
    log_message "${BLUE}Deployment completed${NC}"
fi

# Log summary
log_message "=== Session Summary ==="
log_message "Session Log: $SESSION_LOG"
if [ "$SHOW_SERIAL" = true ] && [ -f "$SERIAL_LOG" ]; then
    log_message "Serial Log: $SERIAL_LOG"
    SERIAL_LINES=$(wc -l < "$SERIAL_LOG" 2>/dev/null || echo "0")
    log_message "Serial output lines captured: $SERIAL_LINES"
fi

# Show available logs
echo ""
echo -e "${BLUE}📁 Session Logs:${NC}"
echo -e "   📄 Session: $SESSION_LOG"
if [ -f "$SERIAL_LOG" ]; then
    echo -e "   📊 Serial: $SERIAL_LOG"
fi

# List recent log files
echo ""
echo -e "${BLUE}📚 Recent log files in runtime-logs/:${NC}"
ls -la "$LOGS_DIR"/*.log 2>/dev/null | tail -5 | while read line; do
    echo -e "   $line"
done

if [ "$SHOW_SERIAL" = false ]; then
    echo -e "${YELLOW}💡 Use '$0 -s' to include serial monitoring${NC}"
fi

# Harvest IP address from ESP32 serial output
harvest_ip_address() {
    local timeout=30
    local ip_address=""
    
    echo -e "${BLUE}📡 Harvesting IP address from ESP32 serial output...${NC}"
    echo "Monitoring serial port for DHCP assignment..."
    
    # Use timeout command to limit monitoring time
    ip_address=$(timeout $timeout python3 -c "
import serial
import sys
import time
import re

try:
    ser = serial.Serial('$ESP32_PORT', 115200, timeout=1)
    start_time = time.time()
    
    while time.time() - start_time < $timeout:
        try:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                # Look for IP address patterns in ESP32 logs
                ip_match = re.search(r'IP address: (\d+\.\d+\.\d+\.\d+)', line)
                if ip_match:
                    print(ip_match.group(1))
                    sys.exit(0)
                    
                # Alternative patterns
                server_match = re.search(r'Server accessible at: http://(\d+\.\d+\.\d+\.\d+)', line)
                if server_match:
                    print(server_match.group(1))
                    sys.exit(0)
        except Exception:
            continue
            
    sys.exit(1)  # Timeout
except Exception as e:
    sys.exit(1)
" 2>/dev/null)
    
    if [ ! -z "$ip_address" ]; then
        echo -e "${GREEN}✅ ESP32 IP Address: $ip_address${NC}"
        
        # Update .env file if IP changed
        if [ "$ip_address" != "$ESP32_IP" ]; then
            echo -e "${BLUE}🔄 IP changed from ${ESP32_IP:-'none'} to $ip_address, updating .env${NC}"
            update_esp32_ip "$ip_address"
        else
            echo -e "${GREEN}✅ IP unchanged: $ip_address${NC}"
        fi
        
        echo ""
        echo -e "${BLUE}🌐 Web Interface: http://$ip_address/${NC}"
        echo -e "${BLUE}📊 API Endpoint: http://$ip_address/api/${NC}"
        echo -e "${BLUE}🔍 Test Page: http://$ip_address/test${NC}"
        log_message "ESP32 IP Address harvested: $ip_address"
        return 0
    else
        echo -e "${YELLOW}⚠️  Could not harvest IP address from serial output${NC}"
        echo -e "${YELLOW}💡 Try manually checking serial monitor or network scan${NC}"
        log_message "Failed to harvest IP address from serial output"
        return 1
    fi
}

echo ""
# Always harvest IP address from serial logs
if ! harvest_ip_address; then
    echo -e "${RED}❌ Failed to determine ESP32 IP address${NC}"
    echo -e "${YELLOW}💡 Check serial monitor manually: python3 serial_monitor.py${NC}"
fi

# Add session end to log
log_message "=== Session Ended ==="