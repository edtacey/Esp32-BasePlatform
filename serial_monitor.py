#!/usr/bin/env python3

import serial
import time
import sys

PORT = '/dev/ttyUSB0'
BAUDRATE = 115200

try:
    # Open serial connection
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    
    print(f"=== ESP32 Serial Monitor ===")
    print(f"Port: {PORT}, Baud: {BAUDRATE}")
    print(f"Press Ctrl+C to exit")
    print("=" * 50)
    
    # Clear any buffered data
    ser.reset_input_buffer()
    
    # Read and display serial data
    start_time = time.time()
    while time.time() - start_time < 20:  # Run for 20 seconds
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8', errors='replace').rstrip()
                if line:
                    timestamp = time.strftime('[%H:%M:%S]')
                    print(f"{timestamp} {line}")
                    sys.stdout.flush()
            except Exception as e:
                print(f"Error reading line: {e}")
        else:
            time.sleep(0.01)
    
    ser.close()
    print("\nMonitoring complete.")
    
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
except KeyboardInterrupt:
    print("\nMonitoring stopped by user.")
    if 'ser' in locals() and ser.is_open:
        ser.close()
except Exception as e:
    print(f"Unexpected error: {e}")