# REX (ROSE Compiler Modernization) Context

## Project Overview
REX is a modernization and cleanup effort of the ROSE compiler infrastructure. It is an open-source compiler infrastructure to build source-to-source program transformation and analysis tools for large-scale applications (C, C++, Fortran, OpenMP, etc.).

**Key Differentiator:** REX exclusively uses the Clang/LLVM frontend (specifically LLVM 22) for C/C++ analysis, moving away from the legacy frontend.

## Directory Structure
*   **`src/`**: Core compiler source code.
    *   `frontend/`: Language frontends (Clang integration).
    *   `midend/`: Analysis and transformation passes (loops, dataflow, etc.).
    *   `backend/`: Code generation (unparsers).
    *   `ROSETTA/`: IR generation tools.
*   **`tests/`**: Test suites.
    *   `smoke/`: Quick verification tests.
    *   `nonsmoke/`: Comprehensive regression tests.
*   **`tools/`**: Production-ready utilities.
*   **`scripts/`**: Build and maintenance scripts.

## Build & Installation
**Primary Build Method:** CMake (via `build-rex.sh`)
**Strict Requirement:** LLVM/Clang 22 must be installed and findable (`llvm-config`).

### Quick Start (Preferred)
```bash
# Configures, builds, and installs to $HOME/rex-install
# Usage: ./build-rex.sh [install_prefix] [build_type]
./build-rex.sh $HOME/rex-install Release
```

### Manual CMake Workflow
```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$HOME/rex-install \
    -DENABLE-C=ON \
    -DENABLE-FORTRAN=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build . -j$(nproc)
cmake --install .
```

## Development Guidelines
*   **Coding Style:** Adhere to `clang-format` default style.
    *   Identifiers: Descriptive, often using `Sg` prefix for IR nodes (Sage).
*   **Testing:**
    *   Run regression tests: `ctest --test-dir build --output-on-failure`
    *   Run specific tests: `ctest -R <regex>`
    *   New tests: Add to `tests/nonsmoke`, mirroring source layout.
    *   New REX-originated test specimens must use `rex_<area>_<behavior>.<ext>` names, with lowercase snake_case for the area and behavior terms. Preserve imported, upstream, or vendor test names unchanged.
*   **Formatting:**
    *   Run `clang-format` on changed files before committing to ensure adherence to formatting requirements.

## Critical Notes for AI Agents
*   **Bug Fix Strategy:** Prioritize fixes in the Clang Frontend (CFE). Do not modify other core components unless absolutely necessary to resolve the root cause. Always implement long-term fixes for the root cause, avoiding per-header/function/string workarounds, hacks, or fallbacks.
*   **Test Integrity:** **NEVER** modify tests or the test framework to mask failures. Fix the compiler, not the test.
*   **Assertions & Error Handling:** Do not remove or soften hard assertions (`ROSE_ASSERT`) unless they are clearly proven to be design errors or temporary placeholders. When an error occurs, expose it hard and as early as possible; do not use fallbacks that silently carry the error.
*   **Build System:** **IGNORE** `Makefile.am` and `configure.ac` unless explicitly checking for legacy compatibility. Focus entirely on `CMakeLists.txt` and `build-rex.sh`.
*   **Clang Version:** The project is pinned to LLVM/Clang 22. Do not attempt to fix build errors by downgrading; fix the code to match the modern API.
*   **ROSE Upstream Sync:** For any ROSE/LLNL upstream sync task, use the repo-local skill at `.codex/skills/rose-upstream-sync/SKILL.md`. Upstream ROSE is a read-only source: fetch/log/show/diff only; never push, open PRs/issues/comments, request reviews, or mention upstream maintainers. Never restore abandoned REX components such as EDG, OFP/old Fortran parser, binary analysis, Sawyer, CodeThorn, Ada, Jovial, Java/ECJ, PHP, JavaScript, Python, UPC, X10, Csharp, Matlab, legacy autotools/Tup paths, or dropped OpenMP designs.
*   **Documentation:** Refer to `AGENTS.md` for the most up-to-date specific instructions for this cleanup branch.
