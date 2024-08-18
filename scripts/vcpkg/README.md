# VCPKG Overlay

Custom overlay configuration for vcpkg.

## Use Pre-built Qt Libraries

### Get Overlay

Clone `vcpkg-overlay` in the subdirectory `script/vcpkg` of your repository.

```sh
git clone https://github.com/SineStriker/vcpkg-overlay.git scripts/vcpkg
```

### Make Manifest File 

Make configuration file `vcpkg.json` in subdirectory `scripts/vcpkg-manifest`.

```json
{
    "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
    "dependencies": [
        "some-depencency"
    ],
    "vcpkg-configuration": {
        "overlay-ports": [
            "../vcpkg/ports"
        ],
        "overlay-triplets": [
            "../vcpkg/triplets"
        ]
    }
}
```

### Setup Environment

You need to install Qt libraries first. (Take Qt5 as an example)

#### Windows

```sh
set QT_DIR=<dir> # directory `Qt5Config.cmake` locates
set Qt5_DIR=%QT_DIR%
set VCPKG_KEEP_ENV_VARS=QT_DIR;Qt5_DIR

git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat

vcpkg install --x-manifest-root=../scripts/vcpkg-manifest --x-install-root=./installed --triplet=x64-windows
```

#### Unix

```sh
export QT_DIR=<dir> # directory `Qt5Config.cmake` locates
export Qt5_DIR=$QT_DIR
export VCPKG_KEEP_ENV_VARS="QT_DIR;Qt5_DIR"

git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

./vcpkg install \
    --x-manifest-root=../scripts/vcpkg-manifest \
    --x-install-root=./installed \
    --triplet=<triplet>

# triplet:
#   Mac:   `x64-osx` or `arm64-osx`
#   Linux: `x64-linux` or `arm64-linux`
```