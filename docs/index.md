# REX developer documentation

Build notes and a generated API reference for the REX compiler source tree.

<section class="api-lookup" aria-label="API symbol lookup">
  <label for="api-lookup-q">Find a symbol</label>
  <input id="api-lookup-q" type="search" class="api-lookup-input" placeholder="SgNode, SageBuilder, BuildAstTreeDepGraph…" aria-describedby="api-lookup-meta" />
  <p id="api-lookup-meta" class="api-lookup-meta" role="status" aria-live="polite">Loading symbol index…</p>
  <div id="api-lookup-results" data-limit="8"></div>
</section>

[Open symbol lookup](api-lookup.md) · [Browse API categories](api.md)

<noscript>Use the API categories to browse without JavaScript.</noscript>

## Developer workspace

<div class="reference-tasks">
  <a class="reference-task" href="build.md"><span aria-hidden="true">01</span><strong>Build REX</strong><span>Configure the LLVM and Flang toolchain and build the compiler.</span></a>
  <a class="reference-task" href="api.md"><span aria-hidden="true">02</span><strong>Explore the API</strong><span>Browse declarations, types, functions and their members.</span></a>
  <a class="reference-task" href="reference/SgNode.md"><span aria-hidden="true">03</span><strong>Inspect the AST</strong><span>Start with SgNode, its interfaces and derived node types.</span></a>
</div>

## Compiler source map

<div class="reference-source-map">
  <a href="https://github.com/ouankou/rex/tree/main/src/frontend"><strong>Frontends</strong><code>src/frontend</code><span>Clang, Flang and SageIII</span></a>
  <a href="https://github.com/ouankou/rex/tree/main/src/midend"><strong>Analysis &amp; transformation</strong><code>src/midend</code><span>AST processing, program analysis and loop processing</span></a>
  <a href="https://github.com/ouankou/rex/tree/main/src/backend"><strong>Backend</strong><code>src/backend</code><span>Source unparsing</span></a>
</div>

The reference includes private implementation members; visibility is not an API stability guarantee.
Use the header search for guides and the symbol lookup for API names.

[Source repository](https://github.com/ouankou/rex) · [Build this documentation](documentation.md) · [Build provenance](docs-build-manifest.json)
