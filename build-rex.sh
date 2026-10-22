#!/bin/bash
#
# REX Build Script with Clang Frontend
# This script automates the build process for REX compiler
# with the LLVM 22 Clang and Flang frontends enabled.
#
# Usage:
#   ./build-rex.sh [install-prefix] [build-type]
#
# Example:
#   ./build-rex.sh $HOME/rex-install RelWithDebInfo
#

set -e  # Exit on error

if [ "$#" -gt 2 ]; then
    echo "Usage: $0 [install-prefix] [build-type]" >&2
    exit 2
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_PREFIX="${1:-$HOME/rex-install}"
BUILD_TYPE="${2:-RelWithDebInfo}"
BUILD_DIR="build"
case "$INSTALL_PREFIX" in
    /*) ;;
    *)
        echo -e "${RED}Error: install prefix must be absolute: $INSTALL_PREFIX${NC}" >&2
        exit 2
        ;;
esac
if [ "$INSTALL_PREFIX" = "/" ]; then
    echo -e "${RED}Error: install prefix must not be the filesystem root.${NC}" >&2
    exit 2
fi
case "$BUILD_TYPE" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *)
        echo -e "${RED}Error: unsupported CMake build type '$BUILD_TYPE'.${NC}" >&2
        exit 2
        ;;
esac
LLVM_REQUIRED_MAJOR="${LLVM_REQUIRED_MAJOR:-22}"
if [ "$LLVM_REQUIRED_MAJOR" != "22" ]; then
    echo -e "${RED}Error: REX is pinned to LLVM/Clang major 22; LLVM_REQUIRED_MAJOR='$LLVM_REQUIRED_MAJOR' is unsupported.${NC}" >&2
    exit 1
fi
NUM_JOBS="${NUM_JOBS:-1}"
if ! [[ "$NUM_JOBS" =~ ^[0-9]+$ ]] || [ "$NUM_JOBS" -lt 1 ]; then
    echo -e "${RED}Error: NUM_JOBS must be a positive integer, got '$NUM_JOBS'.${NC}" >&2
    exit 1
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
        if [ -f "$search_dir/$candidate" ] && [ -x "$search_dir/$candidate" ]; then
            printf '%s\n' "$search_dir/$candidate"
            return 0
        fi
    done
    return 1
}

validate_llvm_compiler() {
    local compiler="$1"
    local expected_kind="$2"
    local role="$3"
    local resolved
    local resolved_dir
    local resolved_real
    local bindir_real
    local version_output
    local version_line
    local major

    resolved="$(command -v "$compiler" 2>/dev/null || true)"
    if [ -z "$resolved" ] || [ ! -f "$resolved" ] || [ ! -x "$resolved" ]; then
        echo -e "${RED}Error: $role compiler is not executable: $compiler${NC}" >&2
        return 1
    fi
    if ! resolved_real="$(readlink -f "$resolved")" ||
       ! bindir_real="$(readlink -f "$LLVM_BINDIR")"; then
        echo "Error: cannot resolve $role compiler or LLVM bindir identity." >&2
        return 1
    fi
    if [ "${resolved_real%/*}" != "$bindir_real" ]; then
        echo -e "${RED}Error: $role compiler '$resolved' is outside the selected LLVM bindir '$LLVM_BINDIR'.${NC}" >&2
        return 1
    fi

    version_output="$("$resolved" --version 2>&1)" || {
        echo -e "${RED}Error: cannot execute $role compiler '$resolved': $version_output${NC}" >&2
        return 1
    }
    version_line="${version_output%%$'\n'*}"
    case "$expected_kind:$version_line" in
        clang:*[cC][lL][aA][nN][gG]*|flang:*[fF][lL][aA][nN][gG]*) ;;
        *)
            echo -e "${RED}Error: $role compiler '$resolved' is not LLVM $expected_kind.${NC}" >&2
            return 1
            ;;
    esac
    if [[ ! "$version_line" =~ version[[:space:]]+([0-9]+)([.]|[[:space:]]|$) ]]; then
        echo -e "${RED}Error: $role compiler '$resolved' reports no valid version: $version_line${NC}" >&2
        return 1
    fi
    major="${BASH_REMATCH[1]}"
    if [ "$major" != "$LLVM_REQUIRED_MAJOR" ]; then
        echo -e "${RED}Error: $role compiler '$resolved' has major '${major:-unknown}'; exactly $LLVM_REQUIRED_MAJOR is required.${NC}" >&2
        return 1
    fi
    resolved_dir="$(cd "$(dirname "$resolved")" && pwd -P)" || return 1
    printf '%s/%s\n' "$resolved_dir" "${resolved##*/}"
}

