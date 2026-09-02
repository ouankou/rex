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
if [ -z "${NUM_JOBS:-}" ]; then
    if ! command -v nproc >/dev/null 2>&1; then
        echo -e "${RED}Error: nproc is required when NUM_JOBS is not set.${NC}" >&2
        exit 1
    fi
    NUM_JOBS="$(nproc)"
fi
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
        if [ -x "$search_dir/$candidate" ]; then
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
    local resolved_real
    local bindir_real
    local version_output
    local major

    resolved="$(command -v "$compiler" 2>/dev/null || true)"
    if [ -z "$resolved" ] || [ ! -x "$resolved" ]; then
        echo -e "${RED}Error: $role compiler is not executable: $compiler${NC}" >&2
        return 1
    fi
    resolved_real="$(readlink -f "$resolved" 2>/dev/null || true)"
    bindir_real="$(readlink -f "$LLVM_BINDIR" 2>/dev/null || true)"
    case "$resolved_real" in
        "$bindir_real"/*) ;;
        *)
            echo -e "${RED}Error: $role compiler '$resolved' is outside the selected LLVM bindir '$LLVM_BINDIR'.${NC}" >&2
            return 1
            ;;
    esac

    version_output="$("$resolved" --version 2>&1)" || {
        echo -e "${RED}Error: cannot execute $role compiler '$resolved': $version_output${NC}" >&2
        return 1
    }
    if ! printf '%s\n' "$version_output" | head -n 1 | grep -qi "$expected_kind"; then
        echo -e "${RED}Error: $role compiler '$resolved' is not LLVM $expected_kind.${NC}" >&2
        return 1
    fi
    major="$(printf '%s\n' "$version_output" | sed -nE 's/.*version[[:space:]]+([0-9]+)([.].*)?/\1/p' | head -n 1)"
    if [ "$major" != "$LLVM_REQUIRED_MAJOR" ]; then
        echo -e "${RED}Error: $role compiler '$resolved' has major '${major:-unknown}'; exactly $LLVM_REQUIRED_MAJOR is required.${NC}" >&2
        return 1
    fi
}

validate_flang_installation() {
    local flang_root="$1"
    local manifest_file="cmake/rex_flang_frontend_manifest.txt"
    local libdir
    local kind
    local name
    local trailing
    local found
    local library_count=0
    local header_count=0

    case "$flang_root" in
        /*) ;;
        *)
            echo -e "${RED}Error: Flang root must be an absolute path: $flang_root${NC}" >&2
            return 1
            ;;
    esac
    if [ ! -f "$manifest_file" ]; then
        echo -e "${RED}Error: REX Flang frontend manifest is missing: $manifest_file${NC}" >&2
        return 1
    fi

    while read -r kind name trailing; do
        [ -n "$kind" ] || continue
        case "$kind" in
            \#*) continue ;;
        esac
        if [ -z "$name" ] || [ -n "$trailing" ]; then
            echo -e "${RED}Error: malformed REX Flang frontend manifest entry: $kind ${name:-}${trailing:+ $trailing}${NC}" >&2
            return 1
        fi
        case "$kind" in
            library)
                library_count=$((library_count + 1))
                found=0
                for libdir in "$flang_root/lib" "$flang_root/lib64"; do
                    if [ -d "$libdir" ] &&
                       compgen -G "$libdir/lib${name}.*" > /dev/null; then
                        found=1
                        break
                    fi
                done
                if [ "$found" -ne 1 ]; then
                    echo -e "${RED}Error: Flang root $flang_root is missing required frontend library lib${name}.${NC}" >&2
                    return 1
                fi
                ;;
            header)
                header_count=$((header_count + 1))
                if [ ! -f "$flang_root/include/$name" ]; then
                    echo -e "${RED}Error: Flang root $flang_root is missing required frontend header include/$name.${NC}" >&2
                    return 1
                fi
                ;;
            *)
                echo -e "${RED}Error: unknown REX Flang frontend manifest kind '$kind'.${NC}" >&2
                return 1
                ;;
        esac
    done < "$manifest_file"

    if [ "$library_count" -eq 0 ] || [ "$header_count" -eq 0 ]; then
        echo -e "${RED}Error: REX Flang frontend manifest must declare libraries and headers.${NC}" >&2
        return 1
    fi
}

find_llvm_config() {
    local candidates=()
    local candidate
    local version
    local major
    local prefix
    local libdir
    local bindir
    local config_dir
    local config_real
    local selected_root=""
    local derived_root=""
    local explicit_selection=0

    append_candidate() {
        local entry="$1"
        [ -n "$entry" ] || return 0
        candidates+=("$entry")
    }

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
        if [ ! -x "$LLVM_CONFIG" ]; then
            echo "Error: LLVM_CONFIG is not executable: $LLVM_CONFIG" >&2
            return 1
        fi
        append_candidate "$(readlink -f "$LLVM_CONFIG")"
    elif [ -n "$selected_root" ]; then
        append_candidate "$selected_root/bin/llvm-config"
        append_candidate "$selected_root/bin/llvm-config-$LLVM_REQUIRED_MAJOR"
        if [ ! -x "$selected_root/bin/llvm-config" ] &&
           [ ! -x "$selected_root/bin/llvm-config-$LLVM_REQUIRED_MAJOR" ]; then
            echo "Error: selected LLVM root has no executable llvm-config: $selected_root" >&2
            return 1
        fi
    else
        if command -v llvm-config >/dev/null 2>&1; then
            append_candidate "$(command -v llvm-config)"
        fi
        if command -v "llvm-config-$LLVM_REQUIRED_MAJOR" >/dev/null 2>&1; then
            append_candidate "$(command -v "llvm-config-$LLVM_REQUIRED_MAJOR")"
        fi
    fi

    local seen=""
    for candidate in "${candidates[@]}"; do
        [ -x "$candidate" ] || continue
        case " $seen " in
            *" $candidate "*) continue ;;
        esac
        seen="$seen $candidate"
        version=$("$candidate" --version 2>/dev/null || true)
        major=$(echo "$version" | sed -nE 's/^([0-9]+).*/\1/p')
        bindir=$("$candidate" --bindir 2>/dev/null || true)
        prefix=$("$candidate" --prefix 2>/dev/null || true)
        libdir=$("$candidate" --libdir 2>/dev/null || true)
        if [ -n "$selected_root" ] && [ -n "$prefix" ]; then
            derived_root="$(cd "$prefix" 2>/dev/null && pwd -P || true)"
            if [ "$derived_root" != "$selected_root" ]; then
                if [ "$explicit_selection" -eq 1 ]; then
                    echo "Error: selected llvm-config reports prefix '$prefix', not explicit LLVM root '$selected_root'." >&2
                fi
                continue
            fi
        fi
        if [ -n "$major" ] && [ "$major" -eq "$LLVM_REQUIRED_MAJOR" ] &&
           [ -n "$prefix" ] &&
           [ "$bindir" = "$prefix/bin" ] &&
           { [ "$libdir" = "$prefix/lib" ] || [ "$libdir" = "$prefix/lib64" ]; } &&
           [ -n "$bindir" ] &&
           [ -x "$bindir/llvm-ar" ] &&
           [ -x "$(find_first_executable "$bindir" "clang-$LLVM_REQUIRED_MAJOR" clang || true)" ] &&
           [ -x "$(find_first_executable "$bindir" "clang++-$LLVM_REQUIRED_MAJOR" clang++ || true)" ] &&
           [ -x "$(find_first_executable "$bindir" "flang-$LLVM_REQUIRED_MAJOR" flang || true)" ]; then
            printf '%s\n' "$candidate"
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
    if git submodule status | grep -q '^-'; then
        git submodule update --init --recursive
        echo -e "${GREEN}Submodules initialized.${NC}"
    else
        echo -e "${GREEN}Submodules already initialized.${NC}"
    fi
