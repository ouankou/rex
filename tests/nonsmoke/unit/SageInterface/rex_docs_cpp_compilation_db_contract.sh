#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 4 ]; then
  echo "usage: $0 generator documentation-driver python fixture-root" >&2
  exit 2
fi

generator=$1
documentation_driver=$2
python=$3
fixture_root=$4

rm -rf "$fixture_root"
mkdir -p "$fixture_root/docs" "$fixture_root/src" "$fixture_root/build" \
  "$fixture_root/output" "$fixture_root/overlay" \
  "$fixture_root/src/c++/include" "$fixture_root/toolchain/include/c++/v1" \
  "$fixture_root/toolchain/bin" \
  "$fixture_root/lib/llvm-22/include/c++/v1" \
  "$fixture_root/lib/llvm-22/lib/clang/22/include" \
  "$fixture_root/usr/include" \
  "$fixture_root/configured-sysroot/usr/include/c++/v1" \
  "$fixture_root/configured-sysroot/usr/include" \
  "$fixture_root/configured-resource/lib/clang/22/include"

selected_compiler="$fixture_root/toolchain/bin/clang++-22"

cat > "$fixture_root/toolchain/bin/fake-clang++-22" <<EOF
#!/bin/sh
saw_sysroot=false
saw_target=false
saw_stdlib=false
saw_gcc_install_dir=false
saw_gcc_triple=false
case " \$* " in
  *" --sysroot=$fixture_root/configured-sysroot "*) saw_sysroot=true ;;
esac
case " \$* " in
  *" -target rex-docs-target "*) saw_target=true ;;
esac
case " \$* " in
  *" -stdlib=libc++ "*) saw_stdlib=true ;;
esac
case " \$* " in
  *" --gcc-install-dir=$fixture_root/configured-gcc-install "*)
    saw_gcc_install_dir=true
    ;;
esac
case " \$* " in
  *" --gcc-triple=rex-docs-gcc "*)
    saw_gcc_triple=true
    ;;
esac
configured=false
if [ "\$saw_sysroot" = true ] && [ "\$saw_target" = true ] && \
   [ "\$saw_stdlib" = true ] && [ "\$saw_gcc_install_dir" = true ] && \
   [ "\$saw_gcc_triple" = true ]; then
  configured=true
fi
case " \$* " in
  *" -x c++ "*)
    if [ "\$configured" = true ]; then
      cat >&2 <<PATHS
#include <...> search starts here:
 $fixture_root/configured-sysroot/usr/include/c++/v1
 $fixture_root/configured-resource/lib/clang/22/include
 $fixture_root/configured-sysroot/usr/include
End of search list.
PATHS
    else
      cat >&2 <<PATHS
#include <...> search starts here:
 $fixture_root/lib/llvm-22/include/c++/v1
 $fixture_root/lib/llvm-22/lib/clang/22/include
 $fixture_root/usr/include
End of search list.
PATHS
    fi
    ;;
  *" -x c "*)
    if [ "\$configured" = true ]; then
      cat >&2 <<PATHS
#include <...> search starts here:
 $fixture_root/configured-resource/lib/clang/22/include
 $fixture_root/configured-sysroot/usr/include
End of search list.
PATHS
    else
      cat >&2 <<PATHS
#include <...> search starts here:
 $fixture_root/lib/llvm-22/lib/clang/22/include
 $fixture_root/usr/include
End of search list.
PATHS
    fi
    ;;
  *)
    echo "unexpected fake compiler arguments: \$*" >&2
    exit 1
    ;;
esac
EOF
chmod +x "$fixture_root/toolchain/bin/fake-clang++-22"

include_classification="$($python "$generator" \
  --print-compiler-include-paths \
  "$fixture_root/toolchain/bin/fake-clang++-22")"
expected_include_classification="libc:$fixture_root/usr/include
stdlib:$fixture_root/lib/llvm-22/include/c++/v1
system:$fixture_root/lib/llvm-22/lib/clang/22/include"
if [ "$include_classification" != "$expected_include_classification" ]; then
  echo "LLVM-hosted libc++ include classification is not exact" >&2
  printf 'expected:\n%s\nactual:\n%s\n' \
    "$expected_include_classification" "$include_classification" >&2
  exit 1
