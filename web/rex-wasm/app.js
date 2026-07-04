const editor = document.querySelector("#source-editor");
const sourceMirror = document.querySelector("#source-mirror");
const fileInput = document.querySelector("#file-input");
const modeButtons = Array.from(document.querySelectorAll(".mode-button"));
const tabList = document.querySelector("#tab-list");
const outputView = document.querySelector("#output-view");
const outputMode = document.querySelector("#output-mode");
const statusLine = document.querySelector("#status-line");
const runBadge = document.querySelector("#run-badge");
const buildCommit = document.querySelector("#build-commit");

const modeLabels = {
  plain : "Round Trip",
  omp_ast : "OpenMP AST-Only",
  omp_lowering : "OpenMP Lowering",
};

const cKeywords = new Set([
  "auto",     "break",    "case",     "const",  "continue", "default", "do",
  "else",     "enum",     "extern",   "for",    "goto",     "if",      "inline",
  "register", "restrict", "return",   "sizeof", "static",   "struct",  "switch",
  "typedef",  "union",    "volatile", "while",
]);

const cTypes = new Set([
  "bool",
  "char",
  "double",
  "float",
  "int",
  "long",
  "short",
  "signed",
  "size_t",
  "unsigned",
  "void",
]);

const sample = {
  filename : "rodinia_axpy_multi_like.c",
  mode : "omp_lowering",
  source : `#define RODINIA_AXPY_SIZE 64
#define RODINIA_AXPY_TEAMS 4
#define RODINIA_AXPY_THREADS 64

static float x[RODINIA_AXPY_SIZE];
static float y[RODINIA_AXPY_SIZE];

static void scale_like(float *xv, float factor, int n) {
  int i;

#pragma omp target teams distribute parallel for map(tofrom : xv[0 : n]) \\
    map(to : factor) num_teams(RODINIA_AXPY_TEAMS) \\
    thread_limit(RODINIA_AXPY_THREADS)
  for (i = 0; i < n; i++) {
    xv[i] = xv[i] * factor;
  }
}

static void axpy_like(float *xv, float *yv, float a, int n) {
  int i;

#pragma omp target teams distribute parallel for map(to : xv[0 : n], a) \\
    map(tofrom : yv[0 : n]) num_teams(RODINIA_AXPY_TEAMS) \\
    thread_limit(RODINIA_AXPY_THREADS)
  for (i = 0; i < n; i++) {
    yv[i] = a * xv[i] + yv[i];
  }
}

int main(void) {
  int n = RODINIA_AXPY_SIZE;
  axpy_like(x, y, 3.0f, n);
  scale_like(y, 2.0f, n);
  return (int)y[0];
}
`,
};

let currentFilename = sample.filename;
let currentMode = sample.mode;
let activeRun = null;
let currentTabs = [];

function escapeHtml(text) {
  return text.replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;");
}

function span(className, text) {
  return `<span class="${className}">${escapeHtml(text)}</span>`;
}

function readWhile(line, start, predicate) {
  let end = start;
  while (end < line.length && predicate(line[end])) {
    end += 1;
  }
  return end;
}

function highlightLine(line, state) {
  if (line.length === 0) {
    return " ";
  }

  if (!state.inBlockComment && /^\s*#/.test(line)) {
    return span("tok-preprocessor", line);
  }

  let i = 0;
  let html = "";
  while (i < line.length) {
    if (state.inBlockComment) {
      const end = line.indexOf("*/", i);
      if (end === -1) {
        html += span("tok-comment", line.slice(i));
        return html;
      }
      html += span("tok-comment", line.slice(i, end + 2));
      i = end + 2;
      state.inBlockComment = false;
      continue;
    }

    if (line.startsWith("//", i)) {
      html += span("tok-comment", line.slice(i));
      return html;
    }

    if (line.startsWith("/*", i)) {
      const end = line.indexOf("*/", i + 2);
      if (end === -1) {
        html += span("tok-comment", line.slice(i));
        state.inBlockComment = true;
        return html;
      }
      html += span("tok-comment", line.slice(i, end + 2));
      i = end + 2;
      continue;
    }

    const ch = line[i];
    if (ch === '"' || ch === "'") {
      const quote = ch;
      let end = i + 1;
      while (end < line.length) {
        if (line[end] === "\\") {
          end += 2;
          continue;
        }
        if (line[end] === quote) {
          end += 1;
          break;
        }
        end += 1;
      }
      html += span("tok-string", line.slice(i, end));
      i = end;
      continue;
    }

    if (/[0-9]/.test(ch)) {
      const match = line.slice(i).match(
          /^(?:0[xX][0-9a-fA-F]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)[uUlLfF]*/);
      if (match) {
        html += span("tok-number", match[0]);
        i += match[0].length;
        continue;
      }
    }

    if (/[A-Za-z_]/.test(ch)) {
      const end = readWhile(line, i, (value) => /[A-Za-z0-9_]/.test(value));
      const word = line.slice(i, end);
      if (cKeywords.has(word)) {
        html += span("tok-keyword", word);
      } else if (cTypes.has(word)) {
        html += span("tok-type", word);
      } else {
        html += escapeHtml(word);
      }
      i = end;
      continue;
    }

    html += escapeHtml(ch);
    i += 1;
  }

  return html;
}

