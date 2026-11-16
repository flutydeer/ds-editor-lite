# 主题开发工作流

## 外部主题目录

设置 `DS_EDITOR_THEME_DIR` 后，主题加载器会把该目录视为主题根目录。每个主题必须放在独立子目录中，并包含自己的 `manifest.json`：

```text
themes/
  lite-dark/
    manifest.json
    colors.json
    app-color-palette.json
    *.qss
```

外部目录中的主题只在对应子目录存在 `manifest.json` 时覆盖同名内置主题。没有外部覆盖时继续使用资源内置主题。一次加载只使用一个来源，不会把外部文件和内置文件混合。主题清单中的资源路径必须是主题目录内的相对路径。

可通过 `DS_EDITOR_THEME` 指定启动时加载的主题 ID。例如预览浅色主题：

```powershell
$env:DS_EDITOR_THEME = "lite-light"
```

启动时优先读取 `DS_EDITOR_THEME`，未设置时读取保存的外观设置。默认跟随系统配色，主题加载失败时尝试 `lite-dark`。环境变量只指定初始主题。在「选项 → 外观」可即时切换并保存「跟随系统」「浅色」或「深色」。

## 手动重载

应用启动后按 `Ctrl+Shift+F5` 会重新读取当前主题。加载失败时保留当前已应用的主题，并通过 Toast 和日志报告错误，因此可以直接修改外部 QSS/JSON 后反复验证。

## 调色

`colors.json` 管理 UI 语义 token。`app-color-palette.json` 管理轨道、片段和说话人的可选颜色。修改控件样式时优先使用语义 token，保留两类色值的独立配置。内置主题的实际色值见 [`lite-light`](../../src/app/Resources/theme/lite-light) 和 [`lite-dark`](../../src/app/Resources/theme/lite-dark) 资源目录。
