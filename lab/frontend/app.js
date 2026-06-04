// 前端控制：渲染按钮 → 点按钮 → POST /api/sql → 把结果追加到控制台。

const grid        = document.getElementById("button-grid");
const consoleEl   = document.getElementById("console");
const dotEl       = document.getElementById("status-dot");
const statusEl    = document.getElementById("status-text");
const runningEl   = document.getElementById("running");
const btnRefresh  = document.getElementById("btn-refresh");
const btnClear    = document.getElementById("btn-clear");
const btnReset    = document.getElementById("btn-reset");
const btnExport   = document.getElementById("btn-export");
const btnStop     = document.getElementById("btn-stop");
const btnSqlRun   = document.getElementById("btn-sql-run");
const btnSqlClear = document.getElementById("btn-sql-clear");
const sqlEditor   = document.getElementById("sql-editor");
const sqlPresets  = document.getElementById("sql-presets");

let currentRun   = null;       // AbortController
const cardById   = new Map();  // demoId → button element

const LIVE_SQL_PRESETS = [
  {
    id: "teacher_basic",
    title: "基础建表 + 查询",
    desc: "用最短链路展示建表、插入、查询都可用。",
    sql: [
      "CREATE TABLE live_basic(id INT, name CHAR(10));",
      "INSERT INTO live_basic VALUES (1, 'miniob');",
      "INSERT INTO live_basic VALUES (2, 'demo');",
      "SELECT * FROM live_basic;",
    ].join("\n"),
  },
  {
    id: "teacher_null",
    title: "NULL 语义",
    desc: "展示 NULL / IS NULL / 聚合跳过 NULL。",
    sql: [
      "CREATE TABLE live_null(id INT NOT NULL, score INT NULL, price FLOAT NOT NULL);",
      "INSERT INTO live_null VALUES (1, 18, 10.0);",
      "INSERT INTO live_null VALUES (2, NULL, 20.0);",
      "SELECT * FROM live_null WHERE score IS NULL;",
      "SELECT count(score) FROM live_null;",
    ].join("\n"),
  },
  {
    id: "teacher_unique",
    title: "UNIQUE 约束",
    desc: "先成功插入，再演示重复键被拒绝。",
    sql: [
      "CREATE TABLE live_uq(id INT, name CHAR(10));",
      "CREATE UNIQUE INDEX live_uq_id ON live_uq(id);",
      "INSERT INTO live_uq VALUES (1, 'ok');",
      "INSERT INTO live_uq VALUES (2, 'ok2');",
      "INSERT INTO live_uq VALUES (1, 'dup');",
      "SELECT * FROM live_uq;",
    ].join("\n"),
  },
  {
    id: "teacher_update_select",
    title: "UPDATE + 子查询",
    desc: "演示 SET 右值来自子查询。",
    sql: [
      "CREATE TABLE live_us1(id INT, val INT);",
      "CREATE TABLE live_us2(id INT, score INT);",
      "INSERT INTO live_us1 VALUES (1, 10);",
      "INSERT INTO live_us1 VALUES (2, 20);",
      "INSERT INTO live_us2 VALUES (1, 100);",
      "INSERT INTO live_us2 VALUES (2, 200);",
      "UPDATE live_us1 SET val = (SELECT score FROM live_us2 WHERE live_us2.id = live_us1.id);",
      "SELECT * FROM live_us1;",
    ].join("\n"),
  },
];

function normalizeSql(sql) {
  return String(sql)
    .split(/\r?\n/)
    .filter((line) => !/^\s*--/.test(line))
    .join("\n")
    .trim();
}

function resolveDemoStatements(demo) {
  return resolveDemo(demo)
    .map((entry) => {
      if (typeof entry === "string") {
        const sql = normalizeSql(entry);
        return sql ? { sql } : null;
      }
      if (!entry || typeof entry.sql !== "string") return null;
      const sql = normalizeSql(entry.sql);
      if (!sql) return null;
      return {
        sql,
        expectedFailure: entry.expectedFailure === true,
        note: typeof entry.note === "string" ? entry.note : "",
      };
    })
    .filter(Boolean);
}

