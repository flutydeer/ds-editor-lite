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

外部目录中的主题只在对应子目录存在 `manifest.json` 时覆盖同名内置主题；没有外部覆盖时继续使用资源内置主题。一次加载只使用一个来源，不会把外部文件和内置文件混合。主题清单中的资源路径必须是主题目录内的相对路径。

可通过 `DS_EDITOR_THEME` 指定启动时加载的主题 ID。例如预览浅色主题：

```powershell
$env:DS_EDITOR_THEME = "lite-light"
```

启动主题的优先级为 `DS_EDITOR_THEME`、外观设置中保存的内置主题、`lite-dark`。环境变量只覆盖
本次启动的初始主题，应用启动后仍可在“选项 > 外观”中即时切换并保存浅色或深色主题。

## 手动重载

应用启动后按 `Ctrl+Shift+F5` 会重新读取当前主题。加载失败时保留当前已应用的主题，并通过 Toast 和日志报告错误，因此可以直接修改外部 QSS/JSON 后反复验证。

## 调色阶段约定

`colors.json` 管理 UI 语义 token；`app-color-palette.json` 管理轨道、片段和说话人等业务颜色。两者保持独立。调色时先修改外部主题目录中的语义 token 和 QSS，确认控件状态与布局无异常后，再将稳定结果合并回资源主题。

`lite-light` 当前的 12 个业务色只是为满足主题完整性校验而复制的临时占位值，不属于浅色主题设计稿。视觉验收和发布前必须替换 `lite-light/app-color-palette.json`。
