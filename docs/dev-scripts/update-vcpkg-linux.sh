export QT_DIR="/home/OrangeCat/Qt/6.7.2/gcc_64" # 根据实际情况修改
export PATH=$QT_DIR/bin:$PATH
export LD_LIBRARY_PATH=$QTDIR/lib:$LD_LIBRARY_PATH
export Qt6_DIR=$QT_DIR
export VCPKG_KEEP_ENV_VARS="QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH"
cd vcpkg
./vcpkg install \
    --x-manifest-root=../scripts/vcpkg-manifest \
    --x-install-root=./installed \
    --triplet=x64-linux