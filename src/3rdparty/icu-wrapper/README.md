# ICU WRAPPER

一个基于 Qt 6 的 BCP 47 语言匹配工具。三个后端（Windows/Linux 的 ICU、macOS 的
Foundation 字符串原语）实现同一匹配算法：沿请求标签的显式父链最具体优先逐级、
规范化小写后精确比对；不使用任何平台自带的距离式语言匹配 API。

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

需要 CMake 3.17+、Qt 6.2+。Windows/Linux 还需要 ICU 65+；macOS 使用系统
Foundation framework。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```
