#!/bin/bash
#
# Test runner script for encode-orc YAML projects
# Runs all test YAML files and outputs results to test-output directory
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Directories
TEST_PROJECTS_DIR="$PROJECT_ROOT/test-projects"
TEST_OUTPUT_DIR="$PROJECT_ROOT/test-output"
BUILD_DIR="$PROJECT_ROOT/build"

# Build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${BLUE}Building project...${NC}"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake ..
    make
    cd "$PROJECT_ROOT"
fi

ENCODE_ORC="$BUILD_DIR/encode-orc"

if [ ! -f "$ENCODE_ORC" ]; then
    echo -e "${RED}Error: encode-orc executable not found. Build may have failed.${NC}"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$TEST_OUTPUT_DIR"

# Count tests
total_tests=0
passed_tests=0
failed_tests=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  encode-orc YAML Project Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Find all YAML files in test-projects directory
if [ ! -d "$TEST_PROJECTS_DIR" ]; then
    echo -e "${RED}Error: test-projects directory not found at $TEST_PROJECTS_DIR${NC}"
    exit 1
fi

yaml_files=("$TEST_PROJECTS_DIR"/*rgb30*.yaml)

if [ ${#yaml_files[@]} -eq 0 ] || [ ! -e "${yaml_files[0]}" ]; then
    echo -e "${RED}Error: No RGB30 YAML files found in $TEST_PROJECTS_DIR${NC}"
    exit 1
fi

# Run each test
for yaml_file in "${yaml_files[@]}"; do
    if [ -f "$yaml_file" ]; then
        total_tests=$((total_tests + 1))
        filename=$(basename "$yaml_file")
        
        echo -n "Test $total_tests: $filename ... "
        
        # Run encode-orc with YAML file
        if "$ENCODE_ORC" "$yaml_file" > /dev/null 2>&1; then
            echo -e "${GREEN}PASSED${NC}"
            passed_tests=$((passed_tests + 1))
            
            # Extract output filename from YAML to verify it was created
            output_file=$(grep "filename:" "$yaml_file" | head -1 | sed 's/.*filename:[[:space:]]*"\([^"]*\)".*/\1/')
            if [ -f "$output_file" ]; then
                file_size=$(stat -f%z "$output_file" 2>/dev/null || stat -c%s "$output_file" 2>/dev/null)
                echo "  └─ Output: $output_file ($(numfmt --to=iec-i --suffix=B $file_size 2>/dev/null || echo "$file_size bytes"))"
            fi
        else
            echo -e "${RED}FAILED${NC}"
            failed_tests=$((failed_tests + 1))
        fi
    fi
done

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo "Total tests:  $total_tests"
echo -e "Passed:       ${GREEN}$passed_tests${NC}"
if [ $failed_tests -gt 0 ]; then
    echo -e "Failed:       ${RED}$failed_tests${NC}"
else
    echo -e "Failed:       ${GREEN}$failed_tests${NC}"
fi
echo ""

if [ $failed_tests -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi
