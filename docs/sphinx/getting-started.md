# Getting Started

## Build the compiler

REX uses LLVM/Clang 20. The fastest way to build and install locally is:

```bash
./build-rex.sh "$HOME/rex-install" Release
```

For a manual CMake flow, see the root `BUILDING_WITH_CLANG.md` guide. Be sure that
`llvm-config` from LLVM 20 is on your `PATH`.

## Generate the docs locally

1. Install Python tooling:
   ```bash
   python3 -m venv /tmp/rex-docs-venv
   source /tmp/rex-docs-venv/bin/activate
   pip install -r docs/requirements.txt
   ```
2. Produce the Doxygen XML (Breathe/Exhale consumes it):
   ```bash
   # User-facing surface (default)
   doxygen docs/Doxyfile

   # Developer/full surface (optional)
   doxygen docs/Doxyfile.dev
   ```
3. Build the HTML site with Sphinx:
   ```bash
   # User docs (default)
   sphinx-build -b html docs/sphinx docs/_build/html

   # Developer docs (point Sphinx at the dev XML)
   REX_DOXY_VARIANT=dev sphinx-build -b html docs/sphinx docs/_build/html-dev
   ```
4. Open `docs/_build/html/index.html` in your browser.

`docs/Doxyfile` is scoped to the `src`, `tools`, `tutorial`, and `exampleTranslators`
directories and skips test/build trees to keep the run lean. `docs/Doxyfile.dev`
exposes the full surface (private/internal) and is noisier/slow; use it only when
you need the exhaustive view.
