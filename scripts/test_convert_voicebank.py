#!/usr/bin/env python3
"""Tests for `convert-voicebank.py`.

The fixture is built here instead of being committed: a minimal 2.3 package is enough for the
conversion itself and for the guards that keep the conversion from touching anything it was not
asked to touch. A conversion of a real voicebank, compared with the published 2.4 package, is a
machine-local check and is recorded in `docs/plans/synthrt-main-migration.md` (§7).

    python3 -m unittest discover -s scripts -p "test_*.py"
"""

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name("convert-voicebank.py")


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def onnx_input_names(*names: str) -> bytes:
    """Builds the little of an ONNX file that the converter walks: the graph and its input names.

    The converter reads the names straight from the protobuf instead of loading the model with its
    weights, so a fixture needs only the three nested fields on that path. Writing them here also
    checks that walk: a frozen field number would leave every name unread, and a test that expects a
    package to stay unchanged would still pass.
    """
    def length(value: int) -> bytes:
        encoded = bytearray()
        while True:
            byte = value & 0x7F
            value >>= 7
            encoded.append(byte | (0x80 if value else 0))
            if not value:
                return bytes(encoded)

    def field(number: int, payload: bytes) -> bytes:
        return bytes([number << 3 | 2]) + length(len(payload)) + payload

    graph = b"".join(field(11, field(1, name.encode("utf-8"))) for name in names)
    return field(7, graph)


class VoicebankFixture(unittest.TestCase):
    """The fixture both conversion test modules share, so that neither carries a copy of 2.3."""
    def setUp(self):
        self.work = Path(tempfile.mkdtemp(prefix="convert-voicebank-"))
        self.addCleanup(shutil.rmtree, self.work, True)
        self.package = self.work / "fixture@1.0.0.0"
        self.write_package(self.package)

    # A 2.3 package: contributions are bare paths, a module declaration carries `class` and its own
    # `id`, and the manifest carries the format version.
    def write_package(self, root: Path) -> None:
        (root / "inferences" / "acoustic").mkdir(parents=True)
        (root / "inferences" / "vocoder").mkdir(parents=True)
        write_json(root / "desc.json", {
            "id": "fixture",
            "version": "1.0.0.0",
            "name": {"_": "Fixture"},
            "vendor": {"_": "Test"},
            "contributes": {
                "inferences": ["inferences/acoustic/config.json",
                               "inferences/vocoder/config.json"],
            },
            "dependencies": [],
        })
        write_json(root / "inferences" / "acoustic" / "config.json", {
            "$version": "1.0",
            "id": "acoustic",
            "class": "ai.svs.AcousticInference",
            "level": 1,
            "configuration": {"sampleRate": 44100, "hopSize": 512, "frameWidth": 2048,
                              "phonemes": "phonemes.json"},
        })
        write_json(root / "inferences" / "acoustic" / "phonemes.json",
                   {"AP": "AP", "cmn/a": "a", "cmn/b": "b"})
        write_json(root / "inferences" / "vocoder" / "config.json", {
            "$version": "1.0",
            "id": "vocoder",
            "class": "ai.svs.VocoderInference",
            "level": 1,
            "configuration": {},
        })

    def run_script(self, *arguments):
        done = subprocess.run([sys.executable, str(SCRIPT), *map(str, arguments)],
                              capture_output=True, text=True, encoding="utf-8")
        return done.returncode, (done.stdout or "") + (done.stderr or "")

    def destination(self, name: str) -> Path:
        target = self.work / name
        shutil.rmtree(target, ignore_errors=True)
        return target

