# 开发者选项页面（Developer Options）实现说明

## 概述

本文档记录了"开发者选项"设置页面的第一阶段实现：添加诊断信息输出开关，用于控制 `MainWindow` 中的 `EventDiagFilter` 事件循环性能诊断日志输出。

## 涉及文件

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/app/Model/AppOptions/Options/DeveloperOption.h` | 开发者选项数据类，继承 `IOption`，使用 `LITE_OPTION_ITEM` 宏声明配置项 |
| `src/app/Model/AppOptions/Options/DeveloperOption.cpp` | 开发者选项的 JSON 序列化/反序列化 |
| `src/app/UI/Dialogs/Options/Pages/DeveloperPage.h` | 开发者选项设置页面 UI 类 |
| `src/app/UI/Dialogs/Options/Pages/DeveloperPage.cpp` | 页面 UI 实现，包含诊断开关控件 |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `src/app/Global/AppOptionsGlobal.h` | 取消注释 `DeveloperOptions` 枚举值，将其从隐藏选项提升为正式选项 |
| `src/app/Model/AppOptions/AppOptions.h` | 添加 `DeveloperOption` 头文件引用、成员变量和访问器方法声明 |
| `src/app/Model/AppOptions/AppOptions.cpp` | 添加 `DeveloperOption` 的加载、保存逻辑和访问器实现 |
| `src/app/UI/Dialogs/Options/AppOptionsDialog.h` | 添加 `DeveloperPage` 前向声明、页面名称和成员变量 |
| `src/app/UI/Dialogs/Options/AppOptionsDialog.cpp` | 集成 `DeveloperPage`：实例化、添加到 `QStackedWidget` 和页面列表 |
| `src/app/UI/Window/MainWindow.h` | 添加 `updateDiagnosticFilter()` 方法声明和 `m_eventDiagFilter` 成员变量 |
| `src/app/UI/Window/MainWindow.cpp` | 将原本无条件安装 `EventDiagFilter` 改为根据配置选项有条件安装/卸载 |
| `src/app/UI/Views/MainTitleBar/MainMenuView.cpp` | 在 Options 菜单中添加"Developer Options..."入口，用分隔线与常规选项隔开 |

## 架构设计

### 配置数据层

`DeveloperOption` 遵循现有 `Option` 类的实现模式：

```cpp
class DeveloperOption final : public IOption {
public:
    explicit DeveloperOption() : IOption("developer") {};
    void load(const QJsonObject &object) override;
    void save(QJsonObject &object) override;
    LITE_OPTION_ITEM(bool, enableDiagnostics, false)
};
```

- 选项键名：`"developer"`
- 配置项：`enableDiagnostics`（默认值 `false`，即默认关闭）
- 使用 `LITE_OPTION_ITEM` 宏自动生成 JSON key 和序列化辅助方法
- 持久化路径：`appConfig.json` 中的 `developer` 节点

### UI 层

`DeveloperPage` 继承 `IOptionPage`（实际继承 `QScrollArea`），遵循与其他页面相同的实现模式：

- 使用 `OptionListCard` 作为容器卡片
- 使用 `SwitchButton` 作为开关控件
- `modifyOption()` 在开关切换时自动保存配置并通知
- 页面标题为 "Developer Options"，在设置对话框标签列表中的排序位于 Inference 之后

### 与 MainWindow 的联动

`MainWindow` 原有的 `EventDiagFilter` 是临时添加到 `MainWindow.cpp` 中的一个诊断事件过滤器，用于每秒打印事件循环的性能统计信息（事件频率、绘制耗时、积压情况等）。原本在每次启动时无条件安装：

```cpp
// 旧代码（已移除）
qApp->installEventFilter(new EventDiagFilter(this));
```

现在改为：

1. `MainWindow` 构造时调用 `updateDiagnosticFilter()`
2. 该方法检查 `appOptions->developer()->enableDiagnostics` 的值
3. 如果为 `true` 且过滤器未安装，创建并安装 `EventDiagFilter`
4. 如果为 `false` 且过滤器已安装，移除并销毁 `EventDiagFilter`
5. `MainWindow` 监听 `AppOptions::optionsChanged` 信号，当 `DeveloperOptions` 变化时自动更新过滤器状态，无需重启应用即可实时生效

### 菜单入口

除了通过设置对话框的侧边标签页访问外，还通过 `MainMenuView::buildOptionsMenu()` 添加了菜单栏快捷入口：

```
Options > Developer Options...
```

点击后直接打开设置对话框并定位到 Developer Options 页面。菜单入口与常规设置项之间用分隔线隔开（`menuOptions->addSeparator()`）。

## 配置 JSON 格式

```json
{
    "developer": {
        "enableDiagnostics": false
    }
}
```

## 后续扩展方向

目标是在开发者选项页面中逐步添加更多对开发者有用的辅助功能，例如：

- 内部调试面板开关
- 性能分析工具入口
- 实验性功能开关
- 日志级别调整
- 等等

添加新选项时，在 `DeveloperOption` 中使用 `LITE_OPTION_ITEM` 宏声明新的配置项，在 `DeveloperPage` 中添加对应的 UI 控件即可。

> 完整的添加选项页面流程请参见 Skill：`.trae/skills/add-option-page/SKILL.md`
