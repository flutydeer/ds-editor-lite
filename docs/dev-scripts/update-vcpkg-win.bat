@echo off

@Rem 根据实际情况修改
set QT_DIR=C:\Apps\Qt\6.8.2\msvc2022_64
set Qt6_DIR=%QT_DIR%
set CMAKE_PREFIX_PATH=%QT_DIR%
set VCPKG_KEEP_ENV_VARS=QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH

set FEATURE_ARG=
if "%1"=="cuda12" (
    set FEATURE_ARG=--x-feature=cuda12
) else if "%1"=="cuda11" (
    set FEATURE_ARG=--x-feature=cuda11
)

pushd vcpkg
vcpkg install --x-manifest-root=../scripts/vcpkg-manifest --x-install-root=./installed --triplet=x64-windows %FEATURE_ARG%
popd
pause