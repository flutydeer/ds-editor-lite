# 自动化

编辑器支持 MCP 和 JSON-RPC。两个协议的启停、访问地址、发现和错误格式见[服务与调用约定](openrpc.md)。

- MCP 客户端通过 `tools/list` 获取当前宿主和权限允许的工具。通过 Connector 连接时，可用
  `editor.tools.list`、`editor.tools.search` 和 `editor.tools.describe` 查询 Connector 开放的目标及其 Schema。
- JSON-RPC 客户端从 `GET /automation/v1/openrpc.json` 或 `rpc.discover` 获取当前接口描述。
- 工具的名称、参数和返回结构由[公共契约](../../src/libs/AutomationWire/PublicToolContract.cpp)定义。测试运行方式见[测试指南](04-testing-and-ci/test-plan.md)。

## 阶段记录

以下目录保留各阶段的设计、实现和验收资料。记录中的工具清单、版本和测试结果对应各自的阶段基线。

- [Automation Facade](01-automation-facade/implementation-plan.md)
- [MCP 服务与 Connector](02-mcp-server-and-connector/implementation-plan.md)
- [Headless 与 JSON-RPC](03-headless-mode/implementation-plan.md)
- [测试体系与 CI](04-testing-and-ci/implementation-plan.md)
