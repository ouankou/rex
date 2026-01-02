# Getting Started

## Build the compiler

REX uses LLVM/Clang 20.x or later. The fastest way to build and install locally is:

```bash
./build-rex.sh "$HOME/rex-install" Release
```

For a manual CMake flow, see the root `BUILDING_WITH_CLANG.md` guide. Be sure that
`llvm-config` from LLVM 20.x or later is on your `PATH`.

## Generate the docs locally

1. Install Python tooling:
   ```bash
   python3 -m venv /tmp/rex-docs-venv
   source /tmp/rex-docs-venv/bin/activate
   pip install -r docs/requirements.txt
   ```
2. Produce the Doxygen XML (Breathe/Exhale consumes it):
   ```bash
   doxygen docs/Doxyfile
   ```
3. Build the HTML site with Sphinx:
   ```bash
   sphinx-build -b html docs/sphinx docs/_build/html
   ```
4. Open `docs/_build/html/index.html` in your browser.

`docs/Doxyfile` is scoped to the `src` tree (headers only) and skips test/build
trees, submodules, and heavy third-party copies to keep the run manageable. The
HTML uses `sphinx-book-theme`.