function parseSqlScript(script) {
  return String(script)
    .split(";")
    .map((part) => normalizeSql(part))
    .filter((sql) => sql.length > 0)
    .map((sql) => ({ sql: sql.endsWith(";") ? sql : `${sql};` }));
}

// ---- 状态栏 -------------------------------------------------------------

async function refreshStatus() {
  dotEl.classList.remove("on", "off");
  dotEl.classList.add("unk");
  statusEl.textContent = "检查 observer…";
  try {
    const r = await fetch("/api/health");
    const j = await r.json();
    if (j.ok) {
      dotEl.classList.remove("unk");
      dotEl.classList.add("on");
      statusEl.textContent = `observer 已连接 · tcp://${j.observer.host}:${j.observer.port}`;
    } else {
      dotEl.classList.remove("unk");
      dotEl.classList.add("off");
      statusEl.textContent = `observer 未启动 (tcp://${j.observer.host}:${j.observer.port})`;
    }
  } catch (e) {
    dotEl.classList.remove("unk");
    dotEl.classList.add("off");
    statusEl.textContent = "前端服务不可达";
  }
}

// ---- 按钮渲染 -----------------------------------------------------------

function renderButtons() {
  const groups = new Map();
  for (const d of DEMOS) {
    if (!groups.has(d.group)) groups.set(d.group, []);
    groups.get(d.group).push(d);
  }
  grid.innerHTML = "";
  for (const [gname, items] of groups) {
    const lbl = document.createElement("div");
    lbl.className = "group-label";
    lbl.textContent = gname;
    grid.appendChild(lbl);
    for (const d of items) {
      const b = document.createElement("button");
      b.className = "card";
      b.dataset.id = d.id;
      b.innerHTML = `<div class="t"></div><div class="g"></div>`;
      b.querySelector(".t").textContent = d.title;
      b.querySelector(".g").textContent = `${d.sql ? d.sql.length : "动态生成"} 条 SQL · ${d.group}`;
      b.addEventListener("click", () => runDemo(d, b));
      grid.appendChild(b);
      cardById.set(d.id, b);
    }
  }
}

// ---- 输出控制台 ---------------------------------------------------------

function appendEvent(parts) {
  const ev = document.createElement("div");
  ev.className = "ev";
  for (const p of parts) {
    const span = document.createElement("span");
    if (p.cls) span.className = p.cls;
    span.textContent = p.text + "\n";
    ev.appendChild(span);
  }
  consoleEl.appendChild(ev);
  consoleEl.scrollTop = consoleEl.scrollHeight;
}

function appendSqlBlock(sql) {
  const ev = document.createElement("div");
  ev.className = "ev";
  const hd = document.createElement("div");
  hd.className = "hd";
  hd.textContent = "SQL";
  const code = document.createElement("span");
  code.className = "sql mono";
  code.textContent = sql;
  ev.appendChild(hd);
  ev.appendChild(code);
  consoleEl.appendChild(ev);
  consoleEl.scrollTop = consoleEl.scrollHeight;
}

function appendResult(stmt, result) {
  const ev = document.createElement("div");
  ev.className = "ev";

  const tag = document.createElement("span");
  const expectedFailure = result.expectedFailure === true;
  const asExpected = expectedFailure && !result.success;
  tag.className = "tag " + ((result.success || asExpected) ? "ok" : "err");
  tag.textContent = result.success ? "SUCCESS" : (asExpected ? "EXPECTED FAILURE" : "FAILURE");

  const code = document.createElement("span");
  code.className = "mono";
  code.textContent = stmt;

  ev.appendChild(tag);
  ev.appendChild(code);
  ev.appendChild(document.createTextNode("\n"));

  if (result.output && result.output.length > 0) {
    const out = document.createElement("span");
    out.className = "mono";
    out.style.whiteSpace = "pre-wrap";
    out.textContent = result.output;
    ev.appendChild(out);
  }

  if (result.note) {
    ev.appendChild(document.createTextNode("\n"));
    const note = document.createElement("span");
    note.className = "mono";
    note.style.whiteSpace = "pre-wrap";
    note.textContent = `说明：${result.note}`;
    ev.appendChild(note);
  }
  consoleEl.appendChild(ev);
  consoleEl.scrollTop = consoleEl.scrollHeight;
}

