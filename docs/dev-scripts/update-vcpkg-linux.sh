export QT_DIR="/home/OrangeCat/Qt/6.8.2/gcc_64" # 根据实际情况修改
export PATH=$QT_DIR/bin:$PATH
export LD_LIBRARY_PATH=$QT_DIR/lib:$LD_LIBRARY_PATH
export Qt6_DIR=$QT_DIR
export VCPKG_KEEP_ENV_VARS="QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH"

FEATURE_ARGS=()
if [ "$1" = "cuda12" ]; then
    FEATURE_ARGS=(-- --x-feature=cuda12)
fi

# vcpkg-workspace.cmake 从 https://github.com/stdware/dev-scripts 下载到 scripts 目录，并据此设置 QT_DIR 与 Qt6_DIR
cmake -P scripts/vcpkg-workspace.cmake install --qt-dir "$QT_DIR" --triplet x64-linux "${FEATURE_ARGS[@]}"
