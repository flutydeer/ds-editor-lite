# 一期 Automation Facade 测试证据索引

本索引只记录匿名证据编号和内容摘要，不记录用户目录、工程目录、临时目录或日志的
本机绝对路径。原始证据保留在测试主机的隔离区域；SHA-256 用于确认内容未被替换。

| 证据编号 | 轮次 | 类型 | SHA-256 |
|---|---:|---|---|
| `E-R00-BEFORE` | 00 | 推理竞态修复前进程日志 | `fdf25d004cc8b5a4cd27822e4a212f152ce0c74f111769baaac4918f0e0694d9` |
| `E-R00-AFTER` | 00 | 推理竞态修复后进程日志 | `3b2bb8b208fad9703aced37151f5f48b1500413da33e2a6b0f9be8d3068ea469` |
| `E-R01-BUILD` | 01 | Modifier 输入桥首次全目标构建日志 | `17108e6d4572b5a6fbe968703d7cccc905efdbaac362d549e227816bc4da0cb7` |
| `E-R01-RUNTIME` | 01 | Modifier 输入桥首次资格验证进程日志 | `c5daabd9e59c85522590625104255d007579592811cb1ad197012b1386bce77e` |
| `E-R02-RUNTIME` | 02 | Modifier 输入桥修正后资格验证进程日志 | `1386307954375fc09b9405198e2f1dc80bf72bd5162a9aade68d1968b40ca027` |
| `E-R02-BRIDGE` | 02 | 已脱敏且已提交的 Modifier 桥关键日志 | `72078cf99bce1d844283e0dc2128151f918a6a10283adb111524b72e81dcd875` |
| `E-R03-ISOLATION` | 03 | 已脱敏且已提交的缓存隔离失败统计 | `74f0d913ad1c69bfe04c1260d7e69c511a470decf1a6595f30aa0aa6883900f8` |

`E-R02-RUNTIME` 的 SHA-256 在进程仍运行时取得，只代表当时的日志快照；稳定、可直接复核的
桥资格证据由 `E-R02-BRIDGE` 取代。原条目保留，不作删除。
