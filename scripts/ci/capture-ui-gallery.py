#!/usr/bin/env python3
"""Capture both UI themes on a dedicated CI desktop after the test suite passes."""

import argparse
from contextlib import contextmanager, ExitStack
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import time


def run(command, log, env, cwd, timeout=300):
    print(f"Running {' '.join(map(str, command))}", flush=True)
    with log.open("w", encoding="utf-8") as stream:
        try:
            subprocess.run(command, cwd=cwd, env=env, stdout=stream,
                           stderr=subprocess.STDOUT, timeout=timeout, check=True)
        except (subprocess.SubprocessError, OSError):
            print(log.read_text(encoding="utf-8", errors="replace"), flush=True)
            raise


@contextmanager
def linux_desktop(theme, diagnostics, env):
    """Use native Openbox frames inside the workflow's disposable Xvfb server."""
    with tempfile.TemporaryDirectory(prefix="ui-desktop-") as temporary, ExitStack() as stack:
        config = Path(temporary) / "openbox.xml"
        decoration = "Onyx" if theme == "dark" else "Clearlooks"
        config.write_text(
            '<?xml version="1.0"?><openbox_config xmlns="http://openbox.org/3.4/rc">'
            f'<theme><name>{decoration}</name></theme>'
            '<focus><focusNew>yes</focusNew></focus>'
            '</openbox_config>', encoding="utf-8")
        processes = []
        try:
            for name, command in (
                ("openbox", ["openbox", "--config-file", str(config)]),
            ):
                log = stack.enter_context((diagnostics / f"{name}.log").open("w"))
                processes.append(subprocess.Popen(command, env=env, stdout=log,
                                                  stderr=subprocess.STDOUT))
                time.sleep(1)
                if processes[-1].poll() is not None:
                    raise RuntimeError(f"{name} could not start; see {diagnostics}")
            yield
        finally:
            for process in reversed(processes):
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def runtime_environment(build, qt):
    env = os.environ.copy()
    env.pop("QT_QPA_PLATFORM_PLUGIN_PATH", None)
    env.pop("QT_STYLE_OVERRIDE", None)
    env["QT_PLUGIN_PATH"] = str(qt / "plugins")
    env["QT_FORCE_STDERR_LOGGING"] = "1"
    env["LITE_SCREENSHOT_DESKTOP_CAPTURE"] = "1"
    system = platform.system()
    env["QT_QPA_PLATFORM"] = {"Windows": "windows", "Darwin": "cocoa", "Linux": "xcb"}[system]
    if system == "Darwin":
        env["DYLD_FRAMEWORK_PATH"] = str(qt / "lib")
    elif system == "Windows":
        cache = {}
        for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
            if "=" in line and not line.startswith(("//", "#")):
                key, value = line.split("=", 1)
                cache[key.split(":", 1)[0]] = value
        installed = Path(cache["VCPKG_INSTALLED_DIR"]) / cache["VCPKG_TARGET_TRIPLET"]
        env["PATH"] = os.pathsep.join(map(str, [qt / "bin", installed / "debug/bin",
                                               installed / "bin"])) + os.pathsep + env["PATH"]
    return env


def capture(build, output, qt):
    system = platform.system()
    platform_id = {"Windows": "windows", "Darwin": "macos", "Linux": "linux"}[system]
    root = output / platform_id
    root.mkdir(parents=True, exist_ok=True)
    binaries = build / "out/bin"
    app = binaries / ("DsEditorLite.exe" if system == "Windows" else "DsEditorLite")
    if system == "Darwin":
        app = binaries / "DsEditorLite.app/Contents/MacOS/DsEditorLite"
    gallery = binaries / ("TestApplicationGui.exe" if system == "Windows" else "TestApplicationGui")
    for binary in (app, gallery):
        if not binary.is_file():
            raise FileNotFoundError(binary)
    base_env = runtime_environment(build, qt)
    report = {"platform": platform_id, "system": platform.platform(), "themes": [],
              "complete": False}
    if system == "Linux":
        report["desktop"] = "Xvfb with Openbox; no compositor or system shadows"
    manifest = root / "capture.json"
    try:
        for theme in ("light", "dark"):
            directory = root / theme
            diagnostics = directory / "diagnostics"
            diagnostics.mkdir(parents=True, exist_ok=True)
            env = base_env | {"LITE_CAPTURE_THEME": theme, "DS_EDITOR_THEME": f"lite-{theme}"}
            with ExitStack() as stack:
                if system == "Linux":
                    stack.enter_context(linux_desktop(theme, diagnostics, env))
                for mode in ("1", "0"):
                    capture_env = env | {"LITE_CHROME_CAPTURE": str(directory),
                                         "LITE_FORCE_NATIVE_FRAME": mode}
                    run([str(app)], diagnostics / f"main-{mode}.log", capture_env, binaries, 180)
                run([str(gallery), "captureUiGallery", "-v1"], diagnostics / "gallery.log",
                    env | {"LITE_UI_GALLERY": str(directory)}, binaries, 360)
            run([sys.executable, str(Path(__file__).with_name("verify-ui-gallery.py")),
                 str(directory), "--theme", theme], diagnostics / "verify.log", env, binaries)
            print((diagnostics / "verify.log").read_text(encoding="utf-8"), flush=True)
            report["themes"].append(theme)
        report["complete"] = True
    finally:
        manifest.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--qt-dir", type=Path, default=os.environ.get("QT_ROOT_DIR"))
    args = parser.parse_args()
    if not args.qt_dir:
        parser.error("--qt-dir or QT_ROOT_DIR is required")
    capture(args.build_dir.resolve(), args.output.resolve(), args.qt_dir.resolve())
