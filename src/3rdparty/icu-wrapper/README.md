# ICU WRAPPER

一个基于 Qt 6 的 BCP 47 语言匹配工具。沿请求标签的显式父链从最具体的标签开始，
逐级对规范化后的候选做不区分大小写的精确匹配。同一级有多个候选时按输入顺序选择。

```cpp
#include <IcuWrapper/IcuWrapper.h>

const QString matched = IcuWrapper::bestMatch(
    QStringLiteral("zh-Hans-CN"),
    {QStringLiteral("en-US"), QStringLiteral("zh-Hans")});
```

匹配成功时返回 `available` 中的原始字符串。输入为空或无法匹配时返回空 `QString`。
输入中的首尾空白、POSIX 编码后缀（如 `.UTF-8`）、关键字后缀
（如 `@calendar=...`）和下划线会在匹配前进行规范化。

## 构建

需要 CMake 3.17+ 和 Qt 6.2+。Windows 使用 Windows SDK 提供的 ICU，Linux 需要 ICU 65+，
macOS 使用系统 Foundation framework。构建要求见 [CMake 配置](CMakeLists.txt)。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```
