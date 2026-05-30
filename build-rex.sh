#!/bin/bash
#
# REX Build Script with Clang Frontend
# This script automates the build process for REX compiler
# with the experimental Clang frontend enabled.
#
# Usage:
#   ./build-rex.sh [install-prefix] [build-type]
#
# Example:
#   ./build-rex.sh $HOME/rex-install RelWithDebInfo
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_PREFIX="${1:-$HOME/rex-install}"
BUILD_TYPE="${2:-RelWithDebInfo}"
BUILD_DIR="build"
LLVM_REQUIRED_MAJOR="${LLVM_REQUIRED_MAJOR:-22}"
if ! [[ "$LLVM_REQUIRED_MAJOR" =~ ^[0-9]+$ ]]; then
    echo -e "${RED}Error: LLVM_REQUIRED_MAJOR must be a numeric major version, got '$LLVM_REQUIRED_MAJOR'.${NC}"
    exit 1
fi
NUM_JOBS="${NUM_JOBS:-$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
if ! [[ "$NUM_JOBS" =~ ^[0-9]+$ ]] || [ "$NUM_JOBS" -lt 1 ]; then
    NUM_JOBS=4
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}REX Build Script with Clang Frontend${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Install prefix: $INSTALL_PREFIX"
echo "Build type:     $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs: $NUM_JOBS"
echo ""

# Resolve a coherent LLVM toolchain from one prefix.
find_first_executable() {
    local search_dir="$1"
    shift
    local candidate
    for candidate in "$@"; do
        if [ -x "$search_dir/$candidate" ]; then
            printf '%s\n' "$search_dir/$candidate"
            return 0
        fi
    done
    return 1
}

has_flang_libraries() {
    local flang_root="$1"
    local libdir
    local libname

    for libdir in "$flang_root/lib" "$flang_root/lib64"; do
        [ -d "$libdir" ] || continue

        local missing=0
        for libname in \
            FortranParser \
            FortranSemantics \
            FortranEvaluate \
            FortranLower \
            FortranDecimal \
            FortranSupport; do
            if ! compgen -G "$libdir/lib${libname}.*" > /dev/null; then
                missing=1
                break
            fi
        done

        if [ "$missing" -eq 0 ]; then
            return 0
        fi
    done

    return 1
}

find_llvm_config() {
    local candidates=()
    local candidate_roots=()
    local candidate
    local version
    local major

    append_candidate() {
        local entry="$1"
        [ -n "$entry" ] || return 0
        candidates+=("$entry")
    }

    append_candidate_root() {
        local root="$1"
        [ -n "$root" ] || return 0
        candidate_roots+=("$root")
    }

    if [ -n "${LLVM_CONFIG:-}" ]; then
        append_candidate "$LLVM_CONFIG"
    fi
    if [ -n "${LLVM_ROOT:-}" ]; then
        append_candidate_root "$LLVM_ROOT"
    fi
    if [ -n "${LLVM_DIR:-}" ]; then
        append_candidate_root "$LLVM_DIR"
        candidate="$(cd "$LLVM_DIR/../../.." 2>/dev/null && pwd -P || true)"
        append_candidate_root "$candidate"
    fi
    if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
        local raw_prefixes="${CMAKE_PREFIX_PATH//;/:}"
        local prefix
        IFS=':' read -r -a _prefix_entries <<< "$raw_prefixes"
        for prefix in "${_prefix_entries[@]}"; do
            append_candidate_root "$prefix"
        done
    fi

    local root
    for root in "${candidate_roots[@]}"; do
        append_candidate "$root/bin/llvm-config"
        append_candidate "$root/bin/llvm-config-$LLVM_REQUIRED_MAJOR"
    done
    if command -v llvm-config >/dev/null 2>&1; then
        append_candidate "$(command -v llvm-config)"
    fi
    if command -v "llvm-config-$LLVM_REQUIRED_MAJOR" >/dev/null 2>&1; then
        append_candidate "$(command -v "llvm-config-$LLVM_REQUIRED_MAJOR")"
    fi

    local seen=""
    local bindir
    for candidate in "${candidates[@]}"; do
        [ -x "$candidate" ] || continue
        case " $seen " in
            *" $candidate "*) continue ;;
        esac
        seen="$seen $candidate"
        version=$("$candidate" --version 2>/dev/null || true)
        major=$(echo "$version" | sed -nE 's/^([0-9]+).*/\1/p')
        bindir=$("$candidate" --bindir 2>/dev/null || true)
        if [ -n "$major" ] && [ "$major" -ge "$LLVM_REQUIRED_MAJOR" ] &&
           [ -n "$bindir" ] &&
           [ -x "$bindir/llvm-ar" ] &&
           [ -x "$(find_first_executable "$bindir" "clang-$LLVM_REQUIRED_MAJOR" clang || true)" ] &&
           [ -x "$(find_first_executable "$bindir" "clang++-$LLVM_REQUIRED_MAJOR" clang++ || true)" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

prepend_path_if_dir() {
    local var_name="$1"
    local dir="$2"
    local current_value="${!var_name}"

    [ -d "$dir" ] || return 0
    if [ -n "$current_value" ]; then
        printf -v "$var_name" '%s:%s' "$dir" "$current_value"
    else
        printf -v "$var_name" '%s' "$dir"
    fi
    export "$var_name"
}

detect_system_multiarch() {
    local machine

    if command -v dpkg-architecture >/dev/null 2>&1; then
        dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null && return 0
    fi

    machine="$(uname -m)"
    case "$machine" in
        loong64)
            machine="loongarch64"
            ;;
    esac
    printf '%s-linux-gnu\n' "$machine"
}

# Prefer the packaged GCC/stdlib headers/libs that match the active machine
# when driving Clang.
if [ -z "${GCC_VERSION:-}" ] && command -v g++ >/dev/null 2>&1; then
    GCC_VERSION="$(g++ -dumpfullversion -dumpversion | sed -nE 's/^([0-9]+).*/\1/p')"
fi
GCC_VERSION="${GCC_VERSION:-14}"
GCC_MULTIARCH="$(gcc -print-multiarch 2>/dev/null || true)"
GCC_MULTIARCH="${GCC_MULTIARCH:-$(detect_system_multiarch)}"
GCC_LIBGCC_PATH="$(gcc -print-libgcc-file-name 2>/dev/null || true)"
if [ -n "$GCC_LIBGCC_PATH" ] && [ "${GCC_LIBGCC_PATH#/}" != "$GCC_LIBGCC_PATH" ]; then
    GCC_PREFIX="$(dirname "$GCC_LIBGCC_PATH")"
elif [ -n "$GCC_MULTIARCH" ]; then
    GCC_PREFIX="/usr/lib/gcc/${GCC_MULTIARCH}/${GCC_VERSION}"
else
    GCC_PREFIX="/usr/lib/gcc/$(detect_system_multiarch)/${GCC_VERSION}"
fi
prepend_path_if_dir CPLUS_INCLUDE_PATH "/usr/include/c++/${GCC_VERSION}"
if [ -n "$GCC_MULTIARCH" ]; then
    prepend_path_if_dir CPLUS_INCLUDE_PATH "/usr/include/${GCC_MULTIARCH}/c++/${GCC_VERSION}"
fi
prepend_path_if_dir LIBRARY_PATH "$GCC_PREFIX"
prepend_path_if_dir LD_LIBRARY_PATH "$GCC_PREFIX"

# Check if we're in the repository root
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: CMakeLists.txt not found. Please run this script from the repository root.${NC}"
    exit 1
fi

if [ "$(uname -s)" != "Linux" ]; then
    echo -e "${RED}Error: This project targets Linux only.${NC}"
    exit 1
fi

# Initialize git submodules if not already done
echo -e "${YELLOW}[1/5] Initializing git submodules...${NC}"
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    if git submodule status | grep -q '^-'; then
        git submodule update --init --recursive
        echo -e "${GREEN}Submodules initialized.${NC}"
    else
        echo -e "${GREEN}Submodules already initialized.${NC}"
    fi
else
    echo -e "${YELLOW}Git metadata not available; assuming submodule contents are already present.${NC}"
fi
echo ""

# Check for LLVM/Clang
echo -e "${YELLOW}[2/5] Checking for LLVM/Clang installation...${NC}"
LLVM_CONFIG_CMD="$(find_llvm_config || true)"
if [ -z "$LLVM_CONFIG_CMD" ]; then
    echo -e "${RED}Error: llvm-config not found. Please install LLVM/Clang $LLVM_REQUIRED_MAJOR or later.${NC}"
    echo "On Ubuntu/Debian: sudo apt-get install llvm-$LLVM_REQUIRED_MAJOR clang-$LLVM_REQUIRED_MAJOR libclang-$LLVM_REQUIRED_MAJOR-dev lld-$LLVM_REQUIRED_MAJOR mold"
    exit 1
fi

LLVM_VERSION=$($LLVM_CONFIG_CMD --version)
LLVM_MAJOR=$(echo "$LLVM_VERSION" | sed -nE 's/^([0-9]+).*/\1/p')
if [ -z "$LLVM_MAJOR" ] || [ "$LLVM_MAJOR" -lt "$LLVM_REQUIRED_MAJOR" ]; then
    echo -e "${RED}Error: detected LLVM version $LLVM_VERSION using '$LLVM_CONFIG_CMD'. REX requires LLVM/Clang $LLVM_REQUIRED_MAJOR or later.${NC}"
    exit 1
fi
LLVM_BINDIR=$($LLVM_CONFIG_CMD --bindir)
LLVM_PREFIX=$($LLVM_CONFIG_CMD --prefix)
AUTO_C_COMPILER="$(find_first_executable "$LLVM_BINDIR" "clang-$LLVM_REQUIRED_MAJOR" clang || true)"
AUTO_CXX_COMPILER="$(find_first_executable "$LLVM_BINDIR" "clang++-$LLVM_REQUIRED_MAJOR" clang++ || true)"
AUTO_LLVM_AR="$(find_first_executable "$LLVM_BINDIR" llvm-ar || true)"
AUTO_LLVM_RANLIB="$(find_first_executable "$LLVM_BINDIR" llvm-ranlib || true)"
AUTO_LLVM_NM="$(find_first_executable "$LLVM_BINDIR" llvm-nm || true)"
AUTO_LLVM_OBJCOPY="$(find_first_executable "$LLVM_BINDIR" llvm-objcopy || true)"
AUTO_LLVM_OBJDUMP="$(find_first_executable "$LLVM_BINDIR" llvm-objdump || true)"
AUTO_LLVM_READELF="$(find_first_executable "$LLVM_BINDIR" llvm-readelf || true)"
AUTO_LLVM_STRIP="$(find_first_executable "$LLVM_BINDIR" llvm-strip || true)"

if [ -z "$AUTO_C_COMPILER" ] || [ -z "$AUTO_CXX_COMPILER" ]; then
    echo -e "${RED}Error: coherent Clang compiler pair not found under $LLVM_BINDIR.${NC}"
    echo "Expected to find clang/clang++ from the same LLVM installation."
    exit 1
fi

if [ -z "$AUTO_LLVM_AR" ] || [ -z "$AUTO_LLVM_RANLIB" ]; then
    echo -e "${RED}Error: coherent LLVM archiver tools not found under $LLVM_BINDIR.${NC}"
    echo "Expected to find llvm-ar and llvm-ranlib from the same LLVM installation."
    exit 1
fi

export PATH="$LLVM_BINDIR${PATH:+:$PATH}"

echo -e "${GREEN}Found LLVM version: $LLVM_VERSION (${LLVM_CONFIG_CMD})${NC}"
echo "LLVM prefix:    $LLVM_PREFIX"
echo "LLVM bindir:    $LLVM_BINDIR"
echo "C compiler:     ${CC:-$AUTO_C_COMPILER}"
echo "C++ compiler:   ${CXX:-$AUTO_CXX_COMPILER}"
echo "Archiver:       $AUTO_LLVM_AR"
echo "Ranlib:         $AUTO_LLVM_RANLIB"
echo "Linker policy:  ${REX_LINKER:-auto}"

CMAKE_GENERATOR_ARGS=()
if [ -n "${REX_CMAKE_GENERATOR:-}" ]; then
    CMAKE_GENERATOR_ARGS=(-G "$REX_CMAKE_GENERATOR")
elif command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR_ARGS=(-G Ninja)
fi

if [ "${#CMAKE_GENERATOR_ARGS[@]}" -gt 0 ]; then
    echo "CMake generator: ${CMAKE_GENERATOR_ARGS[1]}"
else
    echo "CMake generator: CMake default"
fi

ENABLE_FORTRAN_CMAKE=ON
ENABLE_FORTRAN_FLANG_CMAKE=ON
RESOLVED_FLANG_ROOT="${FLANG_ROOT:-$LLVM_PREFIX}"

if [ -n "${FLANG_ROOT:-}" ]; then
    if ! has_flang_libraries "$FLANG_ROOT"; then
        echo -e "${RED}Error: FLANG_ROOT=$FLANG_ROOT does not provide the required libFortran* libraries.${NC}"
        exit 1
    fi
elif has_flang_libraries "$LLVM_PREFIX"; then
    :
else
    ENABLE_FORTRAN_CMAKE=OFF
    ENABLE_FORTRAN_FLANG_CMAKE=OFF
    RESOLVED_FLANG_ROOT=""
    echo -e "${YELLOW}Flang libraries not found under $LLVM_PREFIX.${NC}"
    echo "Fortran support will be disabled for this build."
    echo "Set FLANG_ROOT to an LLVM $LLVM_REQUIRED_MAJOR installation with libFortran* libraries to enable Fortran."
fi

echo "Fortran support: ${ENABLE_FORTRAN_CMAKE}"
if [ -n "$RESOLVED_FLANG_ROOT" ]; then
    echo "Flang root:     $RESOLVED_FLANG_ROOT"
fi
echo ""

# Create and enter build directory
echo -e "${YELLOW}[3/5] Configuring with CMake...${NC}"
if [ -d "$BUILD_DIR" ]; then
    echo "Removing existing build directory..."
    rm -rf "$BUILD_DIR" || { echo -e "${RED}Failed to remove build directory${NC}"; exit 1; }
fi
mkdir -p "$BUILD_DIR" || { echo -e "${RED}Failed to create build directory${NC}"; exit 1; }
cd "$BUILD_DIR" || { echo -e "${RED}Failed to enter build directory${NC}"; exit 1; }

# Configure with CMake (will auto-detect compilers, preferring clang/flang)
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    -DENABLE-C=ON
    -DENABLE-FORTRAN="$ENABLE_FORTRAN_CMAKE"
    -DENABLE-FORTRAN-FLANG="$ENABLE_FORTRAN_FLANG_CMAKE"
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DLLVM_ROOT="$LLVM_PREFIX"
    -DClang_ROOT="$LLVM_PREFIX"
    -DLLVM_REQUIRED_MAJOR="$LLVM_REQUIRED_MAJOR"
    -DREX_LINKER="${REX_LINKER:-auto}"
    -DCMAKE_AR="$AUTO_LLVM_AR"
    -DCMAKE_RANLIB="$AUTO_LLVM_RANLIB"
    -DCMAKE_C_COMPILER_AR="$AUTO_LLVM_AR"
    -DCMAKE_C_COMPILER_RANLIB="$AUTO_LLVM_RANLIB"
    -DCMAKE_CXX_COMPILER_AR="$AUTO_LLVM_AR"
    -DCMAKE_CXX_COMPILER_RANLIB="$AUTO_LLVM_RANLIB"
)