function clearConsole() {
  consoleEl.innerHTML = "";
}

// ---- 执行单条 demo -------------------------------------------------------

async function runDemo(demo, btn) {
  if (currentRun) return; // 串行：避免并发把 observer 状态搞乱
  const stmts = resolveDemoStatements(demo);
  await runStatementBatch({
    title: demo.title,
    label: demo.id,
    stmts,
    btn,
  });
}

async function runStatementBatch({ title, label, stmts, btn = null }) {
  if (stmts.length === 0) {
    appendEvent([{ cls: "err", text: `${title} 没有可执行的 SQL` }]);
    return;
  }

  if (btn) {
    btn.classList.add("running");
    btn.classList.remove("done", "failed");
  }
  runningEl.classList.remove("hidden");
  btnStop.disabled = false;
  appendEvent([
    { cls: "ok",  text: `▶ ${title}  (${stmts.length} 条 SQL)` },
  ]);
  for (const s of stmts) appendSqlBlock(s.sql);

  currentRun = new AbortController();
  const t0 = performance.now();
  let resp, netErr = null;
  try {
    resp = await fetch("/api/sql", {
      method:  "POST",
      headers: { "Content-Type": "application/json" },
      body:    JSON.stringify({ statements: stmts, demoId: label, metaLabel: title }),
      signal:  currentRun.signal,
    });
  } catch (e) {
    netErr = e;
  }
  currentRun = null;
  runningEl.classList.add("hidden");
  btnStop.disabled = true;

  if (netErr) {
    appendEvent([{ cls: "err", text: `✗ 请求失败：${netErr.message || netErr}` }]);
    if (btn) {
      btn.classList.remove("running");
      btn.classList.add("failed");
    }
    return;
  }
  if (!resp.ok) {
    appendEvent([{ cls: "err", text: `✗ HTTP ${resp.status}` }]);
    if (btn) {
      btn.classList.remove("running");
      btn.classList.add("failed");
    }
    return;
  }
  const j = await resp.json();
  if (!j.ok) {
    appendEvent([{ cls: "err", text: `✗ 后端错误：${j.error || "unknown"}` }]);
    if (btn) {
      btn.classList.remove("running");
      btn.classList.add("failed");
    }
    return;
  }
  let failed = 0;
  let expectedFailed = 0;
  for (const r of j.results) appendResult(r.sql, r);
  for (const r of j.results) {
    if (!r.success && r.expectedFailure) {
      expectedFailed++;
    } else if (!r.success) {
      failed++;
    }
  }
  const dt = (performance.now() - t0).toFixed(0);
  appendEvent([{
    cls: failed === 0 ? "ok" : "err",
    text: failed === 0
      ? (expectedFailed > 0
          ? `✓ ${title} 成功，含 ${expectedFailed} 条预期失败  (${dt} ms)`
          : `✓ ${title} 全部成功  (${dt} ms)`)
      : `✗ ${title} 存在 ${failed} 条非预期失败${expectedFailed > 0 ? `，另有 ${expectedFailed} 条预期失败` : ""}  (${dt} ms)`,
  }]);
  if (btn) {
    btn.classList.remove("running");
    btn.classList.add(failed === 0 ? "done" : "failed");
  }
}

// ---- 顶部按钮 -----------------------------------------------------------

