"""Capture a debugger stack for a failed Linux application GUI suite."""

import argparse
import json
import os
from pathlib import Path
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
    junit = results / "junit.xml"
    if not junit.is_file():
        return
    failed = any(test.get("name") == "TestApplicationGui"
                 and (test.find("failure") is not None or test.find("error") is not None)
                 for test in ET.parse(junit).iter("testcase"))
    if not failed:
        return

    discovered = json.loads(subprocess.check_output(
        ["ctest", "--test-dir", str(build), "--show-only=json-v1"], encoding="utf-8"))
    test = next(test for test in discovered["tests"] if test["name"] == "TestApplicationGui")
    properties = {item["name"]: item["value"] for item in test["properties"]}
    output = results / "crashes"
    output.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment.update(value.split("=", 1) for value in properties.get("ENVIRONMENT", []))
    # Diagnostic executions must not contribute to the original coverage measurement.
    environment["GCOV_PREFIX"] = str(output / "gcov")
    environment["GCOV_PREFIX_STRIP"] = "0"
    command = ["gdb", "--batch", "--quiet", "-ex", "set confirm off", "-ex", "run",
               "-ex", "thread apply all bt", "-ex", "kill", "--args", *test["command"],
               "-nocrashhandler"]
    print("Reproducing the failed GUI suite for diagnostics; original JUnit is unchanged.")
    with (output / "application-gui-gdb.log").open("wb") as log:
        with subprocess.Popen(command, cwd=properties["WORKING_DIRECTORY"], env=environment,
                              stdout=log, stderr=subprocess.STDOUT,
                              start_new_session=True) as process:
            try:
                exit_code = process.wait(timeout=90)
                print(f"Diagnostic GDB exit code: {exit_code}")
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                print("Diagnostic GDB reached its 90-second limit; partial output was retained.")


if __name__ == "__main__":
    main()
