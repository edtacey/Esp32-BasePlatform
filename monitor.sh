#!/bin/bash

# Simple ESP32 Serial Monitor Script
PORT="/dev/ttyUSB0"
BAUD="115200"
LOG_FILE="esp32_monitor_$(date +%s).log"

echo "=== ESP32 Serial Monitor ==="
echo "Port: $PORT, Baud: $BAUD"
echo "Logging to: $LOG_FILE"
echo "Press Ctrl+C to stop"
echo ""

# Configure serial port
stty -F $PORT raw $BAUD -echo -echoe -echok -echoctl -echoke

# Start monitoring and log to file
echo "=== ESP32 IoT Orchestrator Serial Log ===" > $LOG_FILE
echo "Started: $(date '+%Y-%m-%d %H:%M:%S')" >> $LOG_FILE
echo "==================================================" >> $LOG_FILE

# Monitor and display
cat $PORT | while IFS= read -r line; do
    # Display with timestamp
    echo "[$(date '+%H:%M:%S')] $line"
    # Log to file
    echo "[$(date '+%H:%M:%S')] $line" >> $LOG_FILE
done