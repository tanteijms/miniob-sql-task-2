// 零依赖 Node HTTP server：
//   - 静态托管 index.html / app.js / style.css / demos.js
//   - POST /api/sql   { statements: [..] }  →  转给本地 MiniOB observer (TCP 6789)
//   - GET  /api/health                       →  后端 + observer 健康状态
//
// observer 协议（plain）：
//   客户端发送： <sql>\0
//   服务端回写： <输出>\0
//   一条连接可串行多轮请求/响应（与 obclient.cpp 行为一致）。

const http          = require("http");
const net           = require("net");
const fs            = require("fs");
const path          = require("path");
const url           = require("url");
const childProcess  = require("child_process");

const PORT          = Number(process.env.PORT          || 3001);
const OBS_HOST      = process.env.OBSERVER_HOST        || "127.0.0.1";
const OBS_PORT      = Number(process.env.OBSERVER_PORT || 6789);
const STATIC_DIR    = __dirname;
const LOG_FILE      = path.join(STATIC_DIR, "session.log");
const OBS_AUTO      = process.env.OBSERVER_AUTO_START !== "0";
const OBS_BIN_ENV   = process.env.OBSERVER_BIN || "";
const OBS_CFG_ENV   = process.env.OBSERVER_CONFIG || "";
const OBS_RUNTIME   = path.join(STATIC_DIR, "runtime", "observer-data");
const REQ_TIMEOUT   = Number(process.env.SQL_TIMEOUT_MS || 20000);
const MAX_BUF_BYTES = 1 << 20; // 1 MiB 单条响应上限
let managedObserver = null;

// 后端会话日志：每次执行都追加一条结构化记录，方便验收后归档。
// 通过 /api/log 下载；也可以直接 cat session.log 查看。
let logStream = null;
try {
  // 启动时清空旧日志，避免跨 session 串味
  fs.writeFileSync(LOG_FILE, "");
  logStream = fs.createWriteStream(LOG_FILE, { flags: "a" });
  logStream.on("error", (e) => console.error("[log] write error:", e.message));
} catch (e) {
  console.error("[log] cannot open session.log:", e.message);
}