function renderCodeLines(target, content, kind = "code") {
  const lines = (content || "").split("\n");
  if (lines.length === 0) {
    lines.push("");
  }

  const state = {inBlockComment : false};
  target.replaceChildren();

  lines.forEach((line, index) => {
    const row = document.createElement("div");
    row.className = "code-line";

    const number = document.createElement("div");
    number.className = "line-number";
    number.textContent = String(index + 1);

    const code = document.createElement("div");
    code.className = "line-code";
    if (kind === "code") {
      code.innerHTML = highlightLine(line, state);
    } else {
      code.textContent = line.length === 0 ? " " : line;
    }

    row.append(number, code);
    target.appendChild(row);
  });
}

function renderSourceMirror() {
  renderCodeLines(sourceMirror, editor.value, "code");
  sourceMirror.style.transform = `translateY(${- editor.scrollTop}px)`;
}

function setOutputMode(mode) {
  outputMode.textContent =
      mode ? `Mode: ${modeLabels[mode] ?? mode}` : "No output yet";
}

function renderBuildInfo() {
  const info = window.REX_WASM_BUILD_INFO || {};
  if (!info.commit || !buildCommit) {
    return;
  }

  buildCommit.textContent = info.commit;
  if (info.commitUrl) {
    buildCommit.href = info.commitUrl;
    buildCommit.target = "_blank";
    buildCommit.rel = "noreferrer";
  } else {
    buildCommit.removeAttribute("href");
  }
  buildCommit.hidden = false;
}

function setStatus(kind, message) {
  statusLine.textContent = message;
  runBadge.className = `run-badge ${kind}`;
  runBadge.textContent = kind === "running" ? "running" : kind;
}

function renderTabs(tabs, activeIndex = 0) {
  currentTabs = tabs;
  tabList.replaceChildren();

  tabs.forEach((tab, index) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `tab-button${index === activeIndex ? " active" : ""}`;
    button.role = "tab";
    button.textContent = tab.name;
    button.title = tab.name;
    button.addEventListener("click", () => renderTabs(currentTabs, index));
    tabList.appendChild(button);
  });

  const tab = tabs[activeIndex] ?? {content : "", kind : "log"};
  renderCodeLines(outputView, tab.content, tab.kind ?? "log");
}

function renderResult(result) {
  const outputTabs = result.files.map((file) => ({
                                        name : file.name,
                                        content : file.content,
                                        kind : "code",
                                      }));
  outputTabs.push({
    name : "Log",
    content : result.log || "(no log output)",
    kind : "log",
  });

  setOutputMode(result.mode);
  renderTabs(outputTabs);
  if (result.ok) {
    setStatus("ok", `${result.files.length} generated file(s)`);
  } else {
    setStatus("error", `REX exited with ${result.exitCode}`);
  }
}

function runCurrentMode() {
  if (activeRun) {
    activeRun.terminate();
    activeRun = null;
  }

  setStatus("running", "Loading REX WASM");
  setOutputMode(currentMode);
  renderTabs([ {name : "Log", content : "Starting REX WASM..."} ]);

  const worker = new Worker("worker.js");
  activeRun = worker;

  worker.onmessage = (event) => {
    if (event.data.type === "ready") {
      setStatus("running", "Running REX");
      return;
    }
    if (event.data.type === "result") {
      activeRun = null;
      worker.terminate();
      renderResult(event.data.result);
      return;
    }
    if (event.data.type === "error") {
      activeRun = null;
      worker.terminate();
      setStatus("error", "REX failed");
      renderTabs([ {name : "Log", content : event.data.message} ]);
    }
  };

  worker.onerror = (event) => {
    activeRun = null;
    worker.terminate();
    setStatus("error", "Worker failed");
    renderTabs([ {name : "Log", content : event.message} ]);
  };

  worker.postMessage({
    type : "run",
    source : editor.value,
    filename : currentFilename,
    mode : currentMode,
  });
}

for (const button of modeButtons) {
  button.addEventListener("click", () => {
    currentMode = button.dataset.mode;
    runCurrentMode();
  });
}

fileInput.addEventListener("change", async () => {
  const file = fileInput.files?.[0];
  if (!file) {
    return;
  }
  currentFilename = file.name;
  editor.value = await file.text();
  renderSourceMirror();
  renderTabs([ {name : "Log", content : `${file.name} loaded.`} ]);
  setOutputMode(null);
  setStatus("idle", "Source loaded");
  fileInput.value = "";
});

editor.value = sample.source;
renderBuildInfo();
renderSourceMirror();
editor.addEventListener("input", renderSourceMirror);
editor.addEventListener("scroll", () => {
  sourceMirror.style.transform = `translateY(${- editor.scrollTop}px)`;
});
renderTabs([ {name : "Log", content : "Ready."} ]);
