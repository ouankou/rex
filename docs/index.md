# REX Developer Documentation

This site documents the REX source tree for developers: build notes and a generated API reference (including private members).

## Jump To

| What | Where |
| --- | --- |
| Find an API by name | [API Quick Lookup](api-lookup.md) |
| Browse API by category | [API Reference Index](reference/index.adoc) |
| Browse namespaces | [Namespaces](reference/sections/namespaces.adoc) |
| Browse types | [Types](reference/sections/types.adoc) |
| Browse functions | [Functions](reference/sections/functions.adoc) |

## Build Metadata

- [docs-build-manifest.json](/docs-build-manifest.json)

## Build Locally

```bash
bash scripts/ci-docs-build
```

Serve the generated website to your LAN on port 8080:

```bash
bash scripts/serve-docs
```
