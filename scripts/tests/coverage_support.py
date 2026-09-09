"""Shared source filtering and line summaries for native coverage collectors."""

import csv
import json
from pathlib import Path
import subprocess


def production_path(filename, repo):
    try:
        relative = Path(filename).resolve().relative_to(repo)
    except ValueError:
        return None
    if len(relative.parts) < 3 or relative.parts[0] != "src":
        return None
    return relative if relative.parts[1] in ("app", "connector", "libs", "tools") else None


def test_executables(ctest, build, repo):
    discovered = subprocess.check_output(
        [ctest, "--test-dir", str(build), "--show-only=json-v1"],
        cwd=repo, encoding="utf-8")
    executables = set()
    for test in json.loads(discovered)["tests"]:
        for argument in test.get("command", []):
            path = Path(argument)
            if not path.is_file():
                continue
            path = path.resolve()
            try:
                path.relative_to(build)
            except ValueError:
                continue
            with path.open("rb") as stream:
                magic = stream.read(4)
            if (magic.startswith(b"MZ") or magic == b"\x7fELF"
                    or magic in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf",
                                 b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
                                 b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca")):
                executables.add(path)
    if not executables:
        raise RuntimeError("No built test executables were found in the CTest configuration")
    return sorted(executables)


def write_line_summary(files, output, exit_code, branch_note):
    files = {path: lines for path, lines in files.items() if lines}
    if not files:
        raise RuntimeError("No production source coverage was collected; check instrumentation")
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
               f"{branch_note}\n")
    if exit_code:
        summary += "The test run failed; this coverage is diagnostic only.\n"
    (output / "summary.txt").write_text(summary, encoding="utf-8")
    print(summary, end="")
