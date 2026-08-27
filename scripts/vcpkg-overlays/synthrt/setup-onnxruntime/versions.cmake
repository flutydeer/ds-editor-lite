# GitHub 发布的 CPU/CUDA 构建版本（1.27.0 起官方移除 CUDA12 预构建，故锁在 1.26.0）
set(_version_ort "1.26.0")
# 官方 DirectML 运行时最高只发布到 1.24.4（NuGet），此处与 _version_ort 解耦
set(_version_ort_dml "1.24.4")
set(_version_dml "1.15.4")
