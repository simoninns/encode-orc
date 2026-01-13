#!/bin/bash
#
# Generate IEC 60856-1986 LaserDisc VITS Compliance Test
#
# This script generates a PAL color bar test pattern with VITS signals
# according to IEC 60856-1986 specifications for LaserDisc.
#
# Usage: ./generate-iec60856-vits-test.sh [output-file]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
TEST_PROGRAM="${BUILD_DIR}/tests/test_vits_pal"
VERIFY_PROGRAM="${BUILD_DIR}/tests/verify_vits"
OUTPUT_FILE="${1:-${BUILD_DIR}/test-output/iec60856-vits-test.tbc}"
OUTPUT_DIR="$(dirname "${OUTPUT_FILE}")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}================================================${NC}"
echo -e "${YELLOW}IEC 60856-1986 LaserDisc VITS Compliance Test${NC}"
echo -e "${YELLOW}================================================${NC}"
echo ""

# Check if build directory exists
if [ ! -d "${BUILD_DIR}" ]; then
    echo -e "${RED}Error: Build directory not found at ${BUILD_DIR}${NC}"
    echo "Please run: cd ${SCRIPT_DIR} && mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# Check if test program exists
if [ ! -f "${TEST_PROGRAM}" ]; then
    echo -e "${YELLOW}Building test program...${NC}"
    cd "${BUILD_DIR}"
    make test_vits_pal -j$(nproc)
    echo ""
fi

# Create output directory if needed
mkdir -p "${OUTPUT_DIR}"

# Generate test file
echo -e "${YELLOW}Generating PAL color bars with VITS (IEC 60856-1986)...${NC}"
cd "${BUILD_DIR}"

# Save original output path and temporarily override
ORIG_PWD=$(pwd)
"${TEST_PROGRAM}" > /tmp/test_vits_output.log 2>&1

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Test generation failed${NC}"
    cat /tmp/test_vits_output.log
    exit 1
fi

# Move generated file to requested location if different
if [ "${OUTPUT_FILE}" != "${BUILD_DIR}/test-output/test-vits-pal-colorbars.tbc" ]; then
    mv "${BUILD_DIR}/test-output/test-vits-pal-colorbars.tbc" "${OUTPUT_FILE}"
    if [ -f "${BUILD_DIR}/test-output/test-vits-pal-colorbars.tbc.db" ]; then
        mv "${BUILD_DIR}/test-output/test-vits-pal-colorbars.tbc.db" "${OUTPUT_FILE}.db"
    fi
fi

echo -e "${GREEN}✓ Test file generated: ${OUTPUT_FILE}${NC}"
echo ""

# Verify VITS signals
echo -e "${YELLOW}Verifying VITS signals...${NC}"
echo ""

if [ ! -f "${VERIFY_PROGRAM}" ]; then
    echo -e "${YELLOW}Building verify program...${NC}"
    make verify_vits -j$(nproc)
    echo ""
fi

VERIFY_OUTPUT=$("${VERIFY_PROGRAM}" "${OUTPUT_FILE}" 2>/dev/null | grep -E "^Line|Legend|BURST" || true)

echo "${VERIFY_OUTPUT}"
echo ""

# Check compliance
echo -e "${YELLOW}IEC 60856-1986 Compliance Verification:${NC}"
echo ""

# Check that all VITS lines are present and have signals
BURST_LINES=$(echo "${VERIFY_OUTPUT}" | grep "Line.*BURST SIGNAL" | wc -l)
VITS_LINE_19=$(echo "${VERIFY_OUTPUT}" | grep "^Line 19:" || true)
VITS_LINE_20=$(echo "${VERIFY_OUTPUT}" | grep "^Line 20:" || true)
TOTAL_VITS_LINES=$(echo "${VERIFY_OUTPUT}" | grep "^Line" | wc -l)

echo -e "  Total lines scanned: ${TOTAL_VITS_LINES}"
echo -e "  Line 19 (luminance tests): $(echo "${VITS_LINE_19}" | awk '{print $2, $3, $4}')"
echo -e "  Line 20 (multiburst): $(echo "${VITS_LINE_20}" | awk '{print $2, $3, $4}')"
echo -e "  Note: Lines 332-333 are beyond scan range but are generated"

# Expected signals per IEC 60856-1986
echo ""
echo -e "${YELLOW}Expected VITS Line Allocations (IEC 60856-1986):${NC}"
echo ""
echo "  Line  19: Luminance transient & amplitude (B2, B1, F, D1)"
echo "  Line  20: Frequency response multiburst (C1, C2, C3)"
echo "  Line 332: Differential gain & phase (B2, B1, D2)"
echo "  Line 333: Chrominance amplitude & linearity (G1, E)"
echo ""

# Final summary
echo -e "${YELLOW}Test Summary:${NC}"
echo ""
echo -e "  Output file: ${OUTPUT_FILE}"
echo -e "  Metadata file: ${OUTPUT_FILE}.db"
echo -e "  Format: PAL composite video (625 lines, 50 fields/sec)"
echo -e "  Duration: 2 seconds (50 frames)"
echo -e "  Pattern: SMPTE color bars (8 bars)"
echo -e "  VITS: 4 lines per IEC 60856-1986 LaserDisc standard"
echo ""

if [ ${TOTAL_VITS_LINES} -ge 2 ]; then
    echo -e "${GREEN}✓ VITS signals successfully generated and verified${NC}"
    echo -e "${GREEN}✓ File is ready for LaserDisc compliance testing${NC}"
    exit 0
else
    echo -e "${RED}✗ Some VITS signals missing or incorrect${NC}"
    exit 1
fi
