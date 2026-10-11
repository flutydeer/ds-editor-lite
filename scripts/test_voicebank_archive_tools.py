"""Regression checks for the voicebank packaging and release-audit tools."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile


TOOLS = Path(__file__).resolve().parents[1] / "docs/synthrt/tools"


class VoicebankArchiveToolsTest(unittest.TestCase):
    def run_tool(self, name, *arguments):
        return subprocess.run([sys.executable, str(TOOLS / name), *map(str, arguments)],
                              capture_output=True, text=True, encoding="utf-8")

    def test_pack_roundtrip(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "package"
            root.mkdir()
            (root / "desc.json").write_text('{"id":"fixture"}', encoding="utf-8")
            (root / "model.bin").write_bytes(b"fixture payload")
            archive = Path(temporary) / "voice.zip"
            result = self.run_tool("pack-voicebank-zip.py", root, archive)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with zipfile.ZipFile(archive) as zipped:
                self.assertEqual(zipped.read("model.bin"), b"fixture payload")

    def test_pack_rejects_external_file_links_before_creating_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "package"
            root.mkdir()
            (root / "desc.json").write_text('{"id":"fixture"}', encoding="utf-8")
            outside = Path(temporary) / "private.txt"
            outside.write_text("must not be packaged", encoding="utf-8")
            try:
                (root / "leak.txt").symlink_to(outside)
            except OSError as error:
                self.skipTest(f"Creating symbolic links is unavailable: {error}")
            archive = Path(temporary) / "voice.zip"
            result = self.run_tool("pack-voicebank-zip.py", root, archive)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("symbolic link", result.stdout)
            self.assertFalse(archive.exists())

    def test_release_comparison_reports_old_to_new(self):
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            releases = []
            for version, member in (("1.0.0.0", "removed.txt"), ("2.0.0.0", "added.txt")):
                tag = f"lang-v{version}"
                folder = cache / tag
                folder.mkdir()
                archive = folder / f"wolf-fixture-{version}.zip"
                with zipfile.ZipFile(archive, "w") as zipped:
                    zipped.writestr("desc.json", json.dumps({"id": "fixture", "version": version,
                                                             "compatVersion": version}))
                    zipped.writestr(member, "test")
                releases.append({"tag_name": tag, "assets": [{"name": archive.name,
                                  "size": archive.stat().st_size}, {"name": "unrelated.zip"}]})
                # Previous runs can leave stale or partially extracted trees. They must not
                # affect the comparison of the archives verified during this run.
                for side in ("x", "y"):
                    stale = folder / f"{side}_{archive.stem}"
                    stale.mkdir()
                    (stale / "stale.txt").write_text("old extraction", encoding="utf-8")
            metadata = cache / "releases.json"
            metadata.write_text(json.dumps(releases), encoding="utf-8")
            result = self.run_tool("compare-wolf-releases.py", "--releases", metadata,
                                   "--cache", cache, "--tags", "lang-v2.0.0.0", "lang-v1.0.0.0")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("added: added.txt", result.stdout)
            self.assertIn("removed: removed.txt", result.stdout)
            self.assertIn("compatVersion 1.0.0.0 -> 2.0.0.0", result.stdout)
            self.assertNotIn("stale.txt", result.stdout)


if __name__ == "__main__":
    unittest.main()
