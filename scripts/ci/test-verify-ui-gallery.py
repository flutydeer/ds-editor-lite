"""The artifact verifier accepts arbitrary gallery inventories, not a snapshot list."""

import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "gallery_verifier", Path(__file__).with_name("verify-ui-gallery.py"))
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class GalleryVerificationTests(unittest.TestCase):
    def make_gallery(self, root, groups):
        def png(path, width, height):
            path.parent.mkdir(parents=True, exist_ok=True)
            # The verifier reads the PNG signature and IHDR dimensions only.
            path.write_bytes(b"\x89PNG\r\n\x1a\n" + b"\0\0\0\rIHDR"
                             + struct.pack(">II", width, height))

        sections = []
        top = 64
        for module, captions in groups:
            images = []
            y = top + 76
            for row, caption in enumerate(captions):
                path = f"modules/{module}/sample-{row}.png"
                png(root / path, 320, 40)
                images.append({"caption": caption, "file": path,
                               "sourceSize": [320, 40], "captureMethod": "test-capture",
                               "imageRect": [64, y, 320, 40],
                               "captionRect": [64, y + 52, 320, 20], "row": row})
                y += 116
            path = f"modules/{module}/review.png"
            png(root / path, 448, y - top)
            sections.append({"id": module, "title": f"Title {module}", "file": path,
                             "headingRect": [64, top, 320, 52], "images": images})
            top = y + 64
        report = {"complete": True, "layout": "flow", "size": [448, top],
                  "theme": "light", "themeId": "lite-light", "sections": sections}
        png(root / "ui-review.png", *report["size"])
        (root / "ui-review.json").write_text(json.dumps(report), encoding="utf-8")

    def test_inventory_is_not_a_contract(self):
        variants = [
            [("new-module", ["Renamed panel"])],
            [("z-last", ["Empty is a valid descriptive word", "Added panel"]),
             ("a-first", ["Different panel"])],
            [("a-first", ["Different panel"]), ("z-last", ["Added panel"])],
        ]
        for groups in variants:
            with self.subTest(groups=groups), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                self.make_gallery(root, groups)
                verifier.verify(root, "light")

    def test_missing_file_is_still_an_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_gallery(root, [("example", ["Panel"])])
            (root / "modules/example/sample-0.png").unlink()
            with self.assertRaises(FileNotFoundError):
                verifier.verify(root)


if __name__ == "__main__":
    unittest.main()
