#!/usr/bin/env bash

# encode-ggv.sh
# Script to encode all GGV test projects in ggv-tests/

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Path to the encode-orc executable
ENCODE_ORC="./build/encode-orc"

# Check if encode-orc exists
if [ ! -f "$ENCODE_ORC" ]; then
    echo -e "${RED}Error: encode-orc executable not found at $ENCODE_ORC${NC}"
    echo "Please build the project first with: cmake --build build"
    exit 1
fi

# Output root can be provided as first argument or via ENCODE_ORC_OUTPUT_ROOT
OUTPUT_ROOT="${ENCODE_ORC_OUTPUT_ROOT:-$SCRIPT_DIR/ggv-output}"
if [ -n "${1:-}" ]; then
    OUTPUT_ROOT="$1"
fi

export ENCODE_ORC_OUTPUT_ROOT="$OUTPUT_ROOT"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_ROOT"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Encoding GGV Test Projects${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Counter for tracking progress
total=0
success=0
failed=0

# Process each YAML file in ggv-tests/
for yaml_file in ggv-tests/*.yaml; do
    if [ -f "$yaml_file" ]; then
        total=$((total + 1))
        echo -e "${GREEN}Processing: $(basename "$yaml_file")${NC}"
        echo "---"
        
        if "$ENCODE_ORC" "$yaml_file"; then
            success=$((success + 1))
            echo -e "${GREEN}✓ Successfully encoded: $(basename "$yaml_file")${NC}"
        else
            failed=$((failed + 1))
            echo -e "${RED}✗ Failed to encode: $(basename "$yaml_file")${NC}"
        fi
        
        echo ""
    fi
done

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo "Total projects: $total"
echo -e "${GREEN}Successful: $success${NC}"
if [ $failed -gt 0 ]; then
    echo -e "${RED}Failed: $failed${NC}"
fi
echo ""

if [ $failed -gt 0 ]; then
    exit 1
fi

echo -e "${GREEN}All projects encoded successfully!${NC}"
