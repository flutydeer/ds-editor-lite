"""Preserve macOS crash reports from application and test processes."""

import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", type=Path, default=Path("build/test-results"))
    args = parser.parse_args()
    results = args.results.resolve()
    output = results / "crashes"
    output.mkdir(parents=True, exist_ok=True)
    reports = Path.home() / "Library/Logs/DiagnosticReports"
    for report in reports.glob("*"):
        if (report.suffix in (".ips", ".crash")
                and report.name.startswith(("Test", "DsEditorLite", "DsConnectorLite"))):
            shutil.copy2(report, output / report.name)
            print(f"Preserved crash report: {report.name}")


if __name__ == "__main__":
    main()
