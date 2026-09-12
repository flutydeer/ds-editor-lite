# 测试声库

`voicebank-fixture.zip` 包含 `ci-fixture@1.0.0`，由普通资源加载、G2P、DiffSinger
推理插件与 ONNX Runtime 执行。它是本项目自制的确定性集成素材，不包含真实声库的
身份资料、训练权重或语料。

- 歌手：`fixture`；声线：`clear`、`soft`，具有不同的四维 embedding。
- 语言：`cmn`、`eng`；默认歌词：`la`。
- 私有 ChainG2P 字典将中文 `la` 或 `啦` 转为 `la`，英语 `la` 转为 `l aa`；中文默认
  用例使用 `啦` 验证 UTF-8 输入。S2P 分别通过
  字典和直接音素路径得到 `l a`、`l aa`，使用包内的小型起音规则和音素表。
- Duration、Pitch、Variance、Acoustic、Vocoder 均有可执行的 ONNX 图，三个预测阶段
  使用各自的语言编码器。张量的音素、音符、帧和采样点长度随实际输入变化。
- Duration 给出供生产流程按词时长归一化的正权重；Pitch 保留未重录区域并为重录区域
  生成简单音高；Variance 传递输入参数；Acoustic 生成随音高及声线变化的 mel；
  Vocoder 输出随输入基频变化的低音量正弦 PCM，采样率为 44100 Hz。

这些图验证资源兼容、语言路由、声线、任务调度、缓存及文件导出的实际接线；其输出
不是歌声，也不用于评价真实模型音质。GAME 与 RMVPE 不在此素材范围。

CMake 将 ZIP 解压至构建目录，测试默认使用它。`DSEL_TEST_VOICEBANK_ROOT` 可覆盖为
本机真实声库目录，同时必须提供 `DSEL_TEST_SINGER_ID`、`DSEL_TEST_LANGUAGE` 和
`DSEL_TEST_LYRIC`。配置的资源失效或推理失败均报告失败。