btnRefresh.addEventListener("click", refreshStatus);
btnClear  .addEventListener("click", clearConsole);
btnStop   .addEventListener("click", () => {
  if (currentRun) currentRun.abort();
});
btnReset  .addEventListener("click", () => resetDemoData());
btnSqlRun .addEventListener("click", () => runAdHocSql());
btnSqlClear.addEventListener("click", () => { sqlEditor.value = ""; });
btnExport .addEventListener("click", () => {
  // 直接走 <a download> 触发浏览器下载
  const a = document.createElement("a");
  a.href = "/api/log/download";
  a.download = "";
  document.body.appendChild(a);
  a.click();
  a.remove();
  appendEvent([{ cls: "ok", text: "📥 已请求下载 session.log（浏览器默认保存路径）" }]);
});

async function resetDemoData() {
  if (currentRun) return;
  if (!Array.isArray(DEMO_TABLES) || DEMO_TABLES.length === 0) return;
  const stmts = DEMO_TABLES.map((t) => ({ sql: `DROP TABLE ${t};` }));
  appendEvent([{ cls: "ok", text: `🔄 重置 demo 数据：尝试 DROP ${stmts.length} 张 demo_* 表（不存在的会 FAILURE，忽略即可）` }]);
  for (const s of stmts) appendSqlBlock(s.sql);

  runningEl.classList.remove("hidden");
  btnStop.disabled = false;
  currentRun = new AbortController();
  const t0 = performance.now();
  let resp, netErr = null;
  try {
    resp = await fetch("/api/sql", {
      method:  "POST",
      headers: { "Content-Type": "application/json" },
      body:    JSON.stringify({ statements: stmts }),
      signal:  currentRun.signal,
    });
  } catch (e) { netErr = e; }
  currentRun = null;
  runningEl.classList.add("hidden");
  btnStop.disabled = true;

  if (netErr) { appendEvent([{ cls: "err", text: `✗ 请求失败：${netErr.message || netErr}` }]); return; }
  if (!resp.ok) { appendEvent([{ cls: "err", text: `✗ HTTP ${resp.status}` }]); return; }
  const j = await resp.json();
  if (!j.ok)   { appendEvent([{ cls: "err", text: `✗ 后端错误：${j.error || "unknown"}` }]); return; }
  let dropped = 0, missing = 0;
  for (const r of j.results) {
    if (r.success) dropped++;
    else           missing++;
  }
  const dt = (performance.now() - t0).toFixed(0);
  appendEvent([{
    cls: "ok",
    text: `✓ 重置完成：实际 DROP ${dropped} 张，${missing} 张本来就不存在  (${dt} ms)`,
  }]);
}

function renderSqlPresets() {
  sqlPresets.innerHTML = "";
  for (const preset of LIVE_SQL_PRESETS) {
    const card = document.createElement("div");
    card.className = "preset-card";
    card.innerHTML = `
      <div class="preset-title">${preset.title}</div>
      <div class="preset-desc">${preset.desc}</div>
      <div class="preset-actions">
        <button class="ghost" data-act="load">加载到输入框</button>
        <button data-act="run">直接执行</button>
      </div>
    `;
    card.querySelector('[data-act="load"]').addEventListener("click", () => {
      sqlEditor.value = preset.sql;
      sqlEditor.focus();
    });
    card.querySelector('[data-act="run"]').addEventListener("click", async () => {
      const stmts = parseSqlScript(preset.sql);
      await runStatementBatch({
        title: `现场推荐：${preset.title}`,
        label: `preset:${preset.id}`,
        stmts,
      });
    });
    sqlPresets.appendChild(card);
  }
}

async function runAdHocSql() {
  const script = sqlEditor.value;
  const stmts = parseSqlScript(script);
  if (stmts.length === 0) {
    appendEvent([{ cls: "err", text: "✗ 输入框里还没有可执行的 SQL" }]);
    return;
  }
  await runStatementBatch({
    title: "手写 SQL",
    label: "ad-hoc-sql",
    stmts,
  });
}

// ---- 启动 ---------------------------------------------------------------

renderButtons();
renderSqlPresets();
refreshStatus();
setInterval(refreshStatus, 8000);
