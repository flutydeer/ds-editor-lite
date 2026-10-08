#!/usr/bin/env python3
"""Tests for `convert-package.py`, the verifying wrapper around the reference converter.

The wrapper holds no rule of its own, so these tests check the two duties it does have. It must read
the values it needs from the reference converter rather than restating them, because its earlier
copies of them drifted as soon as the reference grew; and it must not pass a result on as converted
unless reading it back found no problem.

    python3 -m unittest discover -s scripts -p "test_*.py"
"""

import json
import subprocess
import sys
import unittest
from pathlib import Path

from test_convert_voicebank import VoicebankFixture

SCRIPT = Path(__file__).with_name("convert-package.py")
REFERENCE = Path(__file__).with_name("convert-voicebank.py")


class ConvertPackageTest(VoicebankFixture):
    def run_wrapper(self, *arguments):
        done = subprocess.run([sys.executable, str(SCRIPT), *map(str, arguments)],
                              capture_output=True, text=True, encoding="utf-8")
        return done.returncode, (done.stdout or "") + (done.stderr or "")

    def test_converts_and_verifies(self):
        output = self.destination("out")
        code, text = self.run_wrapper(self.package, "--output", output)
        self.assertEqual(code, 0, text)
        self.assertIn("contribution(s) verified", text)
        self.assertTrue((output / "desc.json").is_file())

    def test_refuses_an_existing_destination_without_touching_it(self):
        output = self.destination("out")
        output.mkdir(parents=True)
        (output / "keep.txt").write_text("user data", encoding="utf-8")
        code, text = self.run_wrapper(self.package, "--output", output)
        self.assertEqual(code, 1, text)
        self.assertIn("not passed on", text)
        self.assertEqual((output / "keep.txt").read_text(encoding="utf-8"), "user data")
        self.assertFalse((output / "desc.json").exists())

    def test_writes_a_patch_that_names_both_manifest_versions(self):
        output = self.destination("out")
        patch = self.work / "patch"
        code, text = self.run_wrapper(self.package, "--output", output, "--patch", patch)
        self.assertEqual(code, 0, text)
        manifest = json.loads((patch / "patch.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["from"]["id"], "fixture")
        self.assertIsNone(manifest["from"]["manifestVersion"])
        self.assertEqual(manifest["to"]["manifestVersion"], "1.0")
        self.assertTrue(Path(str(patch) + ".zip").is_file())

    def test_refuses_a_patch_over_an_earlier_one(self):
        output = self.destination("out")
        patch = self.work / "patch"
        patch.mkdir(parents=True)
        (patch / "earlier.txt").write_text("earlier patch", encoding="utf-8")
        code, text = self.run_wrapper(self.package, "--output", output, "--patch", patch)
        self.assertEqual(code, 1, text)
        self.assertIn("already contains files", text)

    def test_reports_a_category_the_reference_leaves_alone(self):
        # The reference converts the two voicebank categories, so a package that contributes another
        # category keeps that declaration as it was: the result is not a converted package.
        desc_path = self.package / "desc.json"
        desc = json.loads(desc_path.read_text(encoding="utf-8"))
        (self.package / "linguists").mkdir()
        (self.package / "linguists" / "ling.json").write_text("{}\n", encoding="utf-8")
        desc["contributes"]["linguists"] = ["linguists/ling.json"]
        desc_path.write_text(json.dumps(desc, ensure_ascii=False, indent=2) + "\n",
                             encoding="utf-8")
        code, text = self.run_wrapper(self.package, "--output", self.destination("out"))
        self.assertEqual(code, 1, text)
        self.assertIn("contributes no declaration at that path", text)

    def test_the_wrapper_restates_no_value_of_the_reference(self):
        source = SCRIPT.read_text(encoding="utf-8")
        for name in ("DEFAULT_OUTPUT_SUFFIX", "MANIFEST_VERSION"):
            self.assertNotIn(f"{name} = ", source)
            self.assertIn(f"authority.{name}", source)

    def test_the_reference_defines_the_values_the_wrapper_reads(self):
        source = REFERENCE.read_text(encoding="utf-8")
        for name in ("DEFAULT_OUTPUT_SUFFIX", "MANIFEST_VERSION"):
            self.assertIn(f"{name} = ", source)


if __name__ == "__main__":
    unittest.main()