fi

cat > "$fixture_root/build/toolchain_probe_commands.json" <<EOF
[
  {
    "directory": "$fixture_root/src",
    "file": "$fixture_root/src/public.cpp",
    "arguments": ["$fixture_root/toolchain/bin/fake-clang++-22", "--sysroot=$fixture_root/configured-sysroot", "-target", "rex-docs-target", "-stdlib=libc++", "--gcc-install-dir=$fixture_root/configured-gcc-install", "--gcc-triple=rex-docs-gcc", "-c", "$fixture_root/src/public.cpp"]
  }
]
EOF

configured_include_classification="$($python "$generator" \
  --print-compiler-include-paths \
  "$fixture_root/toolchain/bin/fake-clang++-22" \
  --compile-db "$fixture_root/build/toolchain_probe_commands.json")"
expected_configured_include_classification="libc:$fixture_root/configured-sysroot/usr/include
stdlib:$fixture_root/configured-sysroot/usr/include/c++/v1
system:$fixture_root/configured-resource/lib/clang/22/include"
if [ "$configured_include_classification" != \
     "$expected_configured_include_classification" ]; then
  echo "documentation include classification ignored compile-command toolchain flags" >&2
  printf 'expected:\n%s\nactual:\n%s\n' \
    "$expected_configured_include_classification" \
    "$configured_include_classification" >&2
  exit 1
fi

cat > "$fixture_root/build/CMakeCache.txt" <<EOF
CMAKE_CXX_COMPILER:FILEPATH=$selected_compiler
CMAKE_CXX_COMPILER_LAUNCHER:STRING=ccache;--config=$fixture_root/ccache.conf
EOF

cat > "$fixture_root/docs/mrdocs.yml" <<'EOF'
source-root: ..
input:
  - ../src
use-system-stdlib: false
EOF

cat > "$fixture_root/src/public.hpp" <<'EOF'
#pragma once
struct PublicHeaderContract {};
EOF

cat > "$fixture_root/src/public.cpp" <<'EOF'
#include "public.hpp"
EOF

cat > "$fixture_root/src/rex_docs_wrapped_compiler.cpp" <<'EOF'
#include "public.hpp"
EOF

cat > "$fixture_root/src/implementation.c" <<'EOF'
#include <stdatomic.h>
atomic_flag implementation_only_flag = ATOMIC_FLAG_INIT;
EOF

cat > "$fixture_root/build/compile_commands.json" <<EOF
[
  {
    "directory": "$fixture_root/src",
    "file": "$fixture_root/src/public.cpp",
    "arguments": ["/usr/bin/ccache", "--config=$fixture_root/ccache.conf", "$selected_compiler", "-I", "$fixture_root/src", "-I", "$fixture_root/src/c++/include", "-isystem", "$fixture_root/toolchain/include/c++/v1", "-c", "$fixture_root/src/public.cpp"]
  },
  {
    "directory": "$fixture_root/src",
    "file": "$fixture_root/src/rex_docs_wrapped_compiler.cpp",
    "command": "sccache --config $fixture_root/sccache.conf $selected_compiler -I $fixture_root/src -I $fixture_root/src/c++/include -isystem $fixture_root/toolchain/include/c++/v1 -c $fixture_root/src/rex_docs_wrapped_compiler.cpp"
  },
  {
    "directory": "$fixture_root/src",
    "file": "$fixture_root/src/implementation.c",
    "arguments": ["clang", "-c", "$fixture_root/src/implementation.c"]
  }
]
EOF

detected_compiler="$($python "$generator" \
  --print-cmake-cxx-compiler "$fixture_root/build/compile_commands.json")"
if [ "$detected_compiler" != "$selected_compiler" ]; then
  echo "documentation compiler was not read exactly from CMakeCache.txt" >&2
  exit 1
fi

mkdir -p "$fixture_root/build/CMakeFiles"
cat > "$fixture_root/build/CMakeFiles/CMakeCXXCompiler.cmake" <<EOF
set(CMAKE_CXX_COMPILER "$selected_compiler")
EOF
grep -v '^CMAKE_CXX_COMPILER:' "$fixture_root/build/CMakeCache.txt" \
  > "$fixture_root/build/CMakeCache.cmake4.txt"