if [ -n "$AUTO_LLVM_NM" ]; then
    CMAKE_ARGS+=(-DCMAKE_NM="$AUTO_LLVM_NM")
fi

if [ -n "$AUTO_LLVM_OBJCOPY" ]; then
    CMAKE_ARGS+=(-DCMAKE_OBJCOPY="$AUTO_LLVM_OBJCOPY")
fi

if [ -n "$AUTO_LLVM_OBJDUMP" ]; then
    CMAKE_ARGS+=(-DCMAKE_OBJDUMP="$AUTO_LLVM_OBJDUMP")
fi

if [ -n "$AUTO_LLVM_READELF" ]; then
    CMAKE_ARGS+=(-DCMAKE_READELF="$AUTO_LLVM_READELF")
fi

if [ -n "$AUTO_LLVM_STRIP" ]; then
    CMAKE_ARGS+=(-DCMAKE_STRIP="$AUTO_LLVM_STRIP")
fi

if [ -n "$RESOLVED_FLANG_ROOT" ]; then
    CMAKE_ARGS+=(-DFLANG_ROOT="$RESOLVED_FLANG_ROOT")
fi

if [ -z "${CC:-}" ] && [ -z "${CXX:-}" ]; then
    CMAKE_ARGS+=(
        -DCMAKE_C_COMPILER="$AUTO_C_COMPILER"
        -DCMAKE_CXX_COMPILER="$AUTO_CXX_COMPILER"
    )
else
    echo "Respecting explicit compiler environment: CC=${CC:-<unset>} CXX=${CXX:-<unset>}"
fi

cmake "${CMAKE_GENERATOR_ARGS[@]}" .. "${CMAKE_ARGS[@]}"

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
