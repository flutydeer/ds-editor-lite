# ICU WRAPPER

一个基于 Qt 6 的 BCP 47 语言匹配工具。Windows 和 Linux 使用 ICU，macOS
使用 `NSBundle` 的系统语言匹配 API。

```cpp
#include <IcuWrapper/IcuWrapper.h>

const QString matched = IcuWrapper::bestMatch(
    QStringLiteral("zh-CN"),
    {QStringLiteral("en-US"), QStringLiteral("zh-Hans")});
```

匹配成功时返回 `available` 中的原始字符串；输入为空、语言标签无效或没有有意义的
匹配时返回空 `QString`。输入中的空白、POSIX 编码后缀（如 `.UTF-8`）、关键字后缀
（如 `@calendar=...`）和下划线会在匹配前进行规范化。

## 构建

需要 CMake 3.21+、Qt 6.2+。Windows/Linux 还需要 ICU 65+；macOS 使用系统
Foundation framework。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```
