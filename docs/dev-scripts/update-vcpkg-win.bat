@echo off

@Rem 根据实际情况修改
set QT_DIR=C:\Qt\6.7.2\msvc2019_64
set Qt6_DIR=%QT_DIR%
set CMAKE_PREFIX_PATH=%QT_DIR%
set VCPKG_KEEP_ENV_VARS=QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH

cd vcpkg
vcpkg install --x-manifest-root=../scripts/vcpkg-manifest --x-install-root=./installed --triplet=x64-windows
pause