validate_flang_installation() {
    cmake "-DREX_FLANG_ROOT=$1" -P cmake/rex_check_flang_installation.cmake
}

is_required_llvm_version() {
    [[ "$1" =~ ^([0-9]+)\.[0-9]+\.[0-9]+[A-Za-z0-9.+-]*$ ]] &&
        [[ "${BASH_REMATCH[1]}" == "$LLVM_REQUIRED_MAJOR" ]]
}

find_llvm_config() {
    local candidates=()
    local candidate
    local version
    local prefix
    local libdir
    local bindir
    local config_dir
    local config_real
    local selected_root=""
    local derived_root=""
    local explicit_selection=0

    if [ -n "${LLVM_ROOT:-}" ]; then
        explicit_selection=1
        case "$LLVM_ROOT" in
            /*) ;;
            *)
                echo "Error: LLVM_ROOT must be absolute: $LLVM_ROOT" >&2
                return 1
                ;;
        esac
        if [ ! -d "$LLVM_ROOT" ] ||
           { [ ! -f "$LLVM_ROOT/lib/cmake/llvm/LLVMConfig.cmake" ] &&
             [ ! -f "$LLVM_ROOT/lib64/cmake/llvm/LLVMConfig.cmake" ]; }; then
            echo "Error: LLVM_ROOT is not an LLVM package root: $LLVM_ROOT" >&2
            return 1
        fi
        selected_root="$(cd "$LLVM_ROOT" && pwd -P)"
    fi
    if [ -n "${LLVM_DIR:-}" ]; then
        explicit_selection=1
        case "$LLVM_DIR" in
            /*) ;;
            *)
                echo "Error: LLVM_DIR must be absolute: $LLVM_DIR" >&2
                return 1
                ;;
        esac
        if [ ! -f "$LLVM_DIR/LLVMConfig.cmake" ]; then
            echo "Error: LLVM_DIR does not contain LLVMConfig.cmake: $LLVM_DIR" >&2
            return 1
        fi
        config_real="$(readlink -f "$LLVM_DIR/LLVMConfig.cmake")"
        if [ -z "$config_real" ] || [ ! -f "$config_real" ]; then
            echo "Error: cannot canonicalize LLVMConfig.cmake in LLVM_DIR: $LLVM_DIR" >&2
            return 1
        fi
        config_dir="${config_real%/*}"
        derived_root="$(cd "$config_dir/../../.." && pwd -P)"
        if [ -n "$selected_root" ] && [ "$selected_root" != "$derived_root" ]; then
            echo "Error: LLVM_ROOT and LLVM_DIR select different installations: $selected_root and $derived_root" >&2
            return 1
        fi
        selected_root="$derived_root"
    fi

    if [ -n "${LLVM_CONFIG:-}" ]; then
        explicit_selection=1
        case "$LLVM_CONFIG" in
            /*) ;;
            *)
                echo "Error: LLVM_CONFIG must be an absolute executable path: $LLVM_CONFIG" >&2
                return 1
                ;;
        esac
        if [ ! -f "$LLVM_CONFIG" ] || [ ! -x "$LLVM_CONFIG" ]; then
            echo "Error: LLVM_CONFIG is not executable: $LLVM_CONFIG" >&2
            return 1
        fi
        candidates+=("$(readlink -f "$LLVM_CONFIG")")
    elif [ -n "$selected_root" ]; then
        candidates+=("$selected_root/bin/llvm-config"
                     "$selected_root/bin/llvm-config-$LLVM_REQUIRED_MAJOR")
        if ! find_first_executable "$selected_root/bin" llvm-config \
                "llvm-config-$LLVM_REQUIRED_MAJOR" >/dev/null; then
            echo "Error: selected LLVM root has no executable llvm-config: $selected_root" >&2
            return 1
        fi
    else
        if command -v llvm-config >/dev/null 2>&1; then
            candidates+=("$(command -v llvm-config)")
        fi
        if command -v "llvm-config-$LLVM_REQUIRED_MAJOR" >/dev/null 2>&1; then
            candidates+=("$(command -v "llvm-config-$LLVM_REQUIRED_MAJOR")")
        fi
    fi

    for candidate in "${candidates[@]}"; do
        [ -f "$candidate" ] && [ -x "$candidate" ] || continue
        if ! version=$("$candidate" --version) ||
           ! bindir=$("$candidate" --bindir) ||
           ! prefix=$("$candidate" --prefix) ||
           ! libdir=$("$candidate" --libdir); then
            if [ "$explicit_selection" -eq 1 ]; then
                echo "Error: selected llvm-config query failed: $candidate" >&2
                return 1
            fi
            continue
        fi
        if ! is_required_llvm_version "$version"; then
            if [ "$explicit_selection" -eq 1 ]; then
                echo "Error: selected llvm-config reports invalid or unsupported version '$version': $candidate" >&2
                return 1
            fi
            continue
        fi
        if [ -n "$selected_root" ] && [ -n "$prefix" ]; then
            derived_root="$(cd "$prefix" 2>/dev/null && pwd -P || true)"
            if [ "$derived_root" != "$selected_root" ]; then
                if [ "$explicit_selection" -eq 1 ]; then
                    echo "Error: selected llvm-config reports prefix '$prefix', not explicit LLVM root '$selected_root'." >&2
                fi
                continue
            fi
        fi
        if [ -n "$prefix" ] &&
           [ "$bindir" = "$prefix/bin" ] &&
           { [ "$libdir" = "$prefix/lib" ] || [ "$libdir" = "$prefix/lib64" ]; } &&
           [ -n "$bindir" ] &&
           find_first_executable "$bindir" llvm-ar >/dev/null &&
           find_first_executable "$bindir" \
               "clang-$LLVM_REQUIRED_MAJOR" clang >/dev/null &&
           find_first_executable "$bindir" \
               "clang++-$LLVM_REQUIRED_MAJOR" clang++ >/dev/null &&
           find_first_executable "$bindir" \
               "flang-$LLVM_REQUIRED_MAJOR" flang >/dev/null; then
            LLVM_CONFIG_CMD="$candidate"
            LLVM_VERSION="$version"
            LLVM_BINDIR="$bindir"
            LLVM_PREFIX="$prefix"
            LLVM_LIBDIR="$libdir"
            return 0
        fi
    done
    if [ "$explicit_selection" -eq 1 ]; then
        echo "Error: the explicit LLVM selector does not provide one coherent LLVM/Clang/Flang $LLVM_REQUIRED_MAJOR installation." >&2
    fi
    return 1
}

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
    if ! submodule_status=$(git submodule status --recursive); then
        echo -e "${RED}Error: unable to inspect git submodules.${NC}" >&2
        exit 1
    fi
    if [[ "$submodule_status" == -* || "$submodule_status" == *$'\n-'* ]]; then
        git submodule update --init --recursive
        echo -e "${GREEN}Submodules initialized.${NC}"
    else
        echo -e "${GREEN}Submodules already initialized.${NC}"
    fi
    unset submodule_status
else
    echo -e "${YELLOW}Git metadata not found; assuming submodules are already present.${NC}"
fi
echo ""

# Check for LLVM/Clang
echo -e "${YELLOW}[2/5] Checking for LLVM/Clang installation...${NC}"
if ! find_llvm_config; then
    echo -e "${RED}Error: llvm-config not found. Please install exactly LLVM/Clang $LLVM_REQUIRED_MAJOR.${NC}"
    echo "On Ubuntu/Debian: sudo apt-get install llvm-$LLVM_REQUIRED_MAJOR llvm-$LLVM_REQUIRED_MAJOR-dev clang-$LLVM_REQUIRED_MAJOR libclang-$LLVM_REQUIRED_MAJOR-dev flang-$LLVM_REQUIRED_MAJOR libflang-$LLVM_REQUIRED_MAJOR-dev lld-$LLVM_REQUIRED_MAJOR mold"
    exit 1
fi

AUTO_C_COMPILER="$(find_first_executable "$LLVM_BINDIR" "clang-$LLVM_REQUIRED_MAJOR" clang || true)"
AUTO_CXX_COMPILER="$(find_first_executable "$LLVM_BINDIR" "clang++-$LLVM_REQUIRED_MAJOR" clang++ || true)"
AUTO_FORTRAN_COMPILER="$(find_first_executable "$LLVM_BINDIR" "flang-$LLVM_REQUIRED_MAJOR" flang || true)"
AUTO_LLVM_AR="$(find_first_executable "$LLVM_BINDIR" llvm-ar || true)"
AUTO_LLVM_RANLIB="$(find_first_executable "$LLVM_BINDIR" llvm-ranlib || true)"
AUTO_LLVM_NM="$(find_first_executable "$LLVM_BINDIR" llvm-nm || true)"
AUTO_LLVM_OBJCOPY="$(find_first_executable "$LLVM_BINDIR" llvm-objcopy || true)"
AUTO_LLVM_OBJDUMP="$(find_first_executable "$LLVM_BINDIR" llvm-objdump || true)"
AUTO_LLVM_READELF="$(find_first_executable "$LLVM_BINDIR" llvm-readelf || true)"
AUTO_LLVM_STRIP="$(find_first_executable "$LLVM_BINDIR" llvm-strip || true)"

if [ -z "$AUTO_C_COMPILER" ] || [ -z "$AUTO_CXX_COMPILER" ] || [ -z "$AUTO_FORTRAN_COMPILER" ]; then
    echo -e "${RED}Error: coherent Clang/Flang compiler set not found under $LLVM_BINDIR.${NC}"
    echo "Expected to find clang, clang++, and flang from the same LLVM installation."
    exit 1
fi

if [ -z "$AUTO_LLVM_AR" ] || [ -z "$AUTO_LLVM_RANLIB" ]; then
    echo -e "${RED}Error: coherent LLVM archiver tools not found under $LLVM_BINDIR.${NC}"
    echo "Expected to find llvm-ar and llvm-ranlib from the same LLVM installation."
    exit 1
fi

export PATH="$LLVM_BINDIR${PATH:+:$PATH}"
export LD_LIBRARY_PATH="$LLVM_LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export CMAKE_PREFIX_PATH="$LLVM_PREFIX${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"

EFFECTIVE_C_COMPILER="${CC:-$AUTO_C_COMPILER}"
EFFECTIVE_CXX_COMPILER="${CXX:-$AUTO_CXX_COMPILER}"
EFFECTIVE_FORTRAN_COMPILER="${FC:-$AUTO_FORTRAN_COMPILER}"
EFFECTIVE_C_COMPILER="$(validate_llvm_compiler "$EFFECTIVE_C_COMPILER" clang C)"
EFFECTIVE_CXX_COMPILER="$(validate_llvm_compiler "$EFFECTIVE_CXX_COMPILER" clang C++)"
EFFECTIVE_FORTRAN_COMPILER="$(validate_llvm_compiler "$EFFECTIVE_FORTRAN_COMPILER" flang Fortran)"

echo -e "${GREEN}Found LLVM version: $LLVM_VERSION (${LLVM_CONFIG_CMD})${NC}"
echo "LLVM prefix:    $LLVM_PREFIX"
echo "LLVM bindir:    $LLVM_BINDIR"
echo "C compiler:     $EFFECTIVE_C_COMPILER"
echo "C++ compiler:   $EFFECTIVE_CXX_COMPILER"
echo "Fortran compiler: $EFFECTIVE_FORTRAN_COMPILER"
echo "Archiver:       $AUTO_LLVM_AR"
echo "Ranlib:         $AUTO_LLVM_RANLIB"

REX_LINKER="${REX_LINKER:-auto}"
case "$REX_LINKER" in
    none)
        REX_LINKER=system
        ;;
    auto|system|lld|mold|bfd)
        ;;
    *)
        echo -e "${RED}Error: unsupported REX_LINKER='$REX_LINKER'. Use auto, lld, mold, bfd, or system.${NC}"
        exit 1
        ;;
esac
echo "Linker policy:  REX_LINKER=$REX_LINKER (resolved by CMake)"

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

RESOLVED_FLANG_ROOT="${FLANG_ROOT:-$LLVM_PREFIX}"

if [ "$(readlink -f "$RESOLVED_FLANG_ROOT" 2>/dev/null || true)" != \
     "$(readlink -f "$LLVM_PREFIX" 2>/dev/null || true)" ]; then
    echo -e "${RED}Error: FLANG_ROOT must be the selected LLVM installation prefix: $LLVM_PREFIX${NC}" >&2
    exit 1
fi

if ! validate_flang_installation "$RESOLVED_FLANG_ROOT"; then
    echo -e "${RED}Error: build-rex.sh requires the complete LLVM/Flang $LLVM_REQUIRED_MAJOR frontend installation.${NC}" >&2
    echo "Install the matching Flang development package or set FLANG_ROOT to its absolute installation prefix." >&2
    exit 1
fi

echo "Fortran support: ON"
echo "Flang root:     $RESOLVED_FLANG_ROOT"
echo ""

# Create and enter build directory
echo -e "${YELLOW}[3/5] Configuring with CMake...${NC}"
mkdir -p "$BUILD_DIR" || { echo -e "${RED}Failed to create build directory${NC}"; exit 1; }
cd "$BUILD_DIR" || { echo -e "${RED}Failed to enter build directory${NC}"; exit 1; }

CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
    -DENABLE-C=ON
    -DENABLE-FORTRAN=ON
    -DCMAKE_CXX_STANDARD=17
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DLLVM_ROOT="$LLVM_PREFIX"
    -DFLANG_ROOT="$RESOLVED_FLANG_ROOT"
    -DClang_ROOT="$LLVM_PREFIX"
    -DLLVM_REQUIRED_MAJOR="$LLVM_REQUIRED_MAJOR"
    -DREX_LINKER="$REX_LINKER"
    -DCMAKE_AR="$AUTO_LLVM_AR"
    -DCMAKE_RANLIB="$AUTO_LLVM_RANLIB"
    -DCMAKE_C_COMPILER_AR="$AUTO_LLVM_AR"
    -DCMAKE_C_COMPILER_RANLIB="$AUTO_LLVM_RANLIB"
    -DCMAKE_CXX_COMPILER_AR="$AUTO_LLVM_AR"
    -DCMAKE_CXX_COMPILER_RANLIB="$AUTO_LLVM_RANLIB"
    -DCMAKE_C_COMPILER="$EFFECTIVE_C_COMPILER"
    -DCMAKE_CXX_COMPILER="$EFFECTIVE_CXX_COMPILER"
    -DCMAKE_Fortran_COMPILER="$EFFECTIVE_FORTRAN_COMPILER"
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

cmake "${CMAKE_GENERATOR_ARGS[@]}" .. "${CMAKE_ARGS[@]}"

echo -e "${GREEN}CMake configuration successful.${NC}"
echo ""

# Build
echo -e "${YELLOW}[4/5] Building REX (this may take a while)...${NC}"
cmake --build . -j "$NUM_JOBS"

echo -e "${GREEN}Build successful.${NC}"
echo ""

# Install
echo -e "${YELLOW}[5/5] Installing to $INSTALL_PREFIX...${NC}"
cmake --install .

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
