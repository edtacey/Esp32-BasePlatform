# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Boundaries

**CRITICAL**: This is a baseline/template directory. No files or resources should be consumed from outside this BaseLine-B directory without explicit user permission.

## Available Tools

### Quick Launch
- `./cl.sh` - Claude launcher script with auto-approve for common operations
  - Usage: `./cl.sh [arguments]`
  - Runs: `claude --dangerously-skip-permissions "$@"`

## Development Workflow

When working in this directory:
1. Always confirm before accessing files outside `/home/edt/ccode/esp32/BaseLine-B/`
2. This appears to be a clean baseline for ESP32 development projects
3. Any project initialization should be done within this directory only

## Important Notes

- This is a minimal baseline directory with just the Claude launcher script
- No external dependencies or references should be assumed
- All development should be self-contained within this directory unless explicitly authorized
- take every thing in OrinatingBrief dir as inital directives for the project breif only formal engineering and architecture mds should always replace this
- when considering changing an external api interface ALWAYS ALWAYS ALWAYS show me the as is and to Be  before proceeding. Make extra effort to ensure you are not duplicating an existing like Function or interface.

## ESP32 Component Architecture Compliance

**MANDATORY**: When implementing new components or extending IoT capabilities:

1. **ALWAYS refer to `ESP32_COMPONENT_ARCHITECTURE.md`** for complete architectural guidelines, naming conventions, and implementation patterns
2. **ALWAYS use `TemplateComponent.h/.cpp`** as starting point for new component development
3. **VERY IMPORTANT**: If any proposed change falls outside the established architecture patterns documented in `ESP32_COMPONENT_ARCHITECTURE.md`, you MUST:
   - Stop implementation immediately
   - Check with user to either:
     a) Update the architecture document to accommodate the new pattern, OR
     b) Change the approach to comply with existing architecture
4. **NEVER** deviate from BaseComponent inheritance requirements, execution loop integration, or configuration persistence patterns without explicit approval