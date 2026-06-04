async function fetchReport() {
  const res = await fetch("/api/test-report");
  if (!res.ok) {
    throw new Error(`HTTP ${res.status}`);
  }
  return res.json();
}

function fmtSec(sec) {
  return `${Number(sec || 0).toFixed(1)}s`;
}

function summarizeCategories(cases) {
  const groups = new Map();
  for (const item of cases) {
    const key = item.category || "other";
    if (!groups.has(key)) {
      groups.set(key, { category: key, total: 0, passed: 0, points: 0, duration: 0 });
    }
    const acc = groups.get(key);
    acc.total++;
    if (item.passed) acc.passed++;
    acc.points += Number(item.points || 0);
    acc.duration += Number(item.duration_sec || 0);
  }
  return Array.from(groups.values());
}

function summarizeFeatures(cases) {
  const groups = new Map();
  for (const item of cases) {
    const key = item.feature || "unknown";
    if (!groups.has(key)) {
      groups.set(key, { feature: key, total: 0, points: 0, duration: 0, expectedFailure: 0 });
    }
    const acc = groups.get(key);
    acc.total++;
    acc.points += Number(item.points || 0);
    acc.duration += Number(item.duration_sec || 0);
    acc.expectedFailure += Number(item.details?.expected_failure || 0);
  }
  return Array.from(groups.values()).sort((a, b) => a.feature.localeCompare(b.feature));
}

function renderSummary(summary) {
  const host = document.getElementById("report-summary");
  const cards = [
    { label: "总用例", value: summary.total, sub: `通过 ${summary.passed} / 失败 ${summary.failed}` },
    { label: "通过率", value: `${((summary.passed / Math.max(1, summary.total)) * 100).toFixed(1)}%`, sub: `分支 ${summary.branch} · commit ${summary.commitShort}` },
    { label: "总耗时", value: fmtSec(summary.totalDurationSec), sub: `observer: ${summary.observerName}` },
    { label: "测试层级", value: `${summary.categoryCount}`, sub: "官方 / 自定义 / 压力 三层回归" },
  ];
  host.innerHTML = cards.map((card) => `
    <article class="stat-card">
      <div class="stat-label">${card.label}</div>
      <div class="stat-value">${card.value}</div>
      <div class="stat-sub">${card.sub}</div>
    </article>
  `).join("");
}

function renderCategories(items) {
  const host = document.getElementById("category-bars");
  const max = Math.max(...items.map((it) => it.total), 1);
  host.innerHTML = items.map((item) => `
    <article class="bar-card">
      <div class="bar-head">
        <strong>${item.category}</strong>
        <span class="bar-meta">${item.passed}/${item.total} 通过 · ${item.points} 分 · ${fmtSec(item.duration)}</span>
      </div>
      <div class="bar-track">
        <div class="bar-fill" style="width:${(item.total / max) * 100}%"></div>
      </div>
    </article>
  `).join("");
}

function renderFeatures(items) {
  const host = document.getElementById("feature-table");
  host.innerHTML = items.map((item) => `
    <tr>
      <td>${item.feature}</td>
      <td>${item.total}</td>
      <td>${item.points}</td>
      <td>${fmtSec(item.duration)}</td>
      <td>${item.expectedFailure}</td>
    </tr>
  `).join("");
}

function renderSlowCases(items) {
  const host = document.getElementById("slow-table");
  host.innerHTML = items.map((item) => `
    <tr>
      <td>${item.id}</td>
      <td>${item.category}</td>
      <td>${item.feature}</td>
      <td>${fmtSec(item.duration_sec)}</td>
    </tr>
  `).join("");
}

function renderSnippets(items) {
  const host = document.getElementById("snippet-grid");
  host.innerHTML = items.map((item) => `
    <article class="snippet-card">
      <h3>${item.title}</h3>
      <pre>${item.content}</pre>
    </article>
  `).join("");
}

async function main() {
  try {
    const payload = await fetchReport();
    renderSummary(payload.summary);
    renderCategories(payload.categories);
    renderFeatures(payload.features);
    renderSlowCases(payload.slowCases);
    renderSnippets(payload.snippets);
  } catch (err) {
    document.getElementById("report-summary").innerHTML = `
      <article class="stat-card">
        <div class="stat-label">加载失败</div>
        <div class="stat-value">/api/test-report</div>
        <div class="stat-sub">${err.message || err}</div>
      </article>
    `;
  }
}

main();
