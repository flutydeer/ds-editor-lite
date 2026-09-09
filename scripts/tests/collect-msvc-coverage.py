"""Collect native Windows coverage, merging duplicate source lines across test programs."""

import argparse
import csv
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET


def path_pattern(path):
    return re.escape(path.as_posix()).replace("/", r"[\\/]")


def summarize(report, repo, output, exit_code):
    files = {}
    for cls in ET.parse(report).iter("class"):
        path = Path(cls.attrib["filename"])
        relative = path.relative_to(repo)
        lines = files.setdefault(relative, {})
        for line in cls.findall("./lines/line"):
            number = int(line.attrib["number"])
            lines[number] = lines.get(number, False) or int(line.attrib["hits"]) > 0
    files = {path: lines for path, lines in files.items() if lines}
    if not files:
        raise RuntimeError("No production source coverage was collected; check PDBs and /PROFILE")
    with (output / "files.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["filename", "line_total", "line_covered", "line_percent"])
        for name, lines in sorted(files.items()):
            covered = sum(lines.values())
            writer.writerow([name.as_posix(), len(lines), covered,
                             f"{100 * covered / len(lines):.2f}"])
    covered = sum(sum(lines.values()) for lines in files.values())
    total = sum(len(lines) for lines in files.values())
    summary = (f"CTest/collector exit code: {exit_code}\n"
               f"Production source lines: {covered}/{total} ({100 * covered / total:.2f}%)\n"
               "See junit.xml for failures and skipped tests.\n"
               "Duplicate file/line entries are combined across modules.\n"
               "The native collector does not provide GCC-compatible branch coverage.\n")
    if exit_code:
        summary += "The test run failed; this coverage is diagnostic only.\n"
    (output / "summary.txt").write_text(summary, encoding="utf-8")
    print(summary, end="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--collector", required=True,
                        help="Path to Visual Studio's Microsoft.CodeCoverage.Console.exe")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--build-dir", type=Path, default=Path("build/Coverage"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("ctest_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("This collector requires Windows; use GCC/gcovr on Linux")
    repo = Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)

    config = ET.Element("Configuration")
    coverage = ET.SubElement(config, "CodeCoverage")
    modules = ET.SubElement(coverage, "ModulePaths")
    include = ET.SubElement(modules, "Include")
    discovered = subprocess.check_output(
        [args.ctest, "--test-dir", str(build), "--show-only=json-v1"],
        cwd=repo, encoding="utf-8")
    executables = set()
    for test in json.loads(discovered)["tests"]:
        for argument in test.get("command", []):
            executable = Path(argument)
            if executable.suffix.lower() != ".exe" or not executable.is_file():
                continue
            executable = executable.resolve()
            try:
                executable.relative_to(build)
            except ValueError:
                continue
            executables.add(executable)
    if not executables:
        raise RuntimeError("No built test executables were found in the selected CTest configuration")
    # CTest commands also identify the Editor/Connector children. Obsolete build outputs are excluded.
    for executable in sorted(executables):
        ET.SubElement(include, "ModulePath").text = "^" + path_pattern(executable) + "$"
    directories = ET.SubElement(modules, "IncludeDirectories")
    ET.SubElement(directories, "Directory", Recursive="false").text = str(build / "out/bin")
    sources = ET.SubElement(ET.SubElement(coverage, "Sources"), "Include")
    ET.SubElement(sources, "Source").text = (
        path_pattern(repo / "src") + r"[\\/](app|connector|libs|tools)[\\/].*")
    for key, value in {
        "EnableStaticNativeInstrumentation": "True",
        "EnableDynamicNativeInstrumentation": "False",
        "EnableStaticNativeInstrumentationRestore": "True",
        "CollectFromChildProcesses": "True",
    }.items():
        ET.SubElement(coverage, key).text = value
    settings = output / "coverage.config"
    ET.ElementTree(config).write(settings, encoding="utf-8", xml_declaration=True)
    extra = args.ctest_args[1:] if args.ctest_args[:1] == ["--"] else args.ctest_args
    command = [args.collector, "collect", "--settings", str(settings), "--output",
               str(output / "result.coverage"), "--log-file", str(output / "collector.log"),
               "--log-level", "Info",
               args.ctest, "--test-dir", str(build), "--parallel", "2", "--output-on-failure",
               "--no-tests=error",
               "--output-junit", str(output / "junit.xml"), *extra]
    started_ns = time.time_ns()
    with (output / "tests.log").open("wb") as log:
        result = subprocess.run(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT)
    last_test = build / "Testing/Temporary/LastTest.log"
    if ((output / "junit.xml").is_file() and last_test.is_file()
            and last_test.stat().st_mtime_ns >= started_ns):
        shutil.copyfile(last_test, output / "LastTest.log")
    subprocess.run([args.collector, "merge", str(output / "result.coverage"), "--output",
                    str(output / "coverage.xml"), "--output-format", "cobertura"], check=True)
    summarize(output / "coverage.xml", repo, output, result.returncode)
    print(f"CTest/collector exit code: {result.returncode}; diagnostics: {output}")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