mv "$fixture_root/build/CMakeCache.cmake4.txt" \
  "$fixture_root/build/CMakeCache.txt"
detected_compiler="$($python "$generator" \
  --print-cmake-cxx-compiler "$fixture_root/build/compile_commands.json")"
if [ "$detected_compiler" != "$selected_compiler" ]; then
  echo "documentation compiler was not read exactly from CMake generated state" >&2
  exit 1
fi

cat >> "$fixture_root/build/CMakeCache.txt" <<EOF
CMAKE_CXX_COMPILER:FILEPATH=$fixture_root/toolchain/bin/different-clang++-22
EOF
if "$python" "$generator" \
  --print-cmake-cxx-compiler "$fixture_root/build/compile_commands.json" \
  >"$fixture_root/mismatched-compiler-stdout" \
  2>"$fixture_root/mismatched-compiler-stderr"; then
  echo "documentation compiler discovery accepted mismatched CMake state" >&2
  exit 1
fi
if ! grep -Fxq \
  "CMake cache/generated C++ compiler state mismatch: $fixture_root/toolchain/bin/different-clang++-22 != $selected_compiler" \
  "$fixture_root/mismatched-compiler-stderr"; then
  echo "mismatched CMake compiler state did not produce the hard diagnostic" >&2
  cat "$fixture_root/mismatched-compiler-stderr" >&2
  exit 1
fi
cat > "$fixture_root/build/CMakeCache.txt" <<EOF
CMAKE_CXX_COMPILER:FILEPATH=$selected_compiler
CMAKE_CXX_COMPILER_LAUNCHER:STRING=ccache;--config=$fixture_root/ccache.conf
EOF

mkdir -p "$fixture_root/cacheless"
cp "$fixture_root/build/compile_commands.json" \
  "$fixture_root/cacheless/compile_commands.json"
if "$python" "$generator" \
  --print-cmake-cxx-compiler \
  "$fixture_root/cacheless/compile_commands.json" \
  >"$fixture_root/cacheless/stdout" 2>"$fixture_root/cacheless/stderr"; then
  echo "documentation compiler discovery accepted a missing CMake cache" >&2
  exit 1
fi
if ! grep -Fxq \
  "documentation compilation database has no adjacent CMakeCache.txt: $fixture_root/cacheless/compile_commands.json" \
  "$fixture_root/cacheless/stderr"; then
  echo "missing CMake cache did not produce the hard diagnostic" >&2
  cat "$fixture_root/cacheless/stderr" >&2
  exit 1
fi

mkdir -p "$fixture_root/compilerless"
cp "$fixture_root/build/compile_commands.json" \
  "$fixture_root/compilerless/compile_commands.json"
cat > "$fixture_root/compilerless/CMakeCache.txt" <<EOF
CMAKE_CXX_COMPILER_LAUNCHER:STRING=ccache;--config=$fixture_root/ccache.conf
EOF
if "$python" "$generator" \
  --print-cmake-cxx-compiler \
  "$fixture_root/compilerless/compile_commands.json" \
  >"$fixture_root/compilerless/stdout" \
  2>"$fixture_root/compilerless/stderr"; then
  echo "documentation compiler discovery accepted missing CMake compiler state" >&2
  exit 1
fi
if ! grep -Fxq \
  "CMake configuration contains no exact CMAKE_CXX_COMPILER in $fixture_root/compilerless/CMakeCache.txt or $fixture_root/compilerless/CMakeFiles/CMakeCXXCompiler.cmake" \
  "$fixture_root/compilerless/stderr"; then
  echo "missing CMake compiler state did not produce the hard diagnostic" >&2
  cat "$fixture_root/compilerless/stderr" >&2
  exit 1
fi

docs_db="$($python "$generator" \
  --config "$fixture_root/docs/mrdocs.yml" \
  --out-dir "$fixture_root/output" \
  --compiler "$selected_compiler" \
  --compile-db "$fixture_root/build/compile_commands.json" \
  --overlay-dir "$fixture_root/overlay" \
  --stdlib-include "$fixture_root/toolchain/include/c++/v1")"

