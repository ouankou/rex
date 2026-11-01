# Build the documentation

The reference is generated from REX sources and generated AST headers with MrDocs, then rendered by Zensical.

From a configured REX build:

```bash
bash scripts/ci-docs-build
```

This checks the documentation contracts, builds the generated headers, renders the site, and validates local links and payload size.
The compiler itself does not need a full rebuild just to generate documentation.

To rebuild and serve the site locally:

```bash
bash scripts/serve-docs
```

To keep output under the build directory:

```bash
DOCS_SCRATCH_DIR=build/docs-scratch bash scripts/ci-docs-build
```

[Build provenance](docs-build-manifest.json) records the source revision and tool versions for the published site.
