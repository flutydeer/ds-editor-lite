# spec 2.4 声库与验证

| 内容 | 文档 |
| --- | --- |
| 依赖、运行时与当前边界 | [接入状态](../plans/synthrt-main-migration.md) |
| 模块分工与部署契约 | [接入指南](../guides/synthrt-wolf-otter-integration.md) |
| 声库声明与交付要求 | [声库要点](packaging-voicebank-essentials.md) |
| 构建、CI 假声库与真实声库验收 | [验证指南](developer-repro.md) |

本仓消费已符合 spec 2.4 的声库。CI 使用仓库自带假声库，
语言包由 vcpkg 和构建部署流程提供，不依赖声库转换或独立下载脚本。