$python - "$docs_db" "$fixture_root/src/public.cpp" \
  "$fixture_root/src/rex_docs_wrapped_compiler.cpp" \
  "$fixture_root/src/c++/include" \
  "$fixture_root/toolchain/include/c++/v1" \
  "$selected_compiler" <<'PY'
import json
import sys
from pathlib import Path

database_path = Path(sys.argv[1]).resolve()
expected_sources = {Path(argument).resolve() for argument in sys.argv[2:4]}
project_include = str(Path(sys.argv[4]).resolve())
stdlib_include = str(Path(sys.argv[5]).resolve())
selected_compiler = str(Path(sys.argv[6]).resolve())
database = json.loads(database_path.read_text(encoding="utf-8"))
if len(database) != len(expected_sources):
    raise SystemExit(
        f"expected {len(expected_sources)} C++ translation units, got {len(database)}"
    )
if {Path(entry["file"]).resolve() for entry in database} != expected_sources:
    raise SystemExit("documentation database did not preserve the real C++ units")
for entry in database:
    arguments = entry["arguments"]
    if arguments[0] != selected_compiler:
        raise SystemExit("documentation database did not install the selected compiler")
    if any(
        Path(argument).name
        in {"buildcache", "ccache", "distcc", "icecc", "sccache"}
        for argument in arguments[1:]
    ):
        raise SystemExit("compiler launcher or replaced compiler remains in command")
    if selected_compiler in arguments[1:]:
        raise SystemExit("original compiler remains after launcher arguments")
    if any(
        argument.startswith("--config") or argument.endswith(".conf")
        for argument in arguments[1:]
    ):
        raise SystemExit("compiler launcher arguments remain in command")
    if "-DROSE_DOCGEN" not in arguments:
        raise SystemExit("documentation database lacks the documentation contract")
    if "-nostdinc++" not in arguments:
        raise SystemExit("documentation database lacks controlled stdlib mode")
    if not any(
        arguments[index : index + 2] == ["-I", project_include]
        for index in range(len(arguments) - 1)
    ):
        raise SystemExit("project include directory named c++ was removed")
    if stdlib_include in arguments:
        raise SystemExit("compiler-owned standard library include was retained")
    if any("implementation.c" in argument for argument in arguments):
        raise SystemExit("C implementation source leaked into the C++ database")
PY

cat > "$fixture_root/build/launcher_without_compiler.json" <<EOF
[
  {
    "directory": "$fixture_root/src",
    "file": "$fixture_root/src/public.cpp",
    "arguments": ["ccache", "--config=$fixture_root/ccache.conf"]
  }
]
EOF

if "$python" "$generator" \
  --config "$fixture_root/docs/mrdocs.yml" \
  --out-dir "$fixture_root/invalid-output" \
  --compiler "$selected_compiler" \
  --compile-db "$fixture_root/build/launcher_without_compiler.json" \
  --stdlib-include "$fixture_root/toolchain/include/c++/v1" \
  >"$fixture_root/invalid-stdout" 2>"$fixture_root/invalid-stderr"; then
  echo "launcher-only compilation command was accepted" >&2
  exit 1
fi
if ! grep -Fxq "compile command does not contain selected compiler: $selected_compiler" \
  "$fixture_root/invalid-stderr"; then
  echo "launcher-only compilation command did not produce the hard diagnostic" >&2
  cat "$fixture_root/invalid-stderr" >&2
  exit 1
fi

if grep -Fq 'filter-compilation-db' "$documentation_driver"; then
  echo "documentation driver still contains a private database implementation" >&2
  exit 1
fi
if ! grep -Fq 'docs_db="$(build-docs-compilation-db' "$documentation_driver"; then
  echo "documentation driver does not use the C++ database builder" >&2
  exit 1
fi
if ! grep -Fq -- '--print-cmake-cxx-compiler "$db"' "$documentation_driver"; then
  echo "documentation driver does not use the exact CMake compiler owner" >&2
  exit 1
fi
