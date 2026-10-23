# spec 2.4 构建与验证

## 准备依赖与构建

环境要求和依赖安装位置以 [AGENTS.md](../../AGENTS.md) 为准。
依赖须安装到 `vcpkg/installed`，配置使用仓库提供的 presets。

Windows 可在仓库根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Dependencies
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode ConfigureAndBuild -Preset tests
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/skills/scripts/run-cmake-preset.ps1 -Mode Test -Preset local
```

脚本在同一会话初始化 VS 与 Qt。其他平台准备好 Qt 和 vcpkg 后，使用
`cmake --preset tests`、`cmake --build --preset tests` 与 `ctest --preset local`。
三平台 CI 的实际步骤见 [tests.yml](../../.github/workflows/tests.yml)。

## 默认 CI 假声库

[src/tests/resources/voicebank-fixture.zip](../../src/tests/resources/voicebank-fixture.zip)
直接保存 spec 2.4 声明、小型字典及确定性 ONNX 图。CMake 解压后供测试加载，
不在测试前运行声库转换器。后续格式调整应直接维护这份夹具。

它覆盖中英文 G2P、声线、时长、音高、唱法参数、声学、声码器及 WAV 输出。
输出为测试波形，不用于评价模型音质。详细内容见
[测试资源说明](../../src/tests/resources/README.md)。

仅验证资源套件时，在已准备好的开发环境运行：

```sh
cmake --build --preset tests --target TestModelResources
ctest --test-dir build/Tests -R "^TestModelResources$" --output-on-failure
```

## 真实声库与分析器

将已发布的 spec 2.4 声库解压为包目录，每个包根目录包含 `desc.json`。
编辑器搜索路径可指向包目录或其父目录。语言依赖由已部署的 `wolf/packages` 提供。

可选真实声库审计使用 CMake 缓存变量，不能用同名环境变量代替：

```sh
cmake --preset tests -DLITE_AUDIT_VOICEBANK="<声库包目录>" -DLITE_AUDIT_LANGUAGE_PACKAGES="<该声库的语言依赖目录>"
cmake --build --preset tests --target TestModelResources
```

真实音高和音符提取模型另通过 `LITE_OTTER_FIXTURES` 配置。
缺少这些外部资源只跳过对应用例，不跳过默认假声库的运行时回归。

## 编辑器验收

通过界面或 Ds Connector Lite 验收时，使用同一套已构建的编辑器与插件：

1. 读取声库列表，核对包身份、歌手、语言、声线和不可用原因。
2. 新建工程并选择歌手，插入带歌词与语言的音符，核对发音、音素及起音。
3. 完成各可用合成阶段，检查输出有限且具有有效音频能量。
4. 验证默认声线、可选声线及支持的混声方式，确认播放能够推进。
5. 导出 WAV，保存并重新打开 DSPX，核对音符、歌手、语言和混声设置。
6. 使用分析器包验证音高或音符提取，并检查取消和错误反馈。

未声明声线的歌手使用默认声线，自动化接口中的可空 speaker 应传 `null`。
模型执行验证应使用独立缓存或清理本次测试缓存，避免将旧缓存命中误作新模型执行成功。
歌词、声线或参数的检查以声库实际声明为准，不能用“包已列出”代替 G2P 与合成验证。
