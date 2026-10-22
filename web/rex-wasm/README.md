# REX WASM static app

This directory contains the browser-only UI for the REX WebAssembly demo.
The build script copies these files beside `rex_wasm.js`, `rex_wasm.wasm`, and
`rex_wasm.data` to produce a static directory that can be published by GitHub
Pages or any other static host.

Build the full static site with:

```bash
scripts/ci-rex-wasm-build-dist
```

The demo intentionally supports only one source file and ships one built-in
OpenMP GPU offloading example. It exposes three modes:

- plain C/C++ round trip
- OpenMP AST-only
- OpenMP lowering

All modes route through REX with `-rose:skipfinalCompileStep`; the browser app
only displays generated source files and the REX log.

The published browser package intentionally omits the Emscripten sysroot to keep
the static site small. Use self-contained C/C++ examples; OpenMP pragma parsing
and the bundled `omp.h` resource are still available.

Source filenames must have a supported suffix: `.c` for C, or `.C`, `.cc`,
`.cp`, `.cpp`, `.CPP`, `.cxx`, and `.c++` for C++. Language selection uses the
original basename before storage sanitization. Unsupported or empty names and
embedded NUL characters produce an explicit failed response before parsing;
they are not assigned a default language. Spaces in supported filenames are
sanitized for storage, and leading dashes are treated as filename characters.
