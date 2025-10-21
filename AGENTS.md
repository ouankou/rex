# Repository Guidelines

## Project Structure & Module Organization
- `src/`: core compiler sources and frontends; keep new modules close to their owning subsystem and wire them into the nearest `CMakeLists.txt`.
- `tests/`: smoke, regression, and compile-time suites (`roseTests/`, `nonsmoke/`, `CompileTests/`); mirror source layout when adding cases.
- `tools/`: production-ready command-line utilities; update docs alongside behavioral changes.
- `scripts/`: build, policy, and release automation (`build-rex.sh`, `policies/`, packaging helpers); reuse existing hooks instead of duplicating logic.
- `docs/` and `tutorial/`: user and developer reference; refresh these when public interfaces evolve.

## Build, Test, and Development Commands
- Bootstrap with LLVM Clang 20: `./build-rex.sh $HOME/rex-install` (configures, builds, installs).
- Manual flow: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Denable-c=ON -Denable-fortran=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.
- Compile: `cmake --build build -j$(nproc)`; install locally with `cmake --install build`.
- Run regression suites: `ctest --test-dir build --output-on-failure` or `cmake --build build --target check` after configuration.

## Coding Style & Naming Conventions
- Match surrounding indentation (predominantly two-space indents, braces on the following line) and prefer descriptive identifiers (`Sg*` classes, `*_support` helpers).
- Headers should provide unique include guards; avoid tabs, trailing whitespace, and duplicated headers—violations are caught by `scripts/policies`.
- Place ROSE includes before STL/third-party headers, and keep namespaces explicit in headers (no `using namespace` in public headers).

## Testing Guidelines
- Extend existing suites by copying patterns in `tests/roseTests` (C/C++), `tests/nonsmoke` (feature smoke tests), and `tests/CompileTests` (compilation guards); name files after the feature under test (e.g., `testLoopNormalization.C`).
- Always run `ctest` (or targeted `ctest -R <pattern>`) before submitting; Fortran work should also follow the prompts in `FORTRAN_TESTING_GUIDE.md`.

## Commit & Pull Request Guidelines
- Follow the prevailing short imperative subject style (`Fix critical memory errors…`); prefix scope when helpful (e.g., `CI:`).
- Keep commits focused and reference issues in the body (`Refs #1234`) when applicable; avoid large unrelated changes.
- Pull requests should summarize motivation, list key changes, note test coverage, and include logs or screenshots for user-facing updates; link dependent PRs or issues explicitly.

## Policy & Automation Checks
- Run `scripts/policies-checker.sh <path>` before pushing; it enforces line endings, header uniqueness, and other repository rules.
- CI expects dependencies (LLVM/Clang 20) reachable via `llvm-config`; sync submodules with `git submodule update --init --recursive` when touching third-party code.
