# 对话框按钮的平台布局

保留现有 Button / AccentButton 绘制、业务回调和默认键，只让
QDialogButtonBox 根据角色处理横向顺序及 Tab 顺序。局部 `button-layout`
不受 AppEnvironment 为绘制兼容性安装的 Windows style 影响。

## 平台策略

- Windows：Qt WinLayout，确认在取消左侧。
- macOS：Qt MacLayout，取消在确认左侧，辅助 ActionRole 在左侧独立分组。
- Linux：Qt GnomeLayout，明确采用 GNOME 的取消在前惯例；不宣称自动跟随 KDE。

从左到右的主要组合：

| 场景 | Windows | macOS | Linux |
| --- | --- | --- | --- |
| 普通确认 | OK / Cancel | Cancel / OK | Cancel / OK |
| 后台进度 | Hide / Cancel | Cancel / Hide | Cancel / Hide |
| 导出提交 | Export / Cancel | Cancel / Export | Cancel / Export |
| 导出成功 | Open Folder / Close | Open Folder / 留白 / Close | Open Folder / Close |
| 重置、清理、停止动态混合确认 | 执行 / Cancel | Cancel / 执行 | Cancel / 执行 |
| 保存更改 | Save / Don't save / Cancel | Don't save / 间隔 / Cancel / Save | Don't save / Cancel / Save |

导出页的 Dry Run、帮助、警告仍在现有辅助区域，不与提交按钮混排。
重置、清理、停止动态混合是对用户已选择操作的确认，使用 AcceptRole；
Don't save 是保存流程中的破坏性替代选项，使用 DestructiveRole。
Open Folder 使用 ActionRole，因为它不关闭结果窗口；Close 使用 RejectRole。

## 行为边界

不连接按钮盒的 accepted/rejected 信号来自动关闭窗口，避免进度取消被误当作关闭。
MessageDialog 仍通过原 button ID 返回结果，与排列位置无关。
保留原始初始默认键：普通确认、保存、导出、Hide，以及导出成功后的 Open Folder；
缓存清理保持 Cancel，音素重置和停止动态混合保持原确认按钮。
本次不是默认键或危险操作确认策略的全面重设计：后两项不代表完全遵循
GNOME 对破坏性操作不设确认默认键的建议，缓存清理也保留了与 Apple 建议不同的
Cancel 默认键，避免平台排序迁移暗中改变已有 Enter 行为。

## 参考

- [Apple Alerts](https://developer.apple.com/design/human-interface-guidelines/alerts)：
  主要动作在行尾；区分主动发起的破坏操作确认与意外的破坏性替代选项。
- [Microsoft Win32 Dialog Boxes](https://learn.microsoft.com/en-us/windows/win32/uxguide/win-dialog-box#commit-buttons)：
  提交按钮靠右，执行、否定、取消的顺序。此指南源于 Windows 7，并非最新 WinUI 外观规范。
- [GNOME Dialogs](https://developer.gnome.org/hig/patterns/feedback/dialogs.html)：
  从左到右的语言中取消先于确认。
- [Qt QDialogButtonBox](https://doc.qt.io/qt-6/qdialogbuttonbox.html)：角色及平台布局。

布局测试在每个运行平台强制三种 Qt 策略并验证位置、主题切换、保存的替代按钮、
辅助按钮、默认键、Enter/Escape、按钮重新挂接和取消不关闭。
强制布局策略测试不替代实际 macOS、Windows、Linux CI。
