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
    assert report["size"][0] <= 5120, "Gallery exceeds its maximum row width"
    if theme:
        assert report["theme"] == theme, "Wrong gallery theme"
        assert report["themeId"] == f"lite-{theme}", "Wrong application theme"
        for mode in ("native-frame", "frameless"):
            geometry = json.loads((directory / "modules/01-workspace" /
                                   f"title-bar-{mode}.json").read_text(encoding="utf-8"))
            assert geometry["themeId"] == f"lite-{theme}", "Wrong main-window theme"
            if geometry["captureMethod"] == "x11-window-without-compositor":
                left, top, right, bottom = geometry["frameMargins"]
                _, _, width, height = geometry["widgetRect"]
                scale = geometry["devicePixelRatio"]
                expected_size = [round((width + left + right) * scale),
                                 round((height + top + bottom) * scale)]
                assert png_size(directory / "modules/01-workspace" /
                                f"title-bar-{mode}.png") == expected_size, "X11 capture omits the recorded native frame"
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
    content_right = 0
    for section in report["sections"]:
        module_directory = Path("modules") / section["id"]
        assert Path(section["file"]) == module_directory / "review.png"
        heading = section["headingRect"]
        assert contains(canvas, heading)
        assert heading[1] > previous_bottom, section["title"]
        assert heading[0] == 64 and heading[2] == report["size"][0] - 128
        assert png_size(directory / section["file"])[0] == report["size"][0]
        occupied = []
        rows = collections.defaultdict(list)
        for item in section["images"]:
            assert item["caption"].strip()
            assert "empty" not in item["caption"].lower(), "Empty panels should use their name only"
            assert Path(item["file"]).parent == module_directory, "Screenshot is not classified"
            assert png_size(directory / item["file"]) == item["sourceSize"]
            image, caption = item["imageRect"], item["captionRect"]
            rows[item["row"]].append(image)
            content_right = max(content_right, image[0] + image[2], caption[0] + caption[2])
            assert contains(canvas, image) and contains(canvas, caption)
            assert image[1] >= heading[1] + heading[3], item["caption"]
            assert caption[1] == image[1] + image[3] + 12, "Caption is detached from its image"
            bounds = [caption[0], image[1], caption[2], caption[1] + caption[3] - image[1]]
            assert not any(intersects(bounds, other) for other in occupied), item["caption"]
            occupied.append(bounds)
            previous_bottom = max(previous_bottom, caption[1] + caption[3])
            methods[item["captureMethod"]] += 1
        assert section["images"]
        assert set(rows) == set(range(len(rows))), "Row indices are not contiguous"
        for images in rows.values():
            row_top = min(image[1] for image in images)
            row_height = max(image[3] for image in images)
            for image in images:
                assert image[1] == row_top + (row_height - image[3]) // 2, "Image is not vertically centered"
    assert report["size"][0] == content_right + 64, "Canvas includes unused right padding"
    allowed = {"screencapturekit-window", "windows-desktop-window", "windows-printwindow-frame-qt-client",
               "x11-window-without-compositor",
               "appkit-frame-fallback", "qt-client-fallback",
               "qt-widget-content"}
    assert methods.keys() <= allowed, methods
    assert methods["qt-widget-content"] > 0, "No standalone custom panel captures"
    assert sum(methods.values()) == 50, "Screenshot coverage is incomplete"
    captions = {item["caption"] for section in report["sections"] for item in section["images"]}
    assert {"Parameter curves", "Task list", "Fill lyrics - Help"} <= captions
    print(f"Verified {sum(methods.values())} screenshots in {len(expected_modules)} UI modules")
    for method, count in sorted(methods.items()):
        print(f"  {method}: {count}")
    if methods["x11-window-without-compositor"]:
        print("X11 captures include native Openbox frames; this desktop has no compositor shadows")
    if methods["windows-printwindow-frame-qt-client"]:
        print("PrintWindow frames use complete Qt client pixels; DWM compositor shadows are not guaranteed")
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
