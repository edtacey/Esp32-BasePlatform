ESP32 IoT Orchestrator - LittleFS Data Directory
==================================================

This directory contains files that will be uploaded to the ESP32's
LittleFS filesystem.

Directory Structure:
- /config/     - Component configurations
- /schemas/    - Component schemas  
- /logs/       - Log files (if filesystem logging enabled)

The filesystem is initialized empty, and configurations/schemas
are stored dynamically as components are created and configured.