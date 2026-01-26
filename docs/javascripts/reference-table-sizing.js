(() => {
  const clamp = (value, min, max) => Math.max(min, Math.min(max, value))

  const isTwoColNameDescTable = (table) => {
    const cols = table.querySelectorAll("colgroup col")
    if (cols.length !== 2) return false
    const headerRow = table.querySelector("tbody > tr")
    if (!headerRow) return false
    const cells = headerRow.querySelectorAll("td, th")
    if (cells.length !== 2) return false
    const left = (cells[0].textContent || "").trim().toLowerCase()
    const right = (cells[1].textContent || "").trim().toLowerCase()
    return left === "name" && right === "description"
  }

  const computeNameColCh = (table) => {
    const rows = Array.from(table.querySelectorAll("tbody > tr"))
    if (rows.length <= 1) return null
    let maxLen = 0
    for (const row of rows.slice(1)) {
      const cell = row.querySelector("td:first-child, th:first-child")
      if (!cell) continue
      const code = cell.querySelector("code")
      const basis = code ? code.textContent : cell.textContent
      const text = (basis || "").replace(/\s+/g, " ").trim()
      if (!text) continue
      maxLen = Math.max(maxLen, text.length)
    }
    if (maxLen === 0) return null
    const desired = clamp(maxLen + 2, 10, 28)
    return desired
  }

  const apply = () => {
    const tables = document.querySelectorAll(".md-typeset__table table.tableblock")
    for (const table of tables) {
      if (!isTwoColNameDescTable(table)) continue
      const ch = computeNameColCh(table)
      if (!ch) continue
      table.classList.add("ref-two-col")
      table.style.setProperty("--ref-name-col", `${ch}ch`)
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", apply, { once: true })
  } else {
    apply()
  }
})()