class ConvertVoicebankTest(VoicebankFixture):
    """Tests for the reference converter, which holds the only implementation of the rules."""

    def test_converts_the_package(self):
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output)
        self.assertEqual(code, 0, text)
        desc = read_json(output / "desc.json")
        self.assertEqual([entry["id"] for entry in desc["contributions"]["inference"]],
                         ["acoustic", "vocoder"])
        self.assertEqual(desc["contributions"]["inference"][0]["path"],
                         "./inferences/acoustic/config.json")
        self.assertEqual(desc.get("dependencies", []), [])
        converted = read_json(output / "inferences" / "acoustic" / "config.json")
        self.assertEqual(converted["interface"], "org.openvpi.dsinfer.inference.Acoustic")
        self.assertEqual(converted["variant"], "onnx")
        # `class`, the per-declaration `id` and the manifest version belong to 2.3 only, and the
        # width that the interpreter derives is dropped instead of being carried over.
        for absent in ("class", "$version", "schema"):
            self.assertNotIn(absent, converted)
        self.assertNotIn("frameWidth", converted["configuration"])
        self.assertEqual(converted["configuration"]["sampleRate"], 44100)

    def test_refuses_an_existing_output(self):
        output = self.destination("out")
        output.mkdir(parents=True)
        (output / "keep.txt").write_text("user data", encoding="utf-8")
        code, text = self.run_script(self.package, "--output", output)
        self.assertEqual(code, 1, text)
        self.assertIn("exists", text)
        self.assertEqual((output / "keep.txt").read_text(encoding="utf-8"), "user data")
        self.assertFalse((output / "desc.json").exists())

    def test_force_replaces_an_existing_output(self):
        output = self.destination("out")
        output.mkdir(parents=True)
        (output / "keep.txt").write_text("user data", encoding="utf-8")
        code, text = self.run_script(self.package, "--output", output, "--force")
        self.assertEqual(code, 0, text)
        self.assertFalse((output / "keep.txt").exists())
        self.assertTrue((output / "desc.json").is_file())

    def test_in_place_leaves_the_package_when_the_conversion_fails(self):
        declaration = self.package / "inferences" / "acoustic" / "config.json"
        broken = read_json(declaration)
        broken["class"] = "ai.svs.MadeUpContract"
        write_json(declaration, broken)
        before = {path.relative_to(self.package).as_posix(): path.read_bytes()
                  for path in self.package.rglob("*") if path.is_file()}
        code, text = self.run_script(self.package, "--in-place")
        self.assertEqual(code, 1, text)
        self.assertIn("unknown class", text)
        after = {path.relative_to(self.package).as_posix(): path.read_bytes()
                 for path in self.package.rglob("*") if path.is_file()}
        self.assertEqual(before, after)
        for leftover in ("fixture@1.0.0.0.converting", "fixture@1.0.0.0.replaced"):
            self.assertFalse((self.work / leftover).exists(), leftover)

    def test_in_place_replaces_the_package(self):
        code, text = self.run_script(self.package, "--in-place")
        self.assertEqual(code, 0, text)
        self.assertIn("converted in place", text)
        desc = read_json(self.package / "desc.json")
        self.assertIn("contributions", desc)
        self.assertNotIn("contributes", desc)

    def test_reports_a_declaration_listed_twice(self):
        desc_path = self.package / "desc.json"
        desc = read_json(desc_path)
        desc["contributes"]["inferences"].append(desc["contributes"]["inferences"][0])
        write_json(desc_path, desc)
        code, text = self.run_script(self.package, "--output", self.destination("out"))
        self.assertEqual(code, 1, text)
        self.assertIn("is listed twice", text)

    def test_reports_one_id_on_two_contributions(self):
        first = read_json(self.package / "inferences" / "acoustic" / "config.json")
        second_path = self.package / "inferences" / "vocoder" / "config.json"
        second = read_json(second_path)
        second["id"] = first["id"]
        write_json(second_path, second)
        code, text = self.run_script(self.package, "--output", self.destination("out"))
        self.assertEqual(code, 1, text)
        self.assertIn("is the id of both", text)

    def test_reports_a_value_of_the_wrong_type(self):
        # A declaration written by hand can carry a string where the format requires an object, and
        # reading it as one used to raise: a traceback names neither the file nor the key, and the
        # remaining declarations are never visited.
        declaration = self.package / "inferences" / "acoustic" / "config.json"
        broken = read_json(declaration)
        broken["configuration"] = "not an object"
        write_json(declaration, broken)
        code, text = self.run_script(self.package, "--output", self.destination("out"))
        self.assertEqual(code, 1, text)
        self.assertIn("configuration must be an object", text)
        self.assertNotIn("Traceback", text)

    def test_reports_a_phoneme_table_that_is_not_strings(self):
        table = self.package / "inferences" / "acoustic" / "phonemes.json"
        write_json(table, [["AP"], ["cmn/a"]])
        code, text = self.run_script(self.package, "--output", self.destination("out"))
        self.assertEqual(code, 1, text)
        self.assertIn("a phoneme table must list strings", text)
        self.assertNotIn("Traceback", text)

    def test_reports_a_malformed_installed_package(self):
        # The index of the installed language packages is read before any language is bound, and it
        # used to raise on the first package that did not have the shape it expects.
        installed = self.work / "installed" / "wolf-lang-broken"
        installed.mkdir(parents=True)
        write_json(installed / "desc.json", {
            "id": "wolf/lang-broken",
            "version": "1.0.0.0",
            "contributions": [{"id": "g2p", "path": "./inference.json"}],
        })
        write_json(installed / "inference.json", {"interface": "org.openvpi.wolf.inference.G2P"})
        code, text = self.run_script(self.package, "--output", self.destination("out"),
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 1, text)
        self.assertIn("contributions must be an object", text)
        self.assertNotIn("Traceback", text)

        write_json(installed / "desc.json", {
            "id": "wolf/lang-broken",
            "version": "1.0.0.0",
            "contributions": {"inference": ["not an object"]},
        })
        code, text = self.run_script(self.package, "--output", self.destination("out"),
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 1, text)
        self.assertIn("must be an object that names a path", text)
        self.assertNotIn("Traceback", text)

        write_json(installed / "desc.json", {
            "id": "wolf/lang-broken",
            "version": "1.0.0.0",
            "contributions": {"inference": [{"id": "g2p", "path": "./inference.json"}]},
        })
        write_json(installed / "inference.json", {
            "interface": "org.openvpi.wolf.inference.G2P",
            "imports": ["not an object"],
        })
        code, text = self.run_script(self.package, "--output", self.destination("out"),
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 1, text)
        self.assertIn("an import must be an object that names a ref", text)
        self.assertNotIn("Traceback", text)


