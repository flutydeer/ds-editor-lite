@echo off

@Rem 根据实际情况修改
set QT_DIR=C:\Apps\Qt\6.8.2\msvc2022_64
set Qt6_DIR=%QT_DIR%
set CMAKE_PREFIX_PATH=%QT_DIR%
set VCPKG_KEEP_ENV_VARS=QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH

set FEATURE_ARG=
if "%1"=="cuda12" (
    set FEATURE_ARG=-- --x-feature=cuda12
)

@Rem vcpkg-workspace.cmake 从 https://github.com/stdware/dev-scripts 下载到 scripts 目录，并据此设置 QT_DIR 与 Qt6_DIR
cmake -P scripts/vcpkg-workspace.cmake install --qt-dir %QT_DIR% --triplet x64-windows %FEATURE_ARG%
pause