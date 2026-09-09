"""Preserve macOS crash reports and reproduce a failed RHI test under LLDB."""

import argparse
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build/Coverage"))
    parser.add_argument("--results", type=Path, default=Path("build/test-results"))
    args = parser.parse_args()
    build = args.build_dir.resolve()
    results = args.results.resolve()
    output = results / "crashes"
    output.mkdir(parents=True, exist_ok=True)
    reports = Path.home() / "Library/Logs/DiagnosticReports"
    for report in reports.glob("*"):
        if (report.suffix in (".ips", ".crash")
                and report.name.startswith(("Test", "DsEditorLite", "DsConnectorLite"))):
            shutil.copy2(report, output / report.name)
            print(f"Preserved crash report: {report.name}")

    junit = results / "coverage/junit.xml"
    if not junit.is_file():
        return
    failed = any(test.get("name") == "TestNativeDesktop"
                 and (test.find("failure") is not None or test.find("error") is not None)
                 for test in ET.parse(junit).iter("testcase"))
    if not failed:
        return

    discovered = json.loads(subprocess.check_output(
        ["ctest", "--test-dir", str(build), "--show-only=json-v1"], encoding="utf-8"))
    test = next(test for test in discovered["tests"] if test["name"] == "TestNativeDesktop")
    properties = {item["name"]: item["value"] for item in test["properties"]}
    environment = os.environ.copy()
    environment.update(value.split("=", 1) for value in properties.get("ENVIRONMENT", []))
    # Diagnostic executions must not contribute to the original coverage measurement.
    environment["LLVM_PROFILE_FILE"] = str(output / "diagnostic-%p-%m.profraw")
    command = ["xcrun", "lldb", "--batch", "--no-lldbinit", "-o", "run",
               "-k", "thread backtrace all", "-k", "process kill", "--", *test["command"],
               "rhiNoteDrawingCommitsAndUndoUpdatesInteraction", "-nocrashhandler"]
    print("Reproducing failed RHI test for diagnostics only; original JUnit is unchanged.")
    with (output / "rhi-lldb.log").open("wb") as log:
        with subprocess.Popen(command, cwd=properties["WORKING_DIRECTORY"], env=environment,
                              stdout=log, stderr=subprocess.STDOUT,
                              start_new_session=True) as process:
            try:
                exit_code = process.wait(timeout=90)
                print(f"Diagnostic LLDB exit code: {exit_code}")
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                print("Diagnostic LLDB reached its 90-second limit; partial output was retained.")


if __name__ == "__main__":
    main()
