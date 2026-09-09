"""Run CTest with Clang instrumentation and report production source coverage."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from coverage_support import production_path, test_executables, write_line_summary


def find_tool(name):
    if sys.platform == "darwin":
        return subprocess.check_output(["xcrun", "--find", name], encoding="utf-8").strip()
    result = shutil.which(name)
    if not result:
        raise RuntimeError(f"{name} was not found; use the LLVM tools matching the compiler")
    return result


def read_lcov(report, repo):
    files = {}
    current = None
    with report.open(encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("SF:"):
                current = production_path(line[3:].rstrip("\n"), repo)
                if current is not None:
                    files.setdefault(current, {})
            elif current is not None and line.startswith("DA:"):
                number, hits, *_ = line[3:].strip().split(",")
                number = int(number)
                lines = files[current]
                lines[number] = lines.get(number, False) or int(hits) > 0
            elif line.startswith("end_of_record"):
                current = None
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-cov")
    parser.add_argument("--llvm-profdata")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--build-dir", type=Path, default=Path("build/Coverage"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("ctest_args", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    build = args.build_dir.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    profile_dir = output / "profiles"
    profile_dir.mkdir()
    llvm_cov = args.llvm_cov or find_tool("llvm-cov")
    llvm_profdata = args.llvm_profdata or find_tool("llvm-profdata")
    executables = test_executables(args.ctest, build, repo)
    environment = os.environ.copy()
    environment["LLVM_PROFILE_FILE"] = str(profile_dir / "%p-%m.profraw")
    extra = args.ctest_args[1:] if args.ctest_args[:1] == ["--"] else args.ctest_args
    command = [args.ctest, "--test-dir", str(build), "--parallel", "2", "--output-on-failure",
               "--no-tests=error", "--output-junit", str(output / "junit.xml"), *extra]
    started_ns = time.time_ns()
    with (output / "tests.log").open("wb") as log:
        result = subprocess.run(command, cwd=repo, env=environment,
                                stdout=log, stderr=subprocess.STDOUT)
    last_test = build / "Testing/Temporary/LastTest.log"
    if last_test.is_file() and last_test.stat().st_mtime_ns >= started_ns:
        shutil.copyfile(last_test, output / "LastTest.log")
    profiles = sorted(profile_dir.glob("*.profraw"))
    if not profiles:
        raise RuntimeError("No LLVM profiles were written; check Clang instrumentation")
    profile = output / "coverage.profdata"
    subprocess.run([llvm_profdata, "merge", "-sparse", *map(str, profiles),
                    "-o", str(profile)], check=True)
    objects = [str(executables[0]), "-instr-profile=" + str(profile)]
    objects.extend("-object=" + str(path) for path in executables[1:])
    report = output / "coverage.lcov"
    with report.open("w", encoding="utf-8") as stream:
        subprocess.run([llvm_cov, "export", "-format=lcov", *objects],
                       stdout=stream, check=True)
    files = read_lcov(report, repo)
    sources = [str(repo / path) for path in sorted(files)]
    with (output / "llvm-summary.txt").open("w", encoding="utf-8") as stream:
        subprocess.run([llvm_cov, "report", *objects, *sources], stdout=stream, check=True)
    subprocess.run([llvm_cov, "show", *objects, *sources, "-format=html",
                    "-output-dir=" + str(output / "html")], check=True)
    write_line_summary(files, output, result.returncode,
                       "LLVM branch coverage is recorded in llvm-summary.txt and coverage.lcov.")
    print(f"CTest exit code: {result.returncode}; diagnostics: {output}")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
