# 开发者选项页面（Developer Options）— 设计文档

「选项 → 开发者选项」集中管理调试、诊断与实验功能。事件循环诊断开关控制 `MainWindow` 的 `EventDiagFilter`，用于观察事件频率、绘制耗时和积压情况。

## 架构设计

### 配置数据层

`DeveloperOption` 遵循现有 `Option` 类模式（键名 `"developer"`，持久化在 `appConfig.json` 的 `developer` 节点）：

```cpp
class DeveloperOption final : public IOption {
public:
    explicit DeveloperOption() : IOption("developer") {};
    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;
    LITE_OPTION_ITEM(bool, enableDiagnostics, false)
};
```

配置项用 `LITE_OPTION_ITEM` 宏声明，自动生成 JSON key 与序列化辅助方法。

### UI 层

[`DeveloperPage`](../../src/app/UI/Dialogs/Options/Pages/DeveloperPage.cpp) 继承 `IOptionPage`。页面通过设置服务的 `updateDeveloper()` 提交修改，提交失败时从变更前的快照恢复控件显示。

### 与 MainWindow 的联动

`MainWindow` 按 `enableDiagnostics` 决定是否安装 `EventDiagFilter`。

1. `MainWindow` 构造时调用 `updateDiagnosticFilter()`。
2. 按 `appOptions->developer()->enableDiagnostics` 决定安装/卸载过滤器。
3. 监听 `AppOptions::optionsChanged`，`DeveloperOptions` 变化时实时生效，无需重启。

## 配置 JSON 格式

```json
{
    "developer": {
        "enableDiagnostics": false
    }
}
```

选项读写与页面接入见[首选项页面开发指南](../guides/preferences-guide.md)。开发者分组的持久字段由 [`DeveloperOption`](../../src/app/Model/AppOptions/Options/DeveloperOption.h) 定义，通过公共设置服务写入的字段须在 DTO 和适配器之间双向映射。