class ConvertVoicebankOnsetLayerTest(VoicebankFixture):
    """Tests for the report on a language whose bound linguist reaches no onset layer.

    The onset layer belongs to the deployment and not to one package: a host takes the deepest layer
    of a language from the import set of the linguist that the singer binds, and from no other
    declaration. That import set is a chain rather than a set of switches, so the layer is reached
    only when the bound linguist imports both the S2P member and the onset member, and the report
    has to name the member that is missing. The voicebank supplies the chain through `onsetFile` and
    its own phoneme stage, which the linguist it builds imports, and a language package through a
    linguist of its own, which is bound only while no stage of the voicebank replaces it. The
    converter reports every binding whose linguist reaches no layer — including the binding in which
    the package declares the member on the linguist that the stage of the voicebank replaces — and
    it must not report a binding that reaches the layer, so both the warning and its absence are
    tested.

    These tests build the installed language packages as well, because the binding of a language and
    the layer it reaches are decided by what those packages declare.
    """

    def write_language_package(self, identifier: str, handle: str, scheme: str, onset: bool,
                               s2p: bool = True) -> None:
        """Writes an installed language package with a complete linguist over its own G2P.

        *onset* and *s2p* select the members of the chain that reaches the onset layer. The layer is
        reached only when the linguist imports both, so a package whose linguist imports the onset
        member and no S2P member supplies a member that no host can see.
        """
        root = self.work / "installed" / identifier.replace("/", "-")
        contributions = {
            "inference": [{"id": "g2p", "path": "./inferences/g2p/inference.json"}],
            "linguist": [{"id": f"{handle}-{scheme}",
                          "path": f"./linguists/{handle}-{scheme}/linguist.json"}],
        }
        imports = [{"role": "linguist/g2p", "ref": ":inference/g2p"}]
        write_json(root / "inferences" / "g2p" / "inference.json", {
            "interface": "org.openvpi.wolf.inference.G2P", "level": 1, "variant": "pipe-chain",
            "exports": {"languages": [{"language": handle, "scheme": scheme}]},
        })
        if s2p:
            contributions["inference"].append({"id": "s2p",
                                               "path": "./inferences/s2p/inference.json"})
            imports.append({"role": "linguist/s2p", "ref": ":inference/s2p"})
            write_json(root / "inferences" / "s2p" / "inference.json", {
                "interface": "org.openvpi.wolf.inference.S2P", "level": 1, "variant": "dict",
            })
        if onset:
            contributions["inference"].append({"id": "onset",
                                               "path": "./inferences/onset/inference.json"})
            imports.append({"role": "linguist/onset", "ref": ":inference/onset"})
            write_json(root / "inferences" / "onset" / "inference.json", {
                "interface": "org.openvpi.wolf.inference.Onset", "level": 1, "variant": "rule",
            })
        write_json(root / "desc.json", {
            "$version": "1.0", "id": identifier, "version": "1.0.0.0", "runtimeLevel": 1,
            "contributions": contributions,
        })
        write_json(root / "linguists" / f"{handle}-{scheme}" / "linguist.json", {
            "interface": "org.openvpi.wolf.linguist.WolfLinguist", "level": 1, "variant": "wolf",
            "language": handle, "scheme": scheme, "imports": imports,
        })

    def write_singer(self, languages: list) -> None:
        """Adds a 2.3 singer declaration that declares *languages*."""
        write_json(self.package / "characters" / "singer" / "config.json", {
            "$version": "1.0", "id": "singer", "class": "diffsinger", "level": 1,
            "imports": [{"inferenceId": "acoustic"}, {"inferenceId": "vocoder"}],
            "configuration": {"defaultLanguage": languages[0]["id"], "languages": languages},
        })
        desc = read_json(self.package / "desc.json")
        desc["contributes"]["singers"] = ["characters/singer/config.json"]
        write_json(self.package / "desc.json", desc)

    def stage_files(self) -> None:
        """Writes the dictionary and the rule file that a voicebank-side stage references."""
        assets = self.package / "assets"
        assets.mkdir(exist_ok=True)
        (assets / "cmn.txt").write_text("ma\tm a\n", encoding="utf-8")
        write_json(assets / "cmn_onset.json", {"phonemeTypes": {}, "rules": []})

    def role_of(self, linguist: Path, role: str):
        return [item["ref"] for item in read_json(linguist)["imports"] if item["role"] == role]

    def test_reports_a_language_that_neither_side_supplies(self):
        self.write_singer([{"id": "eng", "g2p": "g2p-eng-official"}])
        self.write_language_package("vendor/lang-eng", "eng", "arpabet", onset=False)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output,
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 0, text)
        # The linguist that is bound is named: the package supplies one and no stage of the
        # voicebank replaces it, so the member it lacks is the one the deployment reaches.
        self.assertIn("no onsetFile, and the bound linguist vendor/lang-eng:linguist/eng-arpabet "
                      "imports no linguist/onset member", text)
        self.assertNotIn("not bound", text)
        # The consequence is named, and the report says where the requirement belongs.
        self.assertIn("attributed to the previous word", text)
        self.assertIn("published combination", text)
        # The warning is not a failure: the language is bound like any other.
        singer = read_json(output / "characters" / "singer" / "config.json")
        self.assertIn("eng", singer["languages"])

    def test_reports_a_language_whose_own_linguist_has_no_onset_member(self):
        self.stage_files()
        self.write_singer([{"id": "cmn", "g2p": "g2p-cmn-official", "s2pMode": "dict",
                            "s2pFile": "../../assets/cmn.txt", "dict": "../../assets/cmn.txt"}])
        self.write_language_package("vendor/lang-cmn", "cmn", "pinyin", onset=False)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output,
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 0, text)
        # The bound linguist is the one built here, and the package contributes no member either.
        self.assertIn("no onsetFile, and the linguist built here (:linguist/cmn-pinyin) imports no "
                      "linguist/onset member", text)
        self.assertNotIn("not bound", text)
        built = output / "linguists" / "cmn-pinyin" / "linguist.json"
        self.assertEqual(self.role_of(built, "linguist/onset"), [])

    def test_silent_when_the_voicebank_supplies_the_onset_rule(self):
        self.stage_files()
        self.write_singer([{"id": "cmn", "g2p": "g2p-cmn-official", "s2pMode": "dict",
                            "s2pFile": "../../assets/cmn.txt", "dict": "../../assets/cmn.txt",
                            "onsetMode": "rule",
                            "onsetFile": "../../assets/cmn_onset.json"}])
        self.write_language_package("vendor/lang-cmn", "cmn", "pinyin", onset=False)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output,
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 0, text)
        self.assertNotIn("onset layer", text)
        # The layer is supplied by the linguist built here, which imports the member.
        built = output / "linguists" / "cmn-pinyin" / "linguist.json"
        self.assertTrue(self.role_of(built, "linguist/onset"))

    def test_reports_a_bound_linguist_that_it_cannot_inspect(self):
        # `--language` binds a linguist that the run did not read, so whether the deployment reaches
        # the layer is unknown and the report must say which condition decides it.
        self.write_singer([{"id": "eng", "g2p": "g2p-eng-official"}])
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output, "--language",
                                     "eng=vendor/other:linguist/eng-arpabet")
        self.assertEqual(code, 0, text)
        self.assertIn("cannot tell whether the deployment reaches the onset layer", text)
        self.assertIn("does not import both the linguist/s2p member and the linguist/onset member",
                      text)

    def test_silent_when_the_language_package_supplies_the_whole_chain(self):
        # The layer is reached because the linguist of the package imports both members of the
        # chain, and not because it imports the last one.
        self.write_singer([{"id": "eng", "g2p": "g2p-eng-official"}])
        self.write_language_package("vendor/lang-eng", "eng", "arpabet", onset=True, s2p=True)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output,
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 0, text)
        self.assertNotIn("onset layer", text)
        singer = read_json(output / "characters" / "singer" / "config.json")
        self.assertEqual(singer["languages"]["eng"], "lang/eng")

    def test_reports_a_bound_linguist_that_has_onset_without_s2p(self):
        # The chain and not its last ring decides the layer: a host reads the deepest layer from the
        # import set of the bound linguist, and a composition without the S2P member stops at the
        # pronunciation layer, so an onset member that such a linguist declares is never reached.
        # Staying silent here would call the deployment supplied while the host sees no layer.
        self.write_singer([{"id": "eng", "g2p": "g2p-eng-official"}])
        self.write_language_package("vendor/lang-eng", "eng", "arpabet", onset=True, s2p=False)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output,
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 0, text)
        # The missing ring is named, together with the layer that the composition stops at, and the
        # report does not leave it sounding as if no side had supplied a member.
        self.assertIn("no onsetFile, and the bound linguist vendor/lang-eng:linguist/eng-arpabet "
                      "imports the linguist/onset member but no linguist/s2p member, so the "
                      "composition stops at the pronunciation layer", text)
        self.assertNotIn("not bound", text)
        self.assertIn("attributed to the previous word", text)
        # The warning is not a failure: the language is bound like any other.
        singer = read_json(output / "characters" / "singer" / "config.json")
        self.assertIn("eng", singer["languages"])

    def test_reports_a_bound_linguist_that_has_neither_member(self):
        # A linguist with only the mandatory G2P stage reaches no layer at all, and the report names
        # both missing members instead of the onset member alone.
        self.write_singer([{"id": "eng", "g2p": "g2p-eng-official"}])
        self.write_language_package("vendor/lang-eng", "eng", "arpabet", onset=False, s2p=False)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output,
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 0, text)
        self.assertIn("no onsetFile, and the bound linguist vendor/lang-eng:linguist/eng-arpabet "
                      "imports neither the linguist/s2p member nor the linguist/onset member, so "
                      "the composition stops at the pronunciation layer", text)
        self.assertIn("attributed to the previous word", text)

    def test_reports_an_override_that_it_could_inspect(self):
        # `--language` binds the linguist of an inspected package, so the member it lacks is known
        # and the message names the linguist that the binding reaches.
        self.write_singer([{"id": "eng", "g2p": "g2p-eng-official"}])
        self.write_language_package("vendor/lang-eng", "eng", "arpabet", onset=False)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output, "--packages",
                                     self.work / "installed", "--language",
                                     "eng=vendor/lang-eng:linguist/eng-arpabet")
        self.assertEqual(code, 0, text)
        self.assertIn("no onsetFile, and the linguist vendor/lang-eng:linguist/eng-arpabet bound "
                      "by --language imports no linguist/onset member", text)
        self.assertNotIn("not bound", text)

    def test_reports_a_package_member_that_is_not_bound(self):
        # The voicebank specifies its own phoneme stage, so the linguist that is bound is the one
        # built here and the complete linguist of the package is not bound. The member of the
        # package exists and is no layer of the deployment, and the warning has to say that instead
        # of leaving it sounding as if the package supplied none.
        self.stage_files()
        self.write_singer([{"id": "cmn", "g2p": "g2p-cmn-official", "s2pMode": "dict",
                            "s2pFile": "../../assets/cmn.txt", "dict": "../../assets/cmn.txt"}])
        self.write_language_package("vendor/lang-cmn", "cmn", "pinyin", onset=True)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output,
                                     "--packages", self.work / "installed")
        self.assertEqual(code, 0, text)
        self.assertIn("no onsetFile, and the linguist built here (:linguist/cmn-pinyin) imports no "
                      "linguist/onset member", text)
        self.assertIn("vendor/lang-cmn:linguist/cmn-pinyin declares the member but is not bound",
                      text)
        # The member exists on the linguist of the package ...
        installed = (self.work / "installed" / "vendor-lang-cmn" / "linguists" / "cmn-pinyin"
                     / "linguist.json")
        self.assertTrue(self.role_of(installed, "linguist/onset"))
        # ... and the layer it would have supplied is not reached, because the singer binds the
        # linguist built here. The warning is not a failure: the language is bound like any other.
        built = output / "linguists" / "cmn-pinyin" / "linguist.json"
        self.assertEqual(self.role_of(built, "linguist/onset"), [])
        imports = read_json(output / "characters" / "singer" / "config.json")["imports"]
        self.assertIn({"role": "lang/cmn", "ref": ":linguist/cmn-pinyin"}, imports)


