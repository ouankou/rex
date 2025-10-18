#!/bin/bash
#
# REX Build Script with Clang Frontend (LLVM 20)
# This script automates the build process for REX compiler
# with the experimental Clang frontend enabled.
#
# Usage:
#   ./build-rex.sh [install-prefix]
#
# Example:
#   ./build-rex.sh $HOME/rex-install
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_PREFIX="${1:-$HOME/rex-install}"
BUILD_DIR="build"
NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}REX Build Script with Clang Frontend${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Install prefix: $INSTALL_PREFIX"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs: $NUM_JOBS"
echo ""

# Check if we're in the repository root
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Please run this script from the repository root.${NC}"
    exit 1
fi

# Initialize git submodules if not already done
echo -e "${YELLOW}[1/5] Initializing git submodules...${NC}"
if git submodule status | grep -q '^-'; then
    git submodule update --init --recursive
    echo -e "${GREEN}Submodules initialized.${NC}"
else
    echo -e "${GREEN}Submodules already initialized.${NC}"
fi
echo ""

# Check for LLVM/Clang
echo -e "${YELLOW}[2/5] Checking for LLVM/Clang installation...${NC}"
if ! command -v llvm-config &> /dev/null; then
    echo -e "${RED}Error: llvm-config not found. Please install LLVM/Clang 20 or later.${NC}"
    echo "On Ubuntu/Debian: sudo apt-get install llvm-20 clang-20 libclang-20-dev"
    exit 1
fi

LLVM_VERSION=$(llvm-config --version)
echo -e "${GREEN}Found LLVM version: $LLVM_VERSION${NC}"
echo ""

# Create and enter build directory
echo -e "${YELLOW}[3/5] Configuring with CMake...${NC}"
if [ -d "$BUILD_DIR" ]; then
    echo "Removing existing build directory..."
    rm -rf "$BUILD_DIR" || { echo -e "${RED}Failed to remove build directory${NC}"; exit 1; }
fi
mkdir -p "$BUILD_DIR" || { echo -e "${RED}Failed to create build directory${NC}"; exit 1; }
cd "$BUILD_DIR" || { echo -e "${RED}Failed to enter build directory${NC}"; exit 1; }

# Configure with CMake (using Clang for LLVM 21 compatibility)
CC=clang CXX=clang++ cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -Denable-clang-frontend=ON \
    -Denable-c=ON \
    -Denable-fortran=OFF \
    -Denable-java=OFF \
    -Denable-php=OFF \
    -Denable-python=OFF \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi
echo -e "${GREEN}CMake configuration successful.${NC}"
echo ""

# Build
echo -e "${YELLOW}[4/5] Building REX (this may take a while)...${NC}"
cmake --build . -j $NUM_JOBS

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful.${NC}"
echo ""

# Install
echo -e "${YELLOW}[5/5] Installing to $INSTALL_PREFIX...${NC}"
cmake --install .

if [ $? -ne 0 ]; then
    echo -e "${RED}Installation failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Installation successful.${NC}"
echo ""

# Summary
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}REX Build Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Installation directory: $INSTALL_PREFIX"
echo "Compiler: $INSTALL_PREFIX/bin/rose-compiler"
echo ""
echo "To use REX, add the following to your PATH:"
echo "  export PATH=$INSTALL_PREFIX/bin:\$PATH"
echo "  export LD_LIBRARY_PATH=$INSTALL_PREFIX/lib:\$LD_LIBRARY_PATH"
echo ""
echo "Test with a simple C file:"
echo "  echo 'int main() { return 0; }' > test.c"
echo "  $INSTALL_PREFIX/bin/rose-compiler -c test.c"
echo ""
