# API Quick Lookup

Find symbols, members, and individual overloads in the generated reference.
Search by a name or qualified name; you can also [browse by category](reference/index.md).

<div class="api-lookup-controls">
  <label for="api-lookup-q">Symbol name</label>
  <input id="api-lookup-q" type="search" class="api-lookup-input" placeholder="For example: SgNode or BuildAstTreeDepGraph" aria-describedby="api-lookup-meta" />
  <span id="api-lookup-meta" class="api-lookup-meta" role="status" aria-live="polite">Loading symbol index…</span>
</div>

<div id="api-lookup-results"></div>
<noscript>JavaScript is required for lookup. Use the category link above to browse without JavaScript.</noscript>

<script>
(() => {
  const q = document.getElementById("api-lookup-q");
  const meta = document.getElementById("api-lookup-meta");
  const results = document.getElementById("api-lookup-results");
  const pathname = window.location.pathname;
  const isDirectoryPage = pathname.endsWith("/") || pathname.endsWith("/index.html");
  const siteRoot = new URL(isDirectoryPage ? "../" : "./", window.location.href);
  const symbolsUrl = new URL("assets/api/symbols.json", siteRoot);
  const state = { symbols: [], loaded: false };

  const escapeHtml = (s) => String(s).replace(/[&<>"']/g, (c) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;"
  }[c]));

  const normalize = (s) => String(s || "").toLowerCase();
  const resolveSiteUrl = (url) => {
    if (typeof url !== "string" || url.startsWith("//") || /^[a-z][a-z0-9+.-]*:/i.test(url)) {
      return null;
    }
    const path = url.replace(/^\/+/, "");
    if (!path.startsWith("reference/")) {
      return null;
    }
    return new URL(path, siteRoot).pathname;
  };

  const render = (items, total) => {
    const safeItems = [];
    for (const s of items) {
      const url = resolveSiteUrl(s?.url);
      if (url) {
        safeItems.push({ ...s, url });
      } else {
        console.error("Skipping invalid symbol URL:", s?.url);
      }
    }
    meta.textContent = state.loaded
      ? `${safeItems.length} matching symbols of ${total}; showing ${Math.min(safeItems.length, 200)}.`
      : "Loading symbol index…";
    if (!state.loaded) return;

    if (!safeItems.length) {
      results.innerHTML = "<p>No matching symbols. Try a shorter name, or browse the reference by category.</p>";
      return;
    }

    const rows = safeItems.slice(0, 200).map((s) => {
      const name = escapeHtml(s.name);
      const url = escapeHtml(s.url);
      const kind = escapeHtml(s.kind);
      return `<div class="api-lookup-row">
        <a href="${url}" class="api-lookup-link">${name}</a>
        <span class="api-lookup-kind">${kind}</span>
      </div>`;
    }).join("");

    results.innerHTML = rows + (items.length > 200 ? `<p class="api-lookup-limit">Showing first 200 results.</p>` : "");
  };

  const filter = () => {
    const needle = normalize(q.value).trim();
    if (!state.loaded) return;
    if (!needle) {
      render(state.symbols, state.symbols.length);
      return;
    }
    const out = [];
    for (const s of state.symbols) {
      if (normalize(s.name).includes(needle)) out.push(s);
    }
    render(out, state.symbols.length);
  };

  q.addEventListener("input", filter);

  fetch(symbolsUrl)
    .then((r) => r.ok ? r.json() : Promise.reject(new Error(`HTTP ${r.status}`)))
    .then((data) => {
      if (!Array.isArray(data?.symbols) || !data.symbols.length) {
        throw new Error("Empty or invalid symbol index");
      }
      state.symbols = data.symbols;
      state.loaded = true;
      filter();
    })
    .catch((err) => {
      meta.textContent = "Could not load the symbol index.";
      results.innerHTML = `<p>Browse the <a href="${escapeHtml(new URL("reference/", siteRoot).pathname)}">reference index</a> instead.</p>`;
      console.error("Could not load API symbol index:", err);
    });
})();
</script>