else
    echo -e "${YELLOW}Git metadata not found; assuming submodules are already present.${NC}"
fi
echo ""

# Check for LLVM/Clang
echo -e "${YELLOW}[2/5] Checking for LLVM/Clang installation...${NC}"
LLVM_CONFIG_CMD="$(find_llvm_config || true)"
if [ -z "$LLVM_CONFIG_CMD" ]; then
    echo -e "${RED}Error: llvm-config not found. Please install exactly LLVM/Clang $LLVM_REQUIRED_MAJOR.${NC}"
    echo "On Ubuntu/Debian: sudo apt-get install llvm-$LLVM_REQUIRED_MAJOR llvm-$LLVM_REQUIRED_MAJOR-dev clang-$LLVM_REQUIRED_MAJOR libclang-$LLVM_REQUIRED_MAJOR-dev flang-$LLVM_REQUIRED_MAJOR libflang-$LLVM_REQUIRED_MAJOR-dev lld-$LLVM_REQUIRED_MAJOR mold"
    exit 1
fi

LLVM_VERSION=$($LLVM_CONFIG_CMD --version)
LLVM_MAJOR=$(echo "$LLVM_VERSION" | sed -nE 's/^([0-9]+).*/\1/p')
if [ "$LLVM_MAJOR" != "$LLVM_REQUIRED_MAJOR" ]; then
    echo -e "${RED}Error: detected LLVM version $LLVM_VERSION using '$LLVM_CONFIG_CMD'. REX requires exactly LLVM/Clang $LLVM_REQUIRED_MAJOR.${NC}"
    exit 1
fi
LLVM_BINDIR=$($LLVM_CONFIG_CMD --bindir)
LLVM_PREFIX=$($LLVM_CONFIG_CMD --prefix)
LLVM_LIBDIR=$($LLVM_CONFIG_CMD --libdir)
if [ "$LLVM_BINDIR" != "$LLVM_PREFIX/bin" ] ||
   [ "$LLVM_LIBDIR" != "$LLVM_PREFIX/lib" -a "$LLVM_LIBDIR" != "$LLVM_PREFIX/lib64" ]; then
    echo -e "${RED}Error: selected llvm-config reports a split installation: prefix=$LLVM_PREFIX bindir=$LLVM_BINDIR libdir=$LLVM_LIBDIR.${NC}" >&2
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
validate_llvm_compiler "$EFFECTIVE_C_COMPILER" clang C
validate_llvm_compiler "$EFFECTIVE_CXX_COMPILER" clang C++
validate_llvm_compiler "$EFFECTIVE_FORTRAN_COMPILER" flang Fortran

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

ENABLE_FORTRAN_CMAKE=ON
ENABLE_FORTRAN_FLANG_CMAKE=ON
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

if [ -n "$RESOLVED_FLANG_ROOT" ]; then
    CMAKE_ARGS+=(-DFLANG_ROOT="$RESOLVED_FLANG_ROOT")
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
