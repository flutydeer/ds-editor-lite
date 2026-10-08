#!/usr/bin/env python3
"""Tests for `convert_phoneme_symbols.py`, the one-shot YAML to JSON helper.

The source is built here instead of being committed: what matters is the mapping this script
applies (the prefix filter, the markers, and the types it does not know) and the safety defaults
it shares with the reference converter.

    python3 -m unittest discover -s scripts -p "test_*.py"
"""

import json
import shutil
import tempfile
import unittest
from pathlib import Path

import convert_phoneme_symbols as helper

SOURCE = """\
symbols:
  - symbol: en/a
    type: vowel
  - symbol: en/l
    type: liquid
  - symbol: en/s
    type: fricative
  - symbol: en/n
    type: nasal
  - symbol: en/ignored
  - symbol: ja/a
    type: vowel
  - symbol: AP
    type: marker
  - symbol: SP
    type: marker
"""


class ConvertPhonemeSymbolsTest(unittest.TestCase):
    def setUp(self):
        self.work = Path(tempfile.mkdtemp(prefix="convert-phoneme-symbols-"))
        self.addCleanup(shutil.rmtree, self.work, True)
        self.source = self.work / "symbols.yaml"
        self.source.write_text(SOURCE, encoding="utf-8")

    def run_helper(self, *arguments):
        return helper.main([str(argument) for argument in arguments])

    def test_keeps_the_prefix_and_the_markers(self):
        output = self.work / "inventory.json"
        self.assertEqual(self.run_helper(self.source, "-o", output, "--prefix", "en"), 0)
        written = json.loads(output.read_text(encoding="utf-8"))
        # `en/` is stripped, a type the table does not know becomes a consonant, an entry without a
        # type is dropped, another language is filtered out, and the markers stay for every
        # language and are vowels.
        self.assertEqual(written["phonemeTypes"],
                         {"a": "vowel", "l": "liquid", "s": "consonant", "n": "consonant",
                          "AP": "vowel", "SP": "vowel"})
        self.assertEqual(written["rules"], helper.DEFAULT_RULES)

    def test_refuses_an_existing_output(self):
        output = self.work / "inventory.json"
        output.write_text("user data", encoding="utf-8")
        self.assertEqual(self.run_helper(self.source, "-o", output), 1)
        self.assertEqual(output.read_text(encoding="utf-8"), "user data")

    def test_force_replaces_the_output(self):
        output = self.work / "inventory.json"
        output.write_text("user data", encoding="utf-8")
        self.assertEqual(self.run_helper(self.source, "-o", output, "--force"), 0)
        self.assertIn("phonemeTypes", json.loads(output.read_text(encoding="utf-8")))

    def test_reports_a_missing_input(self):
        self.assertEqual(self.run_helper(self.work / "absent.yaml", "-o", self.work / "out.json"), 1)
        self.assertFalse((self.work / "out.json").exists())

    def test_reports_a_source_without_symbols(self):
        broken = self.work / "broken.yaml"
        broken.write_text("- just\n- a list\n", encoding="utf-8")
        self.assertEqual(self.run_helper(broken, "-o", self.work / "out.json"), 1)
        self.assertFalse((self.work / "out.json").exists())

    def test_reports_the_missing_parent_directory(self):
        self.assertEqual(self.run_helper(self.source, "-o", self.work / "absent" / "out.json"), 1)


if __name__ == "__main__":
    unittest.main()
