# 开发者选项页面（Developer Options）— 设计文档

> 状态：✅ 第一阶段实施完成（诊断信息输出开关）
> 本文是设计说明；涉及文件修改清单已移除。

## 概述

"开发者选项"设置页面用于集中管理对开发者有用的辅助功能开关。第一阶段实现：**诊断信息输出开关**，控制 `MainWindow` 中的 `EventDiagFilter` 事件循环性能诊断日志（每秒打印事件频率、绘制耗时、积压情况等）。

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

`DeveloperPage` 继承 `IOptionPage`，遵循其他页面模式：`OptionListCard` 容器 + `SwitchButton` 开关；`modifyOption()` 在切换时自动保存并通知。页面在设置对话框标签列表中位于 Inference 之后；菜单入口 `Options > Developer Options...`（与常规设置项用分隔线隔开）。

### 与 MainWindow 的联动

`EventDiagFilter` 不再无条件安装：

1. `MainWindow` 构造时调用 `updateDiagnosticFilter()`；
2. 按 `appOptions->developer()->enableDiagnostics` 决定安装/卸载过滤器；
3. 监听 `AppOptions::optionsChanged`，`DeveloperOptions` 变化时实时生效，无需重启。

## 配置 JSON 格式

```json
{
    "developer": {
        "enableDiagnostics": false
    }
}
```

## 后续扩展方向（未实施）

- 内部调试面板开关
- 性能分析工具入口
- 实验性功能开关
- 日志级别调整

添加新选项：在 `DeveloperOption` 中用 `LITE_OPTION_ITEM` 声明配置项 + 在 `DeveloperPage` 添加对应 UI 控件即可。

## 关键文件

- `src/app/Model/AppOptions/Options/DeveloperOption.h/.cpp`
- `src/app/UI/Dialogs/Options/Pages/DeveloperPage.h/.cpp`
- `src/app/UI/Window/MainWindow.cpp`（`updateDiagnosticFilter()`）
- `src/app/UI/Views/MainTitleBar/MainMenuView.cpp`（菜单入口）
- 完整添加选项页面流程见项目 skill `add-option-page`
