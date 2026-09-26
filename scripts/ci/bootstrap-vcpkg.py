"""Prepare the pinned vcpkg checkout and install the selected platform's dependencies."""

import argparse
import os
from pathlib import Path
import subprocess


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--triplet", required=True,
                        choices=("x64-linux", "x64-windows", "arm64-osx"))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    qt = Path(os.environ.get("QT_ROOT_DIR") or os.environ.get("QT_DIR") or "")
    if not (qt / "lib/cmake/Qt6").is_dir():
        parser.error("Set QT_ROOT_DIR to a Qt installation with the project modules")
    env = os.environ.copy()
    env.update(QT_DIR=str(qt), Qt6_DIR=str(qt / "lib/cmake/Qt6"),
               Qt6GuiTools_DIR=str(qt / "lib/cmake/Qt6GuiTools"), CMAKE_PREFIX_PATH=str(qt),
               VCPKG_KEEP_ENV_VARS="QT_DIR;Qt6_DIR;Qt6GuiTools_DIR;CMAKE_PREFIX_PATH")
    checkout = repo / "vcpkg"
    commit = "abb6dda5cc32914d2e64d7d72b974dc301d1fc8a"
    if not (checkout / ".git").exists():
        run("git", "init", checkout)
        run("git", "-C", checkout, "remote", "add", "origin", "https://github.com/microsoft/vcpkg.git")
        run("git", "-C", checkout, "fetch", "--depth", "1", "origin", commit)
        run("git", "-C", checkout, "checkout", "--detach", "FETCH_HEAD")
    else:
        actual = subprocess.check_output(["git", "-C", str(checkout), "rev-parse", "HEAD"],
                                         text=True).strip()
        if actual != commit:
            parser.error(f"Existing vcpkg checkout must be at {commit}; it has not been modified")
    if os.name == "nt":
        run("cmd.exe", "/d", "/c", checkout / "bootstrap-vcpkg.bat", "-disableMetrics", env=env)
        executable = checkout / "vcpkg.exe"
    else:
        run(checkout / "bootstrap-vcpkg.sh", "-disableMetrics", env=env)
        executable = checkout / "vcpkg"
    run(executable, "install", f"--x-manifest-root={repo / 'scripts/vcpkg-manifest'}",
        f"--x-install-root={checkout / 'installed'}", f"--triplet={args.triplet}",
        "--clean-buildtrees-after-build", env=env)


if __name__ == "__main__":
    main()
