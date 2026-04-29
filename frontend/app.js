const params = new URLSearchParams(window.location.search);
const mode = params.get("mode") === "clr" ? "clr" : "ll1";

const runBtn = document.getElementById("runBtn");
const sampleBtn = document.getElementById("sampleBtn");
const codeInput = document.getElementById("codeInput");
const modeTitle = document.getElementById("modeTitle");
const modeDescription = document.getElementById("modeDescription");
const supportNote = document.getElementById("supportNote");
const traceTitle = document.getElementById("traceTitle");
const statusSection = document.getElementById("statusSection");
const grammarSection = document.getElementById("grammarSection");
const firstFollowSection = document.getElementById("firstFollowSection");
const ll1TableSection = document.getElementById("ll1TableSection");
const clrItemsSection = document.getElementById("clrItemsSection");
const clrTableSection = document.getElementById("clrTableSection");
const traceSection = document.getElementById("traceSection");
const grammarBox = document.getElementById("grammarBox");
const firstBox = document.getElementById("firstBox");
const followBox = document.getElementById("followBox");
const ll1Table = document.getElementById("ll1Table");
const actionTable = document.getElementById("actionTable");
const gotoTable = document.getElementById("gotoTable");
const clrItemsBox = document.getElementById("clrItemsBox");
const traceBody = document.querySelector("#traceTable tbody");

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function makeKVList(obj, titlePrefix) {
  const wrap = document.createElement("div");
  wrap.className = "value-list";
  Object.keys(obj || {}).sort().forEach((key) => {
    const row = document.createElement("div");
    row.className = "value-item";
    row.textContent = `${titlePrefix}(${key}) = { ${(obj[key] || []).join(", ")} }`;
    wrap.appendChild(row);
  });
  return wrap;
}

function showStatus(ok, msg) {
  statusSection.classList.remove("hidden", "ok", "bad");
  statusSection.classList.add(ok ? "ok" : "bad");
  statusSection.textContent = msg;
}

function renderGrammar(rules) {
  grammarBox.innerHTML = "";
  (rules || []).forEach((rule) => {
    const row = document.createElement("div");
    row.className = "value-item";
    row.textContent = rule;
    grammarBox.appendChild(row);
  });
}

function renderGridTable(target, tableObj, rowHeader) {
  const rows = Object.keys(tableObj || {}).sort((a, b) => String(a).localeCompare(String(b), undefined, { numeric: true }));
  const columnSet = new Set();
  rows.forEach((row) => {
    Object.keys(tableObj[row] || {}).forEach((col) => columnSet.add(col));
  });
  const columns = [...columnSet].sort((a, b) => String(a).localeCompare(String(b), undefined, { numeric: true }));

  let html = `<thead><tr><th>${escapeHtml(rowHeader)}</th>`;
  columns.forEach((col) => {
    html += `<th>${escapeHtml(col)}</th>`;
  });
  html += "</tr></thead><tbody>";

  rows.forEach((row) => {
    html += `<tr><th>${escapeHtml(row)}</th>`;
    columns.forEach((col) => {
      const value = tableObj[row][col] || "";
      html += `<td>${escapeHtml(value)}</td>`;
    });
    html += "</tr>";
  });

  html += "</tbody>";
  target.innerHTML = html;
}

function renderClrStates(states) {
  clrItemsBox.innerHTML = "";
  (states || []).forEach((state) => {
    const card = document.createElement("div");
    card.className = "state-card";
    const title = document.createElement("h3");
    title.textContent = `State ${state.state}`;
    card.appendChild(title);

    (state.items || []).forEach((item) => {
      const row = document.createElement("div");
      row.className = "state-item";
      row.textContent = item;
      card.appendChild(row);
    });

    clrItemsBox.appendChild(card);
  });
}

function renderTrace(trace) {
  traceBody.innerHTML = "";
  (trace || []).forEach((step) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `<td>${escapeHtml(step.stack)}</td><td>${escapeHtml(step.input)}</td><td>${escapeHtml(step.action)}</td>`;
    traceBody.appendChild(tr);
  });
}

