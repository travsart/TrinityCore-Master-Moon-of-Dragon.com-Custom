#!/bin/bash
# TrinityCore PlayerBot Module - Incremental Build & Test Script
# Designed for Claude Code Web iterative development

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-playerbot"
SOURCE_DIR="${SCRIPT_DIR}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
echo_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
echo_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
echo_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# ============================================================================
# STEP 1: Dependency Check
# ============================================================================

echo_info "============================================"
echo_info "Step 1: Checking build dependencies..."
echo_info "============================================"

MISSING_DEPS=()

# Check essential build tools
command -v cmake >/dev/null 2>&1 || MISSING_DEPS+=("cmake")
command -v g++ >/dev/null 2>&1 || MISSING_DEPS+=("g++")
command -v make >/dev/null 2>&1 || MISSING_DEPS+=("make")

# Check for required libraries
check_library() {
    local lib=$1
    if ! dpkg -l | grep -q "^ii.*${lib}"; then
        MISSING_DEPS+=("${lib}")
    fi
}

echo_info "Checking required libraries..."
check_library "libboost"
check_library "libssl-dev"
check_library "libreadline-dev"
check_library "libmysqlclient-dev"

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo_error "Missing dependencies: ${MISSING_DEPS[*]}"
    echo_info "Install with: sudo apt-get install ${MISSING_DEPS[*]}"
    exit 1
fi

echo_success "All dependencies found!"
echo_info "CMake version: $(cmake --version | head -1)"
echo_info "GCC version: $(g++ --version | head -1)"

# ============================================================================
# STEP 2: CMake Configuration
# ============================================================================

echo ""
echo_info "============================================"
echo_info "Step 2: Configuring CMake..."
echo_info "============================================"

# Create build directory
if [ ! -d "${BUILD_DIR}" ]; then
    echo_info "Creating build directory: ${BUILD_DIR}"
    mkdir -p "${BUILD_DIR}"
else
    echo_info "Using existing build directory: ${BUILD_DIR}"
fi

cd "${BUILD_DIR}"

# CMake configuration with minimal dependencies
# Only build what's necessary for PlayerBot module compilation
echo_info "Running CMake configuration..."

cmake "${SOURCE_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wno-unused-parameter -Werror=return-type" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_PLAYERBOT=1 \
    -DTOOLS=0 \
    -DSCRIPTS=1 \
    -DWITH_WARNINGS=1 \
    -DWITH_COREDEBUG=0 \
    2>&1 | tee cmake-config.log

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo_error "CMake configuration failed! Check cmake-config.log for details"
    exit 1
fi

echo_success "CMake configuration complete!"

# ============================================================================
# STEP 3: Build Targets Analysis
# ============================================================================

echo ""
echo_info "============================================"
echo_info "Step 3: Analyzing build targets..."
echo_info "============================================"

# Find PlayerBot-related targets
echo_info "Available PlayerBot targets:"
make help 2>/dev/null | grep -i playerbot || echo_warning "No specific PlayerBot targets found"

# ============================================================================
# STEP 4: Incremental Compilation
# ============================================================================

echo ""
echo_info "============================================"
echo_info "Step 4: Starting incremental compilation..."
echo_info "============================================"

# Determine number of cores for parallel build
CORES=$(nproc 2>/dev/null || echo 2)
echo_info "Using ${CORES} cores for parallel build"

# Try to build just the game library (which includes PlayerBot module)
echo_info "Building game library with PlayerBot module..."

# Build the game target which includes modules
if make -j${CORES} game 2>&1 | tee build.log; then
    echo_success "Build completed successfully!"
    BUILD_SUCCESS=true
else
    echo_warning "Build encountered errors (exit code: ${PIPESTATUS[0]})"
    BUILD_SUCCESS=false
fi

# ============================================================================
# STEP 5: Error Analysis
# ============================================================================

echo ""
echo_info "============================================"
echo_info "Step 5: Analyzing build results..."
echo_info "============================================"

# Extract compilation errors
ERROR_COUNT=$(grep -c "error:" build.log 2>/dev/null || echo 0)
WARNING_COUNT=$(grep -c "warning:" build.log 2>/dev/null || echo 0)

echo_info "Compilation statistics:"
echo_info "  - Errors: ${ERROR_COUNT}"
echo_info "  - Warnings: ${WARNING_COUNT}"

if [ "$BUILD_SUCCESS" = true ]; then
    echo_success "============================================"
    echo_success "BUILD SUCCESSFUL!"
    echo_success "============================================"
    exit 0
else
    echo_error "============================================"
    echo_error "BUILD FAILED - Error Summary:"
    echo_error "============================================"

    # Show first 20 errors
    echo ""
    echo_info "First 20 compilation errors:"
    grep "error:" build.log | head -20 || echo_warning "No errors found in log"

    echo ""
    echo_info "Full error log saved to: ${BUILD_DIR}/build.log"
    echo_info "To view full errors: cat ${BUILD_DIR}/build.log | grep -A 3 'error:'"

    exit 1
fi
