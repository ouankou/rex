# API Quick Lookup

This page loads a compact symbol index and lets you jump directly to the generated reference page.

<div class="api-lookup-controls">
  <input id="api-lookup-q" type="search" class="api-lookup-input" placeholder="Search symbol name (e.g. SgToken, isSgToken, buildAst)" />
  <span id="api-lookup-meta" class="api-lookup-meta"></span>
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
  const isSafeUrl = (url) => typeof url === "string" && url.startsWith("/") && !url.startsWith("//");

  const render = (items, total) => {
    const safeItems = [];
    for (const s of items) {
      if (isSafeUrl(s?.url)) {
        safeItems.push(s);
      } else {
        console.error("Skipping invalid symbol URL:", s?.url);
      }
    }
    meta.textContent = state.loaded ? `${safeItems.length}/${total}` : "loading…";
    if (!state.loaded) return;

    if (!safeItems.length) {
      results.innerHTML = "<p>No matches.</p>";
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
