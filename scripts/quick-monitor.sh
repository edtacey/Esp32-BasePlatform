#!/bin/bash

# Quick ESP32 Monitor Script
# Launches ESP32 serial monitor in the most reliable terminal available

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

# Configuration
ESP32_PORT="${1:-/dev/ttyUSB0}"
ESP32_BAUDRATE="115200"

# Terminal preference (reliability order)
TERMINALS=("konsole" "xfce4-terminal" "mate-terminal" "xterm" "terminator")

detect_terminal() {
    for terminal in "${TERMINALS[@]}"; do
        if command -v "$terminal" &> /dev/null; then
            echo "$terminal"
            return 0
        fi
    done
    echo "none"
    return 1
}

launch_monitor() {
    local terminal="$1"
    local title="ESP32 Monitor [$(basename "$ESP32_PORT")]"
    local cmd="pio device monitor --port '$ESP32_PORT' --baud '$ESP32_BAUDRATE' --filter esp32_exception_decoder --filter colorize --filter time"
    
    case "$terminal" in
        "konsole")
            konsole --title "$title" -e bash -c "$cmd; echo 'Press any key to close...'; read" &
            ;;
        "xfce4-terminal")
            xfce4-terminal --title="$title" --command="bash -c '$cmd; echo \"Press any key to close...\"; read'" &
            ;;
        "mate-terminal")
            mate-terminal --title="$title" --command="bash -c '$cmd; echo \"Press any key to close...\"; read'" &
            ;;
        "xterm")
            xterm -title "$title" -e bash -c "$cmd; echo 'Press any key to close...'; read" &
            ;;
        "terminator")
            terminator --title="$title" -x bash -c "$cmd; echo 'Press any key to close...'; read" &
            ;;
        *)
            echo -e "${RED}❌ No suitable terminal found${NC}"
            return 1
            ;;
    esac
}

main() {
    echo -e "${BLUE}🔧 ESP32 Quick Monitor${NC}"
    echo ""
    
    # Check if ESP32 port exists
    if [[ ! -e "$ESP32_PORT" ]]; then
        echo -e "${RED}❌ ESP32 port not found: $ESP32_PORT${NC}"
        echo "Available ports:"
        ls /dev/tty* 2>/dev/null | grep -E "(USB|ACM)" || echo "  None found"
        exit 1
    fi
    
    # Detect terminal
    local terminal=$(detect_terminal)
    if [[ "$terminal" == "none" ]]; then
        echo -e "${RED}❌ No suitable terminal found${NC}"
        echo "Please install one of: ${TERMINALS[*]}"
        exit 1
    fi
    
    echo -e "${GREEN}✅ ESP32 found on: $ESP32_PORT${NC}"
    echo -e "${GREEN}✅ Using terminal: $terminal${NC}"
    echo ""
    
    # Launch monitor
    if launch_monitor "$terminal"; then
        echo -e "${GREEN}✅ Monitor launched successfully${NC}"
        echo "Monitor will open in new $terminal window"
    else
        echo -e "${RED}❌ Failed to launch monitor${NC}"
        exit 1
    fi
}

main "$@"