class ConvertVoicebankWordInputsTest(VoicebankFixture):
    """Tests for the word level inputs of a predictor, which only Duration can receive.

    A 2.3 voicebank records the granularity of its encoder as `predict_dur` and nothing about the
    inputs of its predictor: the common model of that era reads no word input at all, while a
    predictor that splits the frame budget of every word reads both word tensors. The declaration
    therefore has to be derived from the model.
    """

    def write_module(self, kind: str, contract: str, model_inputs):
        """Adds one inference module whose predictor declares *model_inputs*."""
        module = self.package / "inferences" / kind
        module.mkdir(parents=True, exist_ok=True)
        write_json(module / "phonemes.json", {"AP": "AP", "cmn/a": "a", "cmn/b": "b"})
        # The encoder of either role is prepared with the word level inputs of the score, so the
        # declaration of the predictor is the only one that can differ.
        (module / "encoder.onnx").write_bytes(
            onnx_input_names("tokens", "languages", "word_div", "word_dur"))
        (module / "predictor.onnx").write_bytes(onnx_input_names(*model_inputs))
        write_json(module / "config.json", {
            "$version": "1.0",
            "id": kind,
            "class": contract,
            "level": 1,
            "configuration": {"phonemes": "phonemes.json", "encoder": "encoder.onnx",
                              "predictor": "predictor.onnx", "sampleRate": 44100, "hopSize": 512},
        })
        manifest = read_json(self.package / "desc.json")
        manifest["contributes"]["inferences"].append(f"inferences/{kind}/config.json")
        write_json(self.package / "desc.json", manifest)
        return module

    def convert_duration(self, model_inputs):
        """Converts the package with a Duration module whose predictor declares *model_inputs*."""
        self.write_module("duration", "ai.svs.DurationInference", model_inputs)
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output)
        declaration = output / "inferences" / "duration" / "config.json"
        configuration = read_json(declaration)["configuration"] if declaration.is_file() else None
        return code, text, configuration

    def test_declares_the_word_inputs_of_the_predictor(self):
        # The predictor reads both word tensors beside the encoder state, and the task hands them
        # over only for a value that names them. A declaration that omits them makes the session
        # reject the model during synthesis, naming the inputs that are missing.
        code, text, configuration = self.convert_duration(
            ["encoder_out", "x_masks", "ph_midi", "word_div", "word_dur", "spk_embed"])
        self.assertEqual(code, 0, text)
        self.assertEqual(configuration["dur_type"], "rel")
        self.assertIn("declared dur_type=rel", text)

    def test_declares_the_word_division_alone(self):
        # A predictor that needs only the division, for example to add the position of a phoneme
        # inside its word, takes no budget: `abs` declares the division and predicts absolute
        # phoneme durations.
        code, text, configuration = self.convert_duration(
            ["encoder_out", "x_masks", "ph_midi", "word_div"])
        self.assertEqual(code, 0, text)
        self.assertEqual(configuration["dur_type"], "abs")
        self.assertNotIn("word_dur", configuration)

    def test_leaves_a_predictor_without_word_inputs_unchanged(self):
        # The usual 2.3 shape: a word encoder whose predictor reads no word input at all. No key
        # is written, so the conversion of such a package does not change. `linguisticMode` stays
        # absent as well, because the Duration interpreter always prepares the word encoder.
        code, text, configuration = self.convert_duration(
            ["encoder_out", "x_masks", "ph_midi", "spk_embed"])
        self.assertEqual(code, 0, text)
        for absent in ("dur_type", "linguisticMode"):
            self.assertNotIn(absent, configuration)
        self.assertNotIn("word level inputs", text)

    def test_reports_a_budget_without_the_division(self):
        # The interpreter reads the budget of a word through the division, so this shape of
        # predictor matches neither `dur_type` value, and declaring `rel` would name an input that
        # the model does not expect. The declaration is reported rather than written.
        code, text, _ = self.convert_duration(["encoder_out", "ph_midi", "word_dur"])
        self.assertEqual(code, 1, text)
        self.assertIn("no dur_type value describes it", text)

    def test_reports_a_word_input_that_another_role_cannot_supply(self):
        # The Pitch and Variance tasks build their predictor input without word tensors, so a model
        # of either role that declares one loads and cannot render. The warning does not fail the
        # conversion, because no declaration could make such a model render.
        self.write_module("pitch", "ai.svs.PitchInference", ["encoder_out", "ph_dur", "word_div"])
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output)
        self.assertEqual(code, 0, text)
        self.assertIn("the pitch predictor takes word_div, which its task cannot supply", text)
        configuration = read_json(output / "inferences" / "pitch" / "config.json")["configuration"]
        self.assertEqual(configuration["linguisticMode"], "word")
        self.assertNotIn("dur_type", configuration)

    def test_reports_a_predictor_whose_inputs_cannot_be_read(self):
        # The names are the only judge, so a declaration that cannot be verified is reported instead
        # of being converted as a package without word level inputs.
        module = self.write_module("duration", "ai.svs.DurationInference", ["word_div"])
        (module / "predictor.onnx").unlink()
        output = self.destination("out")
        code, text = self.run_script(self.package, "--output", output)
        self.assertEqual(code, 0, text)
        self.assertIn("cannot read the input names of predictor.onnx", text)
        declaration = read_json(output / "inferences" / "duration" / "config.json")
        self.assertNotIn("dur_type", declaration["configuration"])


if __name__ == "__main__":
    unittest.main()
