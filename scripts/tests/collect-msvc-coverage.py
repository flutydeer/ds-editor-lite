"""Collect native Windows coverage, merging duplicate source lines across test programs."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

from coverage_support import production_path, test_executables, write_line_summary


def path_pattern(path):
    return re.escape(path.as_posix()).replace("/", r"[\\/]")


def summarize(report, repo, output, exit_code):
    files = {}
    for cls in ET.parse(report).iter("class"):
        relative = production_path(cls.attrib["filename"], repo)
        if relative is None:
            continue
        lines = files.setdefault(relative, {})
        for line in cls.findall("./lines/line"):
            number = int(line.attrib["number"])
            lines[number] = lines.get(number, False) or int(line.attrib["hits"]) > 0
    write_line_summary(files, output, exit_code,
                       "The native collector does not provide GCC-compatible branch coverage.")


def find_collector():
    executable = "Microsoft.CodeCoverage.Console.exe"
    found = shutil.which(executable)
    if found:
        return found
    vswhere = (Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio"
               / "Installer/vswhere.exe")
    found = subprocess.check_output(
        [str(vswhere), "-products", "*", "-latest", "-find",
         "Common7/IDE/Extensions/Microsoft/CodeCoverage.Console/" + executable],
        encoding="utf-8").strip()
    if not found:
        raise RuntimeError("Visual Studio's native coverage collector was not found; use --collector")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--collector",
                        help="Path to Microsoft's native collector; defaults to the installed Visual Studio")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--build-dir", type=Path, default=Path("build/Coverage"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("ctest_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("This collector requires Windows; use the compiler's coverage tools elsewhere")
    repo = Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    collector = args.collector or find_collector()

    config = ET.Element("Configuration")
    coverage = ET.SubElement(config, "CodeCoverage")
    modules = ET.SubElement(coverage, "ModulePaths")
    include = ET.SubElement(modules, "Include")
    executables = test_executables(args.ctest, build, repo)
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
    command = [collector, "collect", "--settings", str(settings), "--output",
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
    subprocess.run([collector, "merge", str(output / "result.coverage"), "--output",
                    str(output / "coverage.xml"), "--output-format", "cobertura"], check=True)
    summarize(output / "coverage.xml", repo, output, result.returncode)
    print(f"CTest/collector exit code: {result.returncode}; diagnostics: {output}")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
