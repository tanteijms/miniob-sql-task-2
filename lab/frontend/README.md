# MiniOB 验收展示前端

> 课程：数据库系统原理课程设计 — 基于 MiniOB 的数据库管理系统设计与实现
> 用途：本地验收时打开浏览器，点按钮触发对应功能的演示 SQL，输出在页面下方的控制台。

零依赖：Node.js 自带的 `http` + `net` + `fs` 即可，不需 `npm install`。

## 文件

| 文件 | 作用 |
|------|------|
| `server.js`  | Node HTTP 代理，把前端请求转给本地 MiniOB observer (TCP 6789) |
| `index.html` | 单页 UI |
| `style.css`  | 简单深色面板样式 |
| `app.js`     | 渲染按钮、调用 /api/sql、渲染输出 |
| `demos.js`   | 每个功能按钮对应的演示 SQL |

## 启动

### 默认方式：只启动前端

需要先有编译产物（`build/bin/observer` 或 `build_debug/bin/observer`）。在仓库根目录：

在仓库根目录：

```bash
node lab/frontend/server.js
# 自定义端口：PORT=8080 node lab/frontend/server.js
```

现在 `server.js` 会优先探测 `tcp://127.0.0.1:6789`：

- 如果本地 observer 已经在跑，就直接复用它。
- 如果还没跑，前端会自动拉起一个专供 demo 的 observer，工作目录固定在 `lab/frontend/runtime/observer-data`，避免污染平时开发/测试的数据目录。

输出 `[frontend] http://127.0.0.1:3001` 后浏览器打开 `http://127.0.0.1:3001` 即可（如需改端口：`PORT=8080 node lab/frontend/server.js`）。

### 手动方式：自己启动 observer

如果你想继续手动管理 observer，也可以：

```bash
./build/bin/observer -f etc/observer.ini -p 6789 -P plain
OBSERVER_AUTO_START=0 node lab/frontend/server.js
```

> 端口 6789 是 MiniOB 源码里的 `PORT_DEFAULT`；如需改，前端用环境变量 `OBSERVER_PORT` 指定。
> 如需手动指定 observer 路径或配置文件路径，可用 `OBSERVER_BIN=/abs/path/to/observer`、`OBSERVER_CONFIG=/abs/path/to/observer.ini`。

## 使用

- **功能按钮**：每行一个已实现功能（basic / drop-table / update / date / aggregation / like / join / function / order-by / group-by / multi-index / unique / null / simple-sub-query / complex-sub-query / update-select / big-query / big-write），点击会把对应 SQL 一条条发到 observer，结果逐条追加到下方控制台。
- **预期失败说明**：像“删除不存在的表”“UNIQUE 重复插入”“多行标量子查询赋值”这类故意保留的失败场景，页面会显示 `EXPECTED FAILURE`，日志里会以 `EXPF` 标记，并附中文说明，表示这是演示设计而不是程序异常。
- **🔄 重置 demo 数据**：把 `demos.js` 里登记的 `demo_*` 表全部 DROP（不存在的表返回 FAILURE 是正常的）。**重跑同一按钮前必点**，否则表已存在会让 INSERT 重复入，导致 SELECT 出现多份数据。
- **📥 导出日志**：把后端 `session.log` 下载为 `.log` 文本（含时间戳 + demoId + 每条 SQL 的 SUCCESS/FAILURE + 完整输出），用于验收归档。
- **清空输出**：仅清空页面控制台，不动数据库和后端日志。
- **停止**：中断当前正在执行的请求。

前端会自动忽略 `demos.js` 中纯注释行（例如 `-- 标量子查询：...`），这些说明文字不会再被当成 SQL 发给 observer。
前端默认 SQL 超时已放宽到 `20000ms`，避免 `big-query` / `big-write` 这类长脚本在展示环境下被过早判定为超时；如需继续调整，可用 `SQL_TIMEOUT_MS=30000 node lab/frontend/server.js`。

按钮之间用不同表名（`demo_basic`、`demo_ag`、`demo_j1` …），可任意点、并行点，状态互不干扰。

## 后端日志

每次前端 POST `/api/sql` 后，对应的 SQL/结果会追加到 `lab/frontend/session.log`：

```
===== [2026-06-03 21:26:09.878] demo: update_select =====
  OK    | CREATE TABLE demo_us1(id INT, val INT);
         | SUCCESS
  EXPF  | UPDATE demo_us1 SET val = (SELECT score FROM demo_us2);
         | 说明: 预期失败：多行结果不能直接作为标量子查询赋值给 SET 右值。
         | FAILURE
```

server 启动时清空旧日志；服务进程结束后日志仍在。`📥 导出日志` 按钮通过 `/api/log/download` 把它打成带时间戳的 `.log` 文件下载。

## 协议说明

MiniOB plain 协议（`src/obclient/client.cpp`）：

- 客户端发送 `<sql>\0`
- 服务端回写 `<输出>\0`，输出可能是 `SUCCESS` / `FAILURE`，或表头+行

前端把每条 SQL 当作一次独立的请求/响应，复用同一条 TCP 连接串行收发。

## 常见问题

| 现象 | 排查 |
|------|------|
| 状态栏显示「observer 未启动」 | observer 没跑；或端口不是 6789（设置 `OBSERVER_PORT=xxxx node ...`） |
| 同一按钮点两次出现红色 FAILURE | 表已存在；先点「重置 demo 数据」 |
| observer 报记录长度不匹配 | 说明命中了旧版本遗留的数据页；删掉旧数据目录，或直接使用前端自动托管的 `lab/frontend/runtime/observer-data` |
| big-query / big-write 等待时间长 | 正常：分别 1608 / 504 条 SQL，单连接串行；前端会显示「执行中…」 |
| 改完 demos.js 没生效 | 浏览器强制刷新（Cmd+Shift+R）即可，server 本身不缓存 |

## 与 `yys/scripts/dev.sh` 的关系

`dev.sh` 默认用 Unix socket（`-s`），本前端用 TCP（`-p 6789 -P plain`）。两者**不能同时跑同一个 observer 实例**——会争抢数据文件锁。要切换：

- 跑前端：`pkill -f "observer -f" ; ./build/bin/observer -f etc/observer.ini -p 6789 -P plain &`
- 跑 `dev.sh`：`pkill -f "observer -f" ; ./yys/scripts/dev.sh server`