function pad(n, w) { w = w || 2; const s = "" + n; return s.length >= w ? s : "0".repeat(w - s.length) + s; }
function ts() {
  const d = new Date();
  return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} `
       + `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${pad(d.getMilliseconds(), 3)}`;
}

function writeLog(meta, results, extra) {
  if (!logStream) return;
  const lines = [];
  lines.push(`\n===== [${ts()}] ${meta} =====`);
  if (extra) lines.push(extra);
  if (Array.isArray(results)) {
    for (const r of results) {
      const tag = r.success ? "OK   " : (r.expectedFailure ? "EXPF " : "FAIL ");
      const sql = r.sql.length > 200 ? r.sql.slice(0, 200) + " …" : r.sql;
      lines.push(`  ${tag} | ${sql}`);
      if (r.note) {
        lines.push(`         | 说明: ${r.note}`);
      }
      if (r.output) {
        for (const ln of String(r.output).split("\n")) {
          lines.push(`         | ${ln}`);
        }
      }
    }
  }
  logStream.write(lines.join("\n") + "\n");
}

function stripSqlComments(sql) {
  return String(sql)
    .split(/\r?\n/)
    .filter((line) => !/^\s*--/.test(line))
    .join("\n")
    .trim();
}

function normalizeStatements(list) {
  return list
    .map((s) => (typeof s === "string" ? { sql: s } : s))
    .map((s) => (s && typeof s.sql === "string" ? { ...s, sql: stripSqlComments(s.sql) } : null))
    .filter((s) => s && s.sql.length > 0);
}

function probeObserver(timeoutMs) {
  return new Promise((resolve) => {
    const sock = net.createConnection({ host: OBS_HOST, port: OBS_PORT });
    const timer = setTimeout(() => {
      sock.destroy();
      resolve(false);
    }, timeoutMs);
    sock.once("connect", () => {
      clearTimeout(timer);
      sock.destroy();
      resolve(true);
    });
    sock.once("error", () => {
      clearTimeout(timer);
      resolve(false);
    });
  });
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function resolveObserverBinary() {
  if (OBS_BIN_ENV) return OBS_BIN_ENV;
  const candidates = [
    path.resolve(STATIC_DIR, "../../build/bin/observer"),
    path.resolve(STATIC_DIR, "../../build_debug/bin/observer"),
  ];
  for (const file of candidates) {
    if (fs.existsSync(file)) return file;
  }
  return null;
}

function resolveObserverConfig() {
  if (OBS_CFG_ENV) return OBS_CFG_ENV;
  return path.resolve(STATIC_DIR, "../../etc/observer.ini");
}

function prepareObserverRuntime() {
  fs.rmSync(OBS_RUNTIME, { recursive: true, force: true });
  fs.mkdirSync(OBS_RUNTIME, { recursive: true });
}

async function startManagedObserver() {
  if (!OBS_AUTO) {
    console.log("[frontend] observer auto-start disabled");
    return false;
  }
  if (OBS_HOST !== "127.0.0.1" && OBS_HOST !== "localhost") {
    console.log(`[frontend] skip auto-start for remote observer host: ${OBS_HOST}`);
    return false;
  }
  if (await probeObserver(300)) {
    return false;
  }

  const observerBin = resolveObserverBinary();
  const observerCfg = resolveObserverConfig();
  if (!observerBin || !fs.existsSync(observerCfg)) {
    console.warn("[frontend] observer not running, and auto-start prerequisites are missing");
    return false;
  }

  prepareObserverRuntime();
  managedObserver = childProcess.spawn(
    observerBin,
    ["-f", observerCfg, "-p", String(OBS_PORT), "-P", "plain"],
    {
      cwd: OBS_RUNTIME,
      stdio: ["ignore", "pipe", "pipe"],
    },
  );

  managedObserver.stdout.on("data", (chunk) => {
    process.stdout.write(`[observer] ${chunk.toString("utf8")}`);
  });
  managedObserver.stderr.on("data", (chunk) => {
    process.stderr.write(`[observer] ${chunk.toString("utf8")}`);
  });
  managedObserver.on("exit", (code, signal) => {
    console.log(`[frontend] managed observer exited (code=${code}, signal=${signal})`);
    managedObserver = null;
  });

  for (let i = 0; i < 25; i++) {
    if (await probeObserver(200)) {
      console.log(`[frontend] managed observer ready in ${OBS_RUNTIME}`);
      return true;
    }
    if (managedObserver == null) break;
    await sleep(200);
  }

  console.warn("[frontend] managed observer did not become ready in time");
  return false;
}

function stopManagedObserver() {
  if (managedObserver && !managedObserver.killed) {
    managedObserver.kill("SIGTERM");
  }
}

process.on("exit", stopManagedObserver);
process.on("SIGINT", () => {
  stopManagedObserver();
  process.exit(0);
});
process.on("SIGTERM", () => {
  stopManagedObserver();
  process.exit(0);
});

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js":   "application/javascript; charset=utf-8",
  ".css":  "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".md":   "text/markdown; charset=utf-8",
  ".svg":  "image/svg+xml",
};

// ---- observer 客户端：单次连接串行多语句 -------------------------------

function runOnObserver(statements) {
  return new Promise((resolve) => {
    const sock = net.createConnection({ host: OBS_HOST, port: OBS_PORT });
    const results = [];
    let buf = Buffer.alloc(0);
    let cursor = 0;
    let pending = statements.slice();
    let timedOut = false;

    const finish = (err) => {
      if (sock.destroyed) return;
      sock.destroy();
      resolve({ ok: !err, error: err ? String(err) : null, results });
    };

    const timer = setTimeout(() => {
      timedOut = true;
      finish(new Error(`observer 超时（>${REQ_TIMEOUT}ms）`));
    }, REQ_TIMEOUT);

    sock.on("error", (e) => {
      if (timedOut) return;
      clearTimeout(timer);
      finish(new Error(`连接 observer 失败：${e.code || e.message}`));
    });

    sock.on("connect", () => { sendNext(); });

    sock.on("data", (chunk) => {
      if (timedOut) return;
      buf = Buffer.concat([buf, chunk]);
      if (buf.length > MAX_BUF_BYTES) {
        clearTimeout(timer);
        finish(new Error(`observer 返回超过 ${MAX_BUF_BYTES} 字节，已截断`));
        return;
      }
      // 拆出所有以 \0 结尾的完整响应
      let consumed = 0;
      while (true) {
        const nul = buf.indexOf(0, consumed);
        if (nul < 0) break;
        const piece = buf.slice(consumed, nul);
        consumed = nul + 1;
        const stmt = pending[0];
        if (stmt === undefined) {
          // 收到额外响应，忽略但记日志
          continue;
        }
        const output = piece.toString("utf8").replace(/\n$/, "");
        results.push({
          sql: stmt.sql,
          note: stmt.note || "",
          expectedFailure: stmt.expectedFailure === true,
          output,
          success: !/^FAILURE\b/m.test(output),
        });
        pending.shift();
        if (pending.length === 0) {
          clearTimeout(timer);
          finish();
          return;
        }
        // 串行下一条
        sendNext();
      }
      if (consumed > 0) {
        buf = buf.slice(consumed);
      }
    });

    function sendNext() {
      const next = pending[0];
      if (!next) return;
      const payload = Buffer.concat([Buffer.from(next.sql, "utf8"), Buffer.from([0])]);
      sock.write(payload, (err) => {
        if (err && !timedOut) {
          clearTimeout(timer);
          finish(new Error(`写入 observer 失败：${err.message}`));
        }
      });
    }
  });
}

// ---- HTTP 路由 ---------------------------------------------------------

function send(res, status, body, headers = {}) {
  res.writeHead(status, { "Cache-Control": "no-store", ...headers });
  res.end(body);
}

function sendJson(res, status, obj) {
  send(res, status, JSON.stringify(obj), { "Content-Type": MIME[".json"] });
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    let total = 0;
    const chunks = [];
    req.on("data", (c) => {
      total += c.length;
      if (total > 256 * 1024) {
        reject(new Error("请求体超过 256 KiB"));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on("end", () => {
      try {
        const txt = Buffer.concat(chunks).toString("utf8");
        resolve(txt ? JSON.parse(txt) : {});
      } catch (e) {
        reject(new Error("请求体不是合法 JSON"));
      }
    });
    req.on("error", reject);
  });
}

function serveStatic(req, res, pathname) {
  let rel = pathname === "/" ? "/index.html" : pathname;
  // 防止越权
  const full = path.normalize(path.join(STATIC_DIR, rel));
  if (!full.startsWith(STATIC_DIR)) {
    return sendJson(res, 403, { error: "forbidden" });
  }
  fs.stat(full, (err, st) => {
    if (err || !st.isFile()) return sendJson(res, 404, { error: "not found" });
    const ext = path.extname(full).toLowerCase();
    const type = MIME[ext] || "application/octet-stream";
    res.writeHead(200, { "Content-Type": type, "Cache-Control": "no-store" });
    fs.createReadStream(full).pipe(res);
  });
}

const server = http.createServer(async (req, res) => {
  const parsed = url.parse(req.url, true);
  const pathname = parsed.pathname;

  // CORS（本地不同端口之间）
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");
  if (req.method === "OPTIONS") return send(res, 204, "");

  if (req.method === "GET" && pathname === "/api/health") {
    // 简单 TCP 探活
    const sock = net.createConnection({ host: OBS_HOST, port: OBS_PORT });
    const probe = new Promise((resolve) => {
      const t = setTimeout(() => { sock.destroy(); resolve(false); }, 1500);
      sock.once("connect", () => { clearTimeout(t); sock.destroy(); resolve(true); });
      sock.once("error",   () => { clearTimeout(t); resolve(false); });
    });
    const ok = await probe;
    return sendJson(res, 200, {
      ok,
      observer: { host: OBS_HOST, port: OBS_PORT },
      frontend: { port: PORT },
    });
  }

  if (req.method === "POST" && pathname === "/api/sql") {
    let body;
    try {
      body = await readJson(req);
    } catch (e) {
      return sendJson(res, 400, { error: e.message });
    }
    const list = Array.isArray(body.statements) ? body.statements : null;
    if (!list) return sendJson(res, 400, { error: "缺少 statements 数组" });
    const stmts = normalizeStatements(list);
    if (stmts.length === 0) {
      return sendJson(res, 200, { ok: true, results: [] });
    }
    const r = await runOnObserver(stmts);
    writeLog(
      body.demoId ? `demo: ${body.demoId}` : `ad-hoc (${stmts.length} stmts)`,
      r.results,
      body.demoId ? null : null,
    );
    return sendJson(res, 200, r);
  }

  if (req.method === "GET" && (pathname === "/api/log" || pathname === "/api/log/download")) {
    // 下载当前 session 的日志文件
    if (!fs.existsSync(LOG_FILE)) return sendJson(res, 404, { error: "no log" });
    const fname = `miniob-demo-${new Date().toISOString().replace(/[:.]/g, "-")}.log`;
    res.writeHead(200, {
      "Content-Type": "text/plain; charset=utf-8",
      "Content-Disposition": `attachment; filename="${fname}"`,
    });
    fs.createReadStream(LOG_FILE).pipe(res);
    return;
  }

  if (req.method === "GET" && pathname === "/api/log/tail") {
    // 返回最近 N 行（默认 80），用于前端实时同步
    const n = Math.max(1, Math.min(2000, Number(parsed.query.n) || 80));
    if (!fs.existsSync(LOG_FILE)) return sendJson(res, 200, { ok: true, lines: [] });
    fs.readFile(LOG_FILE, "utf8", (err, txt) => {
      if (err) return sendJson(res, 500, { error: err.message });
      const all = txt.split("\n");
      sendJson(res, 200, { ok: true, lines: all.slice(Math.max(0, all.length - n)) });
    });
    return;
  }

  if (req.method === "GET") {
    return serveStatic(req, res, pathname);
  }

  sendJson(res, 405, { error: "method not allowed" });
});

async function main() {
  await startManagedObserver();
  server.listen(PORT, "127.0.0.1", () => {
    console.log(`[frontend] http://127.0.0.1:${PORT}`);
    console.log(`[frontend] → observer tcp://${OBS_HOST}:${OBS_PORT}`);
    if (managedObserver) {
      console.log(`[frontend] managed observer cwd: ${OBS_RUNTIME}`);
    }
    console.log(`[frontend] session log: ${LOG_FILE}`);
  });
}

main().catch((err) => {
  console.error("[frontend] startup failed:", err);
  process.exit(1);
});
