#!/bin/bash

# ESP32 IoT Orchestrator - Development Tools
# Collection of development utilities for the baseline project

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

log() { echo -e "${BLUE}[$(date '+%H:%M:%S')]${NC} $1"; }
success() { echo -e "${GREEN}✅${NC} $1"; }
error() { echo -e "${RED}❌${NC} $1"; }
warning() { echo -e "${YELLOW}⚠️${NC} $1"; }
info() { echo -e "${CYAN}ℹ️${NC} $1"; }

check_dependencies() {
    echo -e "${PURPLE}🔍 Checking Development Dependencies${NC}"
    echo ""
    
    local missing=()
    
    # Check PlatformIO
    if command -v pio &> /dev/null; then
        echo -e "${GREEN}✅ PlatformIO${NC} ($(pio --version))"
    else
        echo -e "${RED}❌ PlatformIO${NC}"
        missing+=("platformio")
    fi
    
    # Check Python3
    if command -v python3 &> /dev/null; then
        echo -e "${GREEN}✅ Python3${NC} ($(python3 --version | cut -d' ' -f2))"
    else
        echo -e "${RED}❌ Python3${NC}"
        missing+=("python3")
    fi
    
    # Check esptool
    if command -v esptool.py &> /dev/null; then
        echo -e "${GREEN}✅ esptool.py${NC}"
    else
        echo -e "${YELLOW}⚠️ esptool.py${NC} (optional)"
    fi
    
    # Check terminals
    echo ""
    echo -e "${CYAN}Available Terminals:${NC}"
    local terminals=("konsole" "xfce4-terminal" "mate-terminal" "xterm" "terminator" "gnome-terminal")
    local found_terminals=()
    
    for terminal in "${terminals[@]}"; do
        if command -v "$terminal" &> /dev/null; then
            echo -e "  ${GREEN}✅ $terminal${NC}"
            found_terminals+=("$terminal")
        else
            echo -e "  ${RED}❌ $terminal${NC}"
        fi
    done
    
    if [[ ${#found_terminals[@]} -eq 0 ]]; then
        missing+=("terminal")
    fi
    
    # Check serial ports
    echo ""
    echo -e "${CYAN}Available Serial Ports:${NC}"
    local ports=($(ls /dev/tty* 2>/dev/null | grep -E "(USB|ACM)" || true))
    
    if [[ ${#ports[@]} -eq 0 ]]; then
        echo -e "  ${RED}❌ No USB/ACM ports found${NC}"
        echo "     Connect ESP32 via USB cable"
    else
        for port in "${ports[@]}"; do
            echo -e "  ${GREEN}✅ $port${NC}"
        done
    fi
    
    # Summary
    echo ""
    if [[ ${#missing[@]} -eq 0 ]]; then
        success "All dependencies satisfied"
    else
        warning "Missing dependencies: ${missing[*]}"
        echo ""
        echo "Install missing dependencies:"
        for dep in "${missing[@]}"; do
            case "$dep" in
                "platformio")
                    echo "  curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/scripts/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules"
                    echo "  pip3 install platformio"
                    ;;
                "python3")
                    echo "  sudo apt update && sudo apt install python3 python3-pip"
                    ;;
                "terminal")
                    echo "  sudo apt install konsole      # (recommended)"
                    echo "  sudo apt install xfce4-terminal"
                    echo "  sudo apt install mate-terminal"
                    ;;
            esac
        done
    fi
}

project_info() {
    echo -e "${PURPLE}📋 Project Information${NC}"
    echo ""
    
    echo "Project Directory: $PROJECT_DIR"
    echo "Project Type: ESP32 IoT Orchestrator (Baseline)"
    echo ""
    
    # Check PlatformIO project
    if [[ -f "$PROJECT_DIR/platformio.ini" ]]; then
        echo -e "${GREEN}✅ PlatformIO Project${NC}"
        
        # Show board info
        local board=$(grep "^board = " "$PROJECT_DIR/platformio.ini" | cut -d' ' -f3)
        local platform=$(grep "^platform = " "$PROJECT_DIR/platformio.ini" | cut -d' ' -f3)
        echo "  Board: $board"
        echo "  Platform: $platform"
        
        # Show dependencies
        echo ""
        echo "Library Dependencies:"
        grep -A 10 "^lib_deps" "$PROJECT_DIR/platformio.ini" | grep -v "^lib_deps" | while read -r dep; do
            if [[ -n "$dep" && "$dep" != *"="* ]]; then
                echo "  📦 ${dep#    }"
            fi
        done
    else
        echo -e "${RED}❌ Not a PlatformIO project${NC}"
    fi
    
    # Show source structure
    echo ""
    echo "Source Structure:"
    if [[ -d "$PROJECT_DIR/src" ]]; then
        find "$PROJECT_DIR/src" -name "*.h" -o -name "*.cpp" | sort | while read -r file; do
            local rel_path=${file#$PROJECT_DIR/}
            echo "  📄 $rel_path"
        done
    fi
}

clean_project() {
    echo -e "${PURPLE}🧹 Cleaning Project${NC}"
    echo ""
    
    cd "$PROJECT_DIR"
    
    log "Cleaning PlatformIO build artifacts..."
    pio run -t clean
    
    log "Removing log files..."
    rm -rf logs/*
    
    log "Removing backup files..."
    find . -name "*.bak" -delete 2>/dev/null || true
    find . -name "*~" -delete 2>/dev/null || true
    
    success "Project cleaned"
}

run_tests() {
    echo -e "${PURPLE}🧪 Running Tests${NC}"
    echo ""
    
    cd "$PROJECT_DIR"
    
    log "Running PlatformIO tests..."
    
    if pio test; then
        success "All tests passed"
    else
        error "Some tests failed"
        return 1
    fi
}

show_usage() {
    echo ""
    echo -e "${PURPLE}ESP32 IoT Orchestrator - Development Tools${NC}"
    echo ""
    echo "Usage: $0 [COMMAND]"
    echo ""
    echo "COMMANDS:"
    echo "  deps        Check development dependencies"
    echo "  info        Show project information"
    echo "  clean       Clean project build artifacts"
    echo "  test        Run tests"
    echo "  help        Show this help"
    echo ""
    echo "EXAMPLES:"
    echo "  $0 deps     # Check if all tools are installed"
    echo "  $0 info     # Show project structure and config"
    echo "  $0 clean    # Clean all build artifacts"
    echo ""
}

main() {
    local command="${1:-help}"
    
    case "$command" in
        "deps"|"dependencies")
            check_dependencies
            ;;
        "info"|"information")
            project_info
            ;;
        "clean")
            clean_project
            ;;
        "test"|"tests")
            run_tests
            ;;
        "help"|"--help"|"-h")
            show_usage
            ;;
        *)
            error "Unknown command: $command"
            show_usage
            exit 1
            ;;
    esac
}

main "$@"