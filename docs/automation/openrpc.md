# JSON-RPC 接口描述与服务器开关

## 服务入口

编辑器的自动化 HTTP 服务只监听 `127.0.0.1`。两个协议共用配置端口、公共 Registry、控制层级、
自定义权限、文件访问范围及传输限制。

| 宿主 | MCP | JSON-RPC |
|---|---|---|
| GUI | 由 MCP 开关控制 | 由 JSON-RPC 开关控制 |
| Headless | 由 MCP 开关控制 | 始终开启 |

设置面板分别提供“启用 MCP 服务器”和“启用 JSON-RPC 服务器”，对应 `mcpEnabled` 和
`rpcEnabled` 配置键，默认均关闭。旧配置缺少 `rpcEnabled` 时按关闭处理，不跟随 MCP 开关。
GUI 中两个开关都关闭时停止监听，任一协议开启时共用同一个 HTTP 监听器。

`--mcp` / `--no-mcp` 和 `--rpc` / `--no-rpc` 分别覆盖对应协议的持久化设置，只影响本次启动。
被命令行覆盖的设置项会锁定，不回写配置。相反的同协议参数不能同时使用，不同协议可以自由组合。
例如 `--rpc --no-mcp` 只开放 JSON-RPC，`--mcp --no-rpc` 在 GUI 中只开放 MCP。

`--headless` 强制开启 JSON-RPC，即使配置关闭或传入 `--no-rpc` 也仍然开启，启动失败按致命错误
处理。MCP 继续由自身开关控制。两个协议仍共用 `--control-port` 和 `--control-level`。
QLocal Bootstrap 的 `server_endpoint` 继续指向 MCP，避免改变 Connector 的发现协议。

| HTTP 入口 | 用途 |
|---|---|
| `/mcp` | MCP |
| `POST /automation/v1` | 原生 JSON-RPC |
| `GET /automation/v1/openrpc.json` | 当前可发现接口的 OpenRPC 文档 |

设置面板的“连接配置”包含 JSON-RPC 卡片，可复制调用端点和接口描述地址。服务器关闭时仍可
复制配置地址。运行中改变端口后，复制的地址使用新端口。

## 发现接口

向 JSON-RPC 端点发送以下请求，设置 `Content-Type: application/json` 和
`Accept: application/json`：

```json
{"jsonrpc":"2.0","id":1,"method":"rpc.discover"}
```

`result` 与文档 GET 入口返回同一个 JSON 对象。`params` 可省略或传 `{}`，其他参数会返回
`-32602`。文档 GET 不要求 Content-Type，不需要 MCP 会话或版本协商。

发现集合由 `PublicAutomationRegistry::enabledContracts()` 产生，与 MCP `tools/list` 共用
宿主和权限过滤。`rpc.discover` 额外出现在 OpenRPC 方法列表中，但不属于公共业务工具，
不改变 MCP 工具目录或公共工具版本。已知业务方法的实际执行仍由 Registry 完成。

文档入口复用 Native 请求的主线程派发、并发限制、超时和响应大小限制，沿用 Host/Origin
校验及 `Cache-Control: no-store`。权限变化后重新读取文档即可取得当前集合。

## 契约来源与 Schema

采用 [OpenRPC 1.4.1](https://github.com/open-rpc/spec/releases/tag/v1.4.1)，描述由
`AutomationWire::ToolContract` 自动生成。名称、说明、分类、输入、输出和工具元数据均来自
现有公共契约，不维护第二份业务接口目录。

- `openrpc` 表示描述规范版本。
- `info.title/version` 复用产品元数据。
- `x-ds-toolset-version` 表示公共业务工具版本。
- `servers[].url` 是当前 JSON-RPC 调用地址。
- `params` 使用 `by-name`，必填参数排在可选参数之前。
- `result.schema` 引用完整的返回值 Schema，包含异步调用返回的既有 `task_id`。
- `x-ds-tool` 复用 MCP 工具元数据，包含控制层级、宿主、同步模式和 `value_sources`。
- `x-ds-annotations` 保留既有工具行为提示。

公共 Schema 保持 JSON Schema 2020-12。OpenRPC 标准字段使用 Draft 7 投影，转换器只处理
项目校验器支持的关键字子集，将 `$defs` 转为 `definitions`，将引用重定位到文档内的
`components.schemas`。带约束的 `$ref` 转为 `allOf`，保留 Draft 7 原本会忽略的相邻约束。
当前不支持导出声明 `$id` 或 `$anchor` 的 Schema，无法转换时返回明确错误。

转换按 Schema 节点进行。`default`、`examples` 和属性名称等业务数据不会被全局替换。
转换后的 Schema 包括完整输入对象和输出对象，引用无需从网络加载。

OpenRPC 的逐参数列表不能完整表达 `tasks.*` 的条件必填、`reveal_clips` 的互斥参数及更新接口的
`minProperties`。因此每个方法同时包含原样的 `x-ds-input-schema`。客户端需要完整校验时，
应将它作为独立的 2020-12 Schema 校验整个 `params` 对象，内部引用相对于该 Schema 根解析。
标准参数列表汇总分支字段，只把所有分支都要求的字段标为必填，不能替代完整输入校验。

## 调用与错误

Native 沿用既有受限调用约定，文档中的 `x-ds-jsonrpc-profile` 记录这些限制：

- 每次 HTTP POST 接受一个请求对象，不接受 batch。
- `id` 为字符串或 JSON 安全整数，不接受缺失及 `null`。
- `params` 必须为对象，省略等价于 `{}`。
- 不支持 notification，无 ID 请求返回 `-32600`，不同于标准 notification 的无响应语义。
- 成功结果直接放入 `result`，没有 MCP 的 `content` 或 `structuredContent` 包装。

JSON-RPC 解析、请求格式、未知方法、参数和内部错误分别使用 `-32700`、`-32600`、`-32601`、
`-32602`、`-32603`。其他业务错误使用 `-32000`，具体错误由 `error.data.code` 和实际
`message` 描述。Host/Origin 拒绝、媒体类型、请求大小、繁忙和超时等传输错误仍使用 HTTP
状态码。通用 OpenRPC 工具可以读取标准字段，完整参数约束需要支持上述扩展。

本契约更新了三期文档中“GUI 不开放 Native”和“Native 不提供发现”的约定。Headless 必须
存在 Native 入口、MCP 可单独关闭的约定继续有效。
