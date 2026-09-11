# vcpkg-overrides

Ports that take precedence over the shared overlay in `scripts/vcpkg/ports`. This branch builds lite against the **main line** of synthrt together with the two libraries layered on it: **wolf**, the linguist category, and **otter**, the analysis category.

| Port | Source |
| :-- | :-- |
| `synthrt` | `diffscope/synthrt`, branch `onnxruntime-builds-uptake`. Overrides the shared overlay's `synthrt` port, which pins the refactor line and performs config fixups for `srt-*` packages the main line does not produce. |
| `wolf` | `diffscope/wolf`, branch `linguistic-level-1` |
| `otter` | `diffscope/otter`, branch `analysis-level-1` |

Each port is pinned to one commit and fetched with `vcpkg_from_git`, which needs no archive hash. `diffscope/wolf` and `diffscope/otter` are private, so installing them needs git credentials for github.com with read access to both.

```sh
CMAKE_PREFIX_PATH=<Qt> VCPKG_KEEP_ENV_VARS=CMAKE_PREFIX_PATH \
vcpkg install --x-manifest-root=scripts/vcpkg-manifest --x-install-root=vcpkg/installed-main
```

The install root is `vcpkg/installed-main` rather than vcpkg's default `vcpkg/installed`, and the presets name the same path. The two lines of synthrt both install `lib/libsynthrt.so`, so they cannot share a tree, and giving this one its own name keeps a stale tree from being picked up silently.

`synthrt[cuda12]` compiles dsinfer's CUDA execution provider and brings the CUDA flavour of the ONNX Runtime payload. Without it Windows runs the DirectML provider and every other platform the CPU one.