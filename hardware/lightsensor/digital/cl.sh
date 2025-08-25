#!/bin/bash

# Claude launcher script with auto-approve for common operations
# Usage: ./cl.sh [arguments]

claude --dangerously-skip-permissions "$@"