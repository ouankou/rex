(() => {
  const q = document.getElementById("api-lookup-q");
  const meta = document.getElementById("api-lookup-meta");
  const results = document.getElementById("api-lookup-results");
  if (!q || !meta || !results) return;
  const siteRoot = new URL("../", document.currentScript.src);
  const limit = Number(results.dataset.limit || 200);
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
    const resolved = new URL(path, siteRoot).pathname;
    return resolved.startsWith(new URL("reference/", siteRoot).pathname) ? resolved : null;
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
      ? `${safeItems.length} matching symbols of ${total}; showing ${Math.min(safeItems.length, limit)}.`
      : "Loading symbol index…";
    if (!state.loaded) return;

    if (!safeItems.length) {
      results.innerHTML = "<p>No matching symbols. Try a shorter name, or browse the reference by category.</p>";
      return;
    }

    const rows = safeItems.slice(0, limit).map((s) => {
      const name = escapeHtml(s.name);
      const url = escapeHtml(s.url);
      const kind = escapeHtml(s.kind);
      return `<div class="api-lookup-row">
        <a href="${url}" class="api-lookup-link">${name}</a>
        <span class="api-lookup-kind">${kind}</span>
      </div>`;
    }).join("");

    results.innerHTML = rows + (items.length > limit ? `<p class="api-lookup-limit">Showing first ${limit} results; refine the name to narrow the list.</p>` : "");
  };

  const filter = () => {
    const needle = normalize(q.value).trim();
    if (!state.loaded) return;
    if (!needle) {
      meta.textContent = `${state.symbols.length} symbols indexed. Enter a name to search.`;
      results.innerHTML = "";
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
