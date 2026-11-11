#!/usr/bin/env python3
"""Check the screenshot gallery's real files, layout and capture provenance."""

import collections
import argparse
import json
from pathlib import Path
import struct


def png_size(path):
    with path.open("rb") as image:
        header = image.read(24)
    assert header[:8] == b"\x89PNG\r\n\x1a\n", f"Not a PNG: {path}"
    return list(struct.unpack(">II", header[16:24]))


def contains(outer, inner):
    x, y, width, height = inner
    ox, oy, ow, oh = outer
    return x >= ox and y >= oy and x + width <= ox + ow and y + height <= oy + oh


def intersects(a, b):
    return (a[0] < b[0] + b[2] and b[0] < a[0] + a[2]
            and a[1] < b[1] + b[3] and b[1] < a[1] + a[3])


def verify(directory, theme=None):
    report = json.loads((directory / "ui-review.json").read_text(encoding="utf-8"))
    assert report["complete"], "Gallery capture did not finish successfully"
    assert report["layout"] == "flow", "Wrong gallery layout"
    assert report["size"][0] > report["size"][1], "The complete gallery must be landscape"
    if theme:
        assert report["theme"] == theme, "Wrong gallery theme"
        assert report["themeId"] == f"lite-{theme}", "Wrong application theme"
        for mode in ("native-frame", "frameless"):
            geometry = json.loads((directory / "modules/01-workspace" /
                                   f"title-bar-{mode}.json").read_text(encoding="utf-8"))
            assert geometry["themeId"] == f"lite-{theme}", "Wrong main-window theme"
    assert png_size(directory / "ui-review.png") == report["size"]
    canvas = [0, 0, *report["size"]]
    expected_modules = ["Project & workspace", "Tracks & timeline", "Notes & pitch editing",
                        "Lyrics & pronunciation", "Voice & mixing", "Audio & project I/O",
                        "Packages & resources", "Preferences", "Tasks & diagnostics"]
    assert [section["title"] for section in report["sections"]] == expected_modules
    module_ids = ["01-workspace", "02-tracks", "03-notes", "04-lyrics", "05-mixing",
                  "06-audio", "07-packages", "08-settings", "09-diagnostics"]
    assert [section["id"] for section in report["sections"]] == module_ids
    methods = collections.Counter()
    previous_bottom = -1
    for section in report["sections"]:
        module_directory = Path("modules") / section["id"]
        assert Path(section["file"]) == module_directory / "review.png"
        heading = section["headingRect"]
        assert contains(canvas, heading)
        assert heading[1] > previous_bottom, section["title"]
        assert heading[0] == 64 and heading[2] == report["size"][0] - 128
        assert png_size(directory / section["file"])[0] == report["size"][0]
        occupied = []
        for item in section["images"]:
            assert item["caption"].strip()
            assert Path(item["file"]).parent == module_directory, "Screenshot is not classified"
            assert png_size(directory / item["file"]) == item["sourceSize"]
            image, caption = item["imageRect"], item["captionRect"]
            assert contains(canvas, image) and contains(canvas, caption)
            assert image[1] >= heading[1] + heading[3], item["caption"]
            assert caption[1] >= image[1] + image[3], item["caption"]
            bounds = [caption[0], image[1], caption[2], caption[1] + caption[3] - image[1]]
            assert not any(intersects(bounds, other) for other in occupied), item["caption"]
            occupied.append(bounds)
            previous_bottom = max(previous_bottom, caption[1] + caption[3])
            methods[item["captureMethod"]] += 1
        assert section["images"]
    allowed = {"screencapturekit-window", "windows-desktop-window", "windows-printwindow-frame",
               "x11-window-without-compositor",
               "appkit-frame-fallback", "qt-client-fallback",
               "qt-widget-content"}
    assert methods.keys() <= allowed, methods
    assert methods["qt-widget-content"] > 0, "No standalone custom panel captures"
    assert sum(methods.values()) == 49, "Screenshot coverage is incomplete"
    print(f"Verified {sum(methods.values())} screenshots in {len(expected_modules)} UI modules")
    for method, count in sorted(methods.items()):
        print(f"  {method}: {count}")
    if methods["x11-window-without-compositor"]:
        print("X11 captures include native Openbox frames; this desktop has no compositor shadows")
    if methods["windows-printwindow-frame"]:
        print("PrintWindow captures include Windows frames, without DWM compositor shadow guarantees")
    fallback_count = methods["appkit-frame-fallback"] + methods["qt-client-fallback"]
    if fallback_count:
        print(f"::warning::{fallback_count} windows used a fallback without system shadow/corner guarantees")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--theme", choices=("light", "dark"))
    args = parser.parse_args()
    verify(args.directory, args.theme)
