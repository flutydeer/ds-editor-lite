"""Regression checks for the voicebank packaging and release-audit tools."""

import json
import hashlib
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
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

    def test_staged_language_check_rejects_obsolete_packages(self):
        cmake = shutil.which("cmake")
        if not cmake:
            self.skipTest("CMake is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            expected = root / "current"
            expected.mkdir()
            (expected / "desc.json").write_text("{}", encoding="utf-8")
            command = [cmake, f"-DLITE_STAGED_LANG_PACKAGES_DIR={root}", "-Dexpected=current",
                       "-P", str(TOOLS.parents[2] / "cmake/LitePackagingLayoutCheck.cmake")]
            valid = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(valid.returncode, 0, valid.stdout + valid.stderr)
            obsolete = root / "obsolete"
            obsolete.mkdir()
            (obsolete / "desc.json").write_text("{}", encoding="utf-8")
            invalid = subprocess.run(command, capture_output=True, text=True)
            self.assertNotEqual(invalid.returncode, 0)
            self.assertIn("unexpected package directories", " ".join(invalid.stderr.split()))
            self.assertIn("obsolete", invalid.stderr)

    def test_linux_plugin_deployment_detects_changes_with_identical_metadata(self):
        if sys.platform != "linux":
            self.skipTest("The deployment script targets Linux")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, deployed, binaries = (root / name for name in ("source", "deployed", "bin"))
            for folder in (source, deployed, binaries):
                folder.mkdir()
            objdump = binaries / "objdump"
            objdump.write_text('#!/usr/bin/env bash\n'
                               'if [[ "$2" == */plugin.so ]]; then printf "  NEEDED libfixture.so\\n"; fi\n',
                               encoding="utf-8")
            objdump.chmod(0o755)
            (deployed / "plugin.so").write_bytes(b"plugin")
            library = source / "libfixture.so"
            library.write_bytes(b"old!")
            original = library.stat()
            environment = {**os.environ, "PATH": str(binaries) + os.pathsep + os.environ["PATH"]}
            command = ["bash", str(TOOLS.parents[2] / "scripts/deploy_linux_plugin_deps.sh"),
                       str(deployed), str(source)]
            first = subprocess.run(command, env=environment, capture_output=True, text=True)
            self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
            output = deployed / library.name
            self.assertEqual(output.read_bytes(), b"old!")
            output.write_bytes(b"RPATH rewritten")
            unchanged = subprocess.run(command, env=environment, capture_output=True, text=True)
            self.assertEqual(unchanged.returncode, 0, unchanged.stdout + unchanged.stderr)
            self.assertEqual(output.read_bytes(), b"RPATH rewritten")
            library.write_bytes(b"new!")
            os.utime(library, ns=(original.st_atime_ns, original.st_mtime_ns))
            self.assertEqual(library.stat().st_size, original.st_size)
            self.assertEqual(library.stat().st_mtime_ns, original.st_mtime_ns)
            changed = subprocess.run(command, env=environment, capture_output=True, text=True)
            self.assertEqual(changed.returncode, 0, changed.stdout + changed.stderr)
            self.assertEqual(output.read_bytes(), b"new!")

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

    def test_fetch_negative_control_detects_a_constant_digest(self):
        module = runpy.run_path(str(TOOLS / "fetch-wolf-release.py"))
        control = module["negative_control"]
        with tempfile.TemporaryDirectory() as temporary:
            self.assertTrue(control(temporary))
            with mock.patch.dict(control.__globals__, sha512_of=lambda _: "0" * 128):
                self.assertFalse(control(temporary))

    def test_comparison_replaces_a_same_sized_bad_cached_archive(self):
        module = runpy.run_path(str(TOOLS / "compare-wolf-releases.py"))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source.zip"
            source.write_bytes(b"good")
            folder = root / "tag"
            folder.mkdir()
            cached = folder / "wolf-fixture-1.0.0.0.zip"
            cached.write_bytes(b"bad!")
            expected = hashlib.sha256(b"good").hexdigest()
            asset = {"name": cached.name, "size": 4, "digest": "sha256:" + expected,
                     "browser_download_url": source.as_uri()}
            path, actual, advertised = module["ensure_archive"](root, "tag", asset)
            self.assertEqual(Path(path).read_bytes(), b"good")
            self.assertEqual(actual, expected)
            self.assertEqual(advertised, expected)
            asset["browser_download_url"] = "invalid://must-not-download"
            self.assertEqual(module["ensure_archive"](root, "tag", asset)[1], expected)

    def test_comparison_rejects_unknown_explicit_tags(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            releases = root / "releases.json"
            releases.write_text(json.dumps([{"tag_name": "lang-v1.0.0.0", "assets": []}]),
                                encoding="utf-8")
            for tags in (("missing", "lang-v1.0.0.0"), ("lang-v1.0.0.0", "missing")):
                with self.subTest(tags=tags):
                    result = self.run_tool("compare-wolf-releases.py", "--releases", releases,
                                           "--cache", root / "cache", "--tags", *tags)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn("tag not found: missing", result.stdout)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertFalse((root / "cache").exists())

    def test_comparison_rejects_duplicate_package_ids_in_one_release(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tag = "lang-v1.0.0.0"
            folder = root / tag
            folder.mkdir()
            assets = []
            for name in ("first", "second"):
                path = folder / f"wolf-{name}-1.0.0.0.zip"
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr("desc.json", json.dumps({"id": "duplicate", "version": "1.0.0.0"}))
                    archive.writestr("content.txt", name)
                assets.append({"name": path.name, "size": path.stat().st_size})
            metadata = root / "releases.json"
            metadata.write_text(json.dumps([{"tag_name": tag, "assets": assets}]), encoding="utf-8")
            result = self.run_tool("compare-wolf-releases.py", "--releases", metadata,
                                   "--cache", root, "--self-check", tag)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("duplicate package ID", result.stdout)
            self.assertNotIn("self-check passed", result.stdout)

    def test_fetch_report_cannot_replace_an_archive_or_authority(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "archives"
            output.mkdir()
            archive = output / "wolf-fixture-1.0.0.0.zip"
            payload = b"verified archive bytes"
            archive.write_bytes(payload)
            authority = root / "assets.cmake"
            source = ('set(WOLF_LANG_PACKAGES_BUNDLE_VERSION "1.0.0.0")\n'
                      f'set(WOLF_LANG_TEST_FILE "{archive.name}")\n'
                      f'set(WOLF_LANG_TEST_SHA512 "{hashlib.sha512(payload).hexdigest()}")\n')
            authority.write_text(source, encoding="utf-8")
            for report in (archive, authority):
                with self.subTest(report=report):
                    result = self.run_tool("fetch-wolf-release.py", "--assets", authority,
                                           "--out", output, "--report", report)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn("report must not overwrite", result.stdout)
                    self.assertEqual(archive.read_bytes(), payload)
                    self.assertEqual(authority.read_text(encoding="utf-8"), source)
            report = output / "report.json"
            result = self.run_tool("fetch-wolf-release.py", "--assets", authority,
                                   "--out", output, "--report", report)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(json.loads(report.read_text(encoding="utf-8"))["archives"][0]
                             ["verdict"], "sha512-ok")
            self.assertEqual(archive.read_bytes(), payload)


if __name__ == "__main__":
    unittest.main()
