#!/bin/bash
# Test script for encode-orc
# Generates test TBC files for validation with decode-orc

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
TEST_OUTPUT_DIR="${PROJECT_DIR}/test-output"
ENCODE_ORC="${PROJECT_DIR}/build/encode-orc"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

# Check if encode-orc binary exists
if [ ! -f "$ENCODE_ORC" ]; then
    print_error "encode-orc binary not found at $ENCODE_ORC"
    print_info "Run 'cmake --build build' first"
    exit 1
fi

# Create test output directory
mkdir -p "$TEST_OUTPUT_DIR"
print_info "Test output directory: $TEST_OUTPUT_DIR"

# Function to run a test
run_test() {
    local name=$1
    local format=$2
    local frames=$3
    local output="${TEST_OUTPUT_DIR}/${name}.tbc"
    
    # Clean up any existing test files
    rm -f "$output" "${output}.db"
    
    print_info "Running test: $name (format=$format, frames=$frames)"
    
    if $ENCODE_ORC -o "$output" -f "$format" -n "$frames"; then
        # Check if files were created
        if [ ! -f "$output" ]; then
            print_error "$name: TBC file not created"
            return 1
        fi
        
        if [ ! -f "${output}.db" ]; then
            print_error "$name: Metadata database not created"
            return 1
        fi
        
        # Check file sizes are reasonable
        local tbc_size=$(stat -f%z "$output" 2>/dev/null || stat -c%s "$output" 2>/dev/null)
        if [ "$tbc_size" -lt 1000 ]; then
            print_error "$name: TBC file too small ($tbc_size bytes)"
            return 1
        fi
        
        # Verify metadata database is valid SQLite
        if ! sqlite3 "${output}.db" "PRAGMA integrity_check;" > /dev/null 2>&1; then
            print_error "$name: Metadata database is not valid SQLite"
            return 1
        fi
        
        # Verify capture table exists
        if ! sqlite3 "${output}.db" "SELECT COUNT(*) FROM capture;" > /dev/null 2>&1; then
            print_error "$name: Metadata database missing capture table"
            return 1
        fi
        
        # Verify field records
        local field_count=$(sqlite3 "${output}.db" "SELECT COUNT(*) FROM field_record;")
        local expected_fields=$((frames * 2))
        if [ "$field_count" != "$expected_fields" ]; then
            print_error "$name: Expected $expected_fields fields, got $field_count"
            return 1
        fi
        
        print_success "$name: Generated and validated ($tbc_size bytes, $field_count fields)"
        return 0
    else
        print_error "$name: encode-orc failed"
        return 1
    fi
}

# Run tests
print_info "Starting encode-orc tests"
echo ""

TESTS_PASSED=0
TESTS_FAILED=0

# PAL tests
if run_test "pal-composite-10" "pal-composite" 10; then
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

if run_test "pal-composite-100" "pal-composite" 100; then
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

# NTSC tests
if run_test "ntsc-composite-10" "ntsc-composite" 10; then
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

if run_test "ntsc-composite-100" "ntsc-composite" 100; then
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

# Summary
echo ""
print_info "Test Summary:"
echo "  Passed: $TESTS_PASSED"
echo "  Failed: $TESTS_FAILED"

if [ $TESTS_FAILED -eq 0 ]; then
    print_success "All tests passed!"
    print_info "Test files available in $TEST_OUTPUT_DIR for decode-orc validation"
    exit 0
else
    print_error "Some tests failed"
    exit 1
fi