function applyModeText() {
  if (mode === "clr") {
    modeTitle.textContent = "CLR Parser Visualizer";
    modeDescription.textContent = "Inspect canonical LR(1) item sets, ACTION/GOTO tables, and shift-reduce parsing steps.";
    supportNote.textContent = "Supported grammar matches the LL(1) mode: declarations, assignments, blocks, if/else, while, for, optional #include, and main().";
    traceTitle.textContent = "CLR Shift / Reduce Trace";
  } else {
    modeTitle.textContent = "LL(1) Parser Visualizer";
    modeDescription.textContent = "Inspect snippet-specific grammar, FIRST/FOLLOW, the LL(1) table, and predictive parsing steps.";
    supportNote.textContent = "Supported: declarations, assignments, blocks, if/else, while, for, optional #include, and main().";
    traceTitle.textContent = "Stack / Input / Action Table";
  }
}

function applySamples() {
  const ll1Samples = [
    `int a = 1;\na = (a + 2) * 3;`,
    `int main() {\n  int i = 0;\n  while (i < 3) {\n    i = i + 1;\n  }\n}`,
    `#include<stdio.h>\nint main() {\n  int i = 0;\n  for (i = 0; i < 3; i = i + 1) {\n    i = i + 2;\n  }\n}`,
  ];

  const clrSamples = [
    `int a = 1;\na = (a + 2) * 3;`,
    `int main() {\n  int i = 0;\n  while (i < 3) {\n    i = i + 1;\n  }\n}`,
    `#include<stdio.h>\nint main() {\n  int i = 0;\n  for (i = 0; i < 3; i = i + 1) {\n    i = i + 2;\n  }\n}`,
  ];

  const samples = mode === "clr" ? clrSamples : ll1Samples;
  sampleBtn.addEventListener("click", () => {
    const random = Math.floor(Math.random() * samples.length);
    codeInput.value = samples[random];
  });
}

async function runAnalysis() {
  const code = codeInput.value.trim();
  if (!code) {
    showStatus(false, "Enter a code snippet first.");
    return;
  }

  runBtn.disabled = true;
  sampleBtn.disabled = true;
  runBtn.textContent = "Analyzing...";
  showStatus(true, `Running ${mode.toUpperCase()} parser...`);

  try {
    const res = await fetch("/api/parse", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ code, mode }),
    });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error || "Unknown server error.");
    }

    renderGrammar(data.grammar_used);
    renderTrace(data.trace);

    grammarSection.classList.remove("hidden");
    traceSection.classList.remove("hidden");

    if (mode === "clr") {
      firstFollowSection.classList.add("hidden");
      ll1TableSection.classList.add("hidden");
      clrItemsSection.classList.remove("hidden");
      clrTableSection.classList.remove("hidden");

      renderClrStates(data.states);
      renderGridTable(actionTable, data.action_table, "State");
      renderGridTable(gotoTable, data.goto_table, "State");

      if (data.clr_ok === false && Array.isArray(data.clr_conflicts) && data.clr_conflicts.length) {
        showStatus(false, `${data.message} ${data.clr_conflicts.join(" | ")}`);
      } else {
        showStatus(data.accepted, data.message);
      }
    } else {
      clrItemsSection.classList.add("hidden");
      clrTableSection.classList.add("hidden");
      firstFollowSection.classList.remove("hidden");
      ll1TableSection.classList.remove("hidden");

      firstBox.innerHTML = "";
      followBox.innerHTML = "";
      firstBox.appendChild(makeKVList(data.first, "FIRST"));
      followBox.appendChild(makeKVList(data.follow, "FOLLOW"));
      renderGridTable(ll1Table, data.table, "NT / T");

      if (data.ll1_ok === false && Array.isArray(data.ll1_conflicts) && data.ll1_conflicts.length) {
        showStatus(false, `${data.message} ${data.ll1_conflicts.join(" | ")}`);
      } else {
        showStatus(data.accepted, data.message);
      }
    }
  } catch (err) {
    showStatus(false, `Error: ${err.message}`);
  } finally {
    runBtn.disabled = false;
    sampleBtn.disabled = false;
    runBtn.textContent = "Run Analysis";
  }
}

applyModeText();
applySamples();

runBtn.addEventListener("click", runAnalysis);
codeInput.addEventListener("keydown", (event) => {
  if ((event.ctrlKey || event.metaKey) && event.key === "Enter") {
    runAnalysis();
  }
});
