# API Quick Lookup

This page loads a compact symbol index and lets you jump directly to the generated reference page.

<div style="display:flex;gap:.5rem;align-items:center;flex-wrap:wrap;margin:1rem 0;">
  <input id="api-lookup-q" type="search" placeholder="Search symbol name (e.g. SgToken, isSgToken, buildAst)" style="flex:1;min-width:18rem;padding:.6rem .8rem;border:1px solid var(--md-default-fg-color--lightest);border-radius:.4rem;background:var(--md-default-bg-color);color:var(--md-default-fg-color);" />
  <span id="api-lookup-meta" style="opacity:.75;font-size:.9em;"></span>
</div>

<div id="api-lookup-results"></div>

<script>
(() => {
  const q = document.getElementById("api-lookup-q");
  const meta = document.getElementById("api-lookup-meta");
  const results = document.getElementById("api-lookup-results");
  const state = { symbols: [], loaded: false };

  const escapeHtml = (s) => String(s).replace(/[&<>"']/g, (c) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;"
  }[c]));

  const normalize = (s) => String(s || "").toLowerCase();

  const render = (items, total) => {
    meta.textContent = state.loaded ? `${items.length}/${total}` : "loading…";
    if (!state.loaded) return;

    if (!items.length) {
      results.innerHTML = "<p>No matches.</p>";
      return;
    }

    const rows = items.slice(0, 200).map((s) => {
      const name = escapeHtml(s.name);
      const url = escapeHtml(s.url);
      const kind = escapeHtml(s.kind);
      return `<div style="display:flex;gap:.75rem;align-items:baseline;margin:.35rem 0;">
        <a href="${url}" style="font-weight:600;">${name}</a>
        <span style="opacity:.65;font-size:.85em;">${kind}</span>
      </div>`;
    }).join("");

    results.innerHTML = rows + (items.length > 200 ? `<p style="opacity:.75">Showing first 200 results.</p>` : "");
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
      if (out.length >= 5000) break;
    }
    render(out, state.symbols.length);
  };

  q.addEventListener("input", filter);

  fetch("/assets/api/symbols.json", { cache: "no-store" })
    .then((r) => r.ok ? r.json() : Promise.reject(new Error(`HTTP ${r.status}`)))
    .then((data) => {
      state.symbols = Array.isArray(data?.symbols) ? data.symbols : [];
      state.loaded = true;
      render(state.symbols, state.symbols.length);
      q.focus();
    })
    .catch((err) => {
      state.loaded = true;
      meta.textContent = "failed to load index";
      results.innerHTML = `<p>Failed to load /assets/api/symbols.json: ${escapeHtml(err.message)}</p>`;
    });
})();
</script>

