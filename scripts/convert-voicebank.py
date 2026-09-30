#!/usr/bin/env python3
"""Converts a voicebank from the 2.3 package format to 2.4 for the synthrt main line.

The two formats differ from the root key onward. 2.3 lists contributions under `contributes` as
bare paths, and 2.4 lists them under `contributions` as {id, path} pairs. A 2.3 module declaration
specifies `class` and its own `id`; a 2.4 declaration specifies `interface` and `variant` and
receives its id from the package. The 2.3 `schema` field is the 2.4 `exports` field. The loader
accepts none of the 2.3 forms, so a voicebank published for 2.3 does not load on the main line.

The conversion does not move files. The directory layout and the file names are preserved, and only
the contents of desc.json and of the declarations are rewritten. Every path inside a
`configuration` is relative to its declaration, so keeping the declarations in place leaves all of
those paths unchanged and excludes the corresponding class of errors.

    python3 scripts/convert-voicebank.py <package> [--output DIR] [--in-place]
                                         [--packages DIR] [--language <handle>=<reference>]...

Reserved phonemes are read from the voicebank instead of being guessed or passed in. A DiffSinger
phoneme table writes a phoneme of a language as `<language>/<phoneme>` and a language-independent
phoneme without a prefix. A key without a prefix therefore indicates, in the files of the
voicebank, that the symbol is not speech, for example a breath, a glottal stop or a hum. A phoneme
is treated as reserved only if every model table contains it without a prefix. Reserved phonemes
are written to the `reservedPhonemes` field of the singer, which the loader checks again, and are
excluded from the declared inventory of every language, because a host never passes a reserved
phoneme through grapheme-to-phoneme conversion.

The language configuration requires additional input. A 2.3 voicebank references a package of the
2.3 G2P system, and 2.4 has no such system: a language is a linguist contribution, composed of a
grapheme-to-phoneme stage, a syllable-to-phoneme stage and an onset stage.

The source of these three stages depends on the language. For a language whose phonetics are the
same for every voicebank, for example English in ARPAbet, the language package contains the
complete linguist, and the converter binds the singer to it. For Mandarin or Japanese the
syllable-to-phoneme dictionary is voicebank content: two Mandarin voicebanks may use different
phoneme inventories, so the language package contains only the grapheme-to-phoneme stage and each
voicebank supplies the other stages. For such a language the converter builds a linguist inside the
voicebank that imports the shared G2P and references the assets of the voicebank for the other two
stages. The 2.3 declaration already references those assets, because 2.3 required the same files
for the same purpose.

--packages specifies the directory of the installed language packages, from which the binding of
each language is selected. A language handle without a matching package is reported and dropped;
the voicebank still synthesizes but has no grapheme-to-phoneme conversion for that language.
"""

import argparse
import json
import math
import re
import os
import shutil
import sys
from pathlib import Path

# Mapping from each 2.3 `class` to the 2.4 (interface, variant, kind) triple. The variants are the
# values that the shipped interpreters declare; the triple must match exactly, or no interpreter is
# selected.
CONTRACTS = {
    "ai.svs.AcousticInference": ("org.openvpi.dsinfer.inference.Acoustic", "onnx", "acoustic"),
    "ai.svs.DurationInference": ("org.openvpi.dsinfer.inference.Duration", "onnx", "duration"),
    "ai.svs.PitchInference": ("org.openvpi.dsinfer.inference.Pitch", "onnx", "pitch"),
    "ai.svs.VarianceInference": ("org.openvpi.dsinfer.inference.Variance", "onnx", "variance"),
    "ai.svs.VocoderInference": ("org.openvpi.dsinfer.inference.Vocoder", "onnx", "vocoder"),
    "diffsinger": ("org.openvpi.dsinfer.singer.DiffSinger", "openvpi", None),
}

# Mapping from each 2.3 s2p mode to the corresponding 2.4 variant. The names are identical because
# both formats describe the same three syllable-to-phoneme methods.
S2P_VARIANTS = {"dict": "dict", "direct": "direct", "mapping": "mapping"}

# Phoneme notation (scheme) of each language. A linguist declares the scheme beside its language
# handle, and chain members are matched against this pair, so the scheme cannot be chosen per
# voicebank. The installed language packages take precedence: the scheme is read from the
# linguist.json of the package or from the language pairs exported by its G2P, and this table is
# used only for a language whose package declares neither, such as a package from an older release.
# The values are those defined in the wolf decisions A45/A49 and match the table in wolf's
# convert-g2p-packages.py; if a package declares a different value, the difference is reported and
# the package value is used. `ds` is the wolf placeholder for the five languages whose schemes are
# not yet named; it is kept here so that both tables contain the same values.
SCHEMES = {
    "cmn": "pinyin", "yue": "jyutping", "jpn": "romaji", "eng": "arpabet",
    "zxx": "passthrough", "por": "xsampa", "kor": "romaja", "ita": "xsampa-geminate",
    "deu": "ds", "fra": "ds", "spa": "ds", "rus": "ds", "fil": "ds",
}

G2P_INTERFACE = "org.openvpi.wolf.inference.G2P"
S2P_INTERFACE = "org.openvpi.wolf.inference.S2P"
ONSET_INTERFACE = "org.openvpi.wolf.inference.Onset"
LINGUIST_INTERFACE = "org.openvpi.wolf.linguist.WolfLinguist"

# 2.3 configuration keys that the 2.4 interpreters do not read. They are dropped instead of carried
# over because an unread key misrepresents the behavior of the model. Whether a configuration can
# drop a key depends on the key and on the configuration, so `dropped_configuration_keys` computes
# the droppable set per configuration.
DROPPED_CONFIGURATION_KEYS = {
    # Derivable from `sampleRate` and `hopSize`, but only if that pair is present and usable.
    "frameWidth",
}

# Singer category fields that hold multi-language paths, and the shapes that 2.4 accepts for them: a
# path, or a map of paths with a string default under `_`. These fields have a defined, closed
# shape, so unlike an unrecognized key they cannot be carried over unchanged: the loader rejects the
# whole declaration if one of them has an unreadable shape, which is worse than losing the field.
SINGER_PATH_FIELDS = ("avatar", "background", "demoAudio")

# Granularity of a linguistic encoder, identified by the input names that it declares: a word
# encoder receives the word division of a lyric, and a phoneme encoder receives the phoneme
# durations. The input names take precedence over any convention because they must match at run
# time: the interpreter prepares one of the two input sets and fails on inputs that the model lacks.
WORD_ENCODER_INPUTS = {"word_div", "word_dur"}
PHONEME_ENCODER_INPUTS = {"ph_dur"}

# Roles with a selectable granularity, and the default granularity of their interpreters. Only a
# role listed here can be prepared with the wrong granularity, and only a value that differs from
# the default is written: a declaration that restates the default indicates a change that does not
# exist. Duration is absent because its interpreter always prepares a word encoder, so for Duration
# the field could only restate the default.
LINGUISTIC_MODE_DEFAULTS = {"variance": "phoneme", "pitch": "phoneme"}

# The 2.3 equivalent in the dsconfig.yaml beside the declaration: `predict_dur` is true for a word
# encoder and false for a phoneme encoder, matching the two paths of the 2.3 encoder code. The value
# is matched as a single scalar instead of parsing YAML, and it is only the fallback: the model is
# inspected first.
PREDICT_DUR = re.compile(r"predict_dur[ \t]*:[ \t]*(true|false)[ \t]*$")


class Report:
    def __init__(self) -> None:
        self.errors = 0
        self.warnings = 0

    def error(self, message: str) -> None:
        print(f"error: {message}", file=sys.stderr)
        self.errors += 1

    def warn(self, message: str) -> None:
        print(f"warning: {message}", file=sys.stderr)
        self.warnings += 1

    def note(self, message: str) -> None:
        print(f"  {message}")


def read_json(path: Path, report: Report):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as problem:
        report.error(f"{path}: cannot be read: {problem}")
        return None


def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


VERSION = re.compile(r"(0|[1-9][0-9]*)(\.(0|[1-9][0-9]*)){0,3}")
SEGMENT = re.compile(r"[A-Za-z0-9_-]+")
PACKAGE_ID = re.compile(r"[A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)*")


def four_part(version, report: Report, where: str):
    """Checks a version against the 2.4 grammar and pads it to four components.

    The grammar allows one to four decimal components without leading zeros. A version that does
    not match is reported and None is returned, because padding it would only defer the rejection
    to load time.
    """
    text = str(version)
    if not VERSION.fullmatch(text):
        report.error(f"{where}: version {text!r} is not one to four decimal components without "
                     f"leading zeros")
        return None
    parts = text.split(".")
    while len(parts) < 4:
        parts.append("0")
    return ".".join(parts)


def check_identifier(value, pattern: re.Pattern, kind: str, report: Report, where: str) -> bool:
    """Returns whether a structural identifier or role matches the grammar of the specification.

    A value that does not match is reported.
    """
    if isinstance(value, str) and pattern.fullmatch(value):
        return True
    report.error(f"{where}: {kind} {value!r} may only use ASCII letters, digits, '_' and '-'"
                 + (" in '/' separated segments" if pattern is PACKAGE_ID else ""))
    return False


def read_table(path: Path, report: Report):
    """Reads a two-column, tab-separated table as {key: [phoneme, ...]}.

    Both table types in a voicebank are read the same way: the left column is a key (a syllable for
    a dictionary, a word for a grapheme table), and the right column is a space-separated
    pronunciation.

    Byte order marks and CRLF line endings are common in these files. Both are stripped, as the
    loader does when it reads the same file.
    """
    entries = {}
    try:
        text = path.read_text(encoding="utf-8-sig")
    except OSError as problem:
        report.error(f"{path}: cannot be read: {problem}")
        return entries
    for line in text.splitlines():
        line = line.rstrip("\r")
        if not line:
            continue
        key, separator, pronunciation = line.partition("\t")
        if not separator:
            continue
        entries[key] = [piece for piece in pronunciation.split(" ") if piece]
    return entries


def read_model_phonemes(root: Path, contributions: dict, report: Report) -> dict:
    """Returns the phoneme table of each model, keyed by contribution id.

    The table defines the phonemes that a model accepts. It is also the only place in which the
    voicebank indicates which phonemes are language-independent: a phoneme of a language is written
    `<language>/<phoneme>`, and a language-independent phoneme has no prefix.
    """
    tables = {}
    for entry in contributions.get("inference", []):
        declaration = read_json(root / entry["path"], report)
        if declaration is None:
            continue
        table = declaration.get("configuration", {}).get("phonemes")
        if not isinstance(table, str):
            continue  # a vocoder has no phoneme table, which is valid
        path = (root / entry["path"]).parent / table
        values = read_json(path, report)
        if isinstance(values, dict):
            tables[entry["id"]] = set(values)
        elif isinstance(values, list):
            tables[entry["id"]] = set(values)
    return tables


def reserved_phonemes(tables: dict, extra: set, report: Report) -> list:
    """Returns the phonemes that a lyric may reference directly, as defined by the models.

    The phonemes are neither guessed nor passed in: a DiffSinger phoneme table writes a phoneme of a
    language as `<language>/<phoneme>` and a language-independent phoneme without a prefix, so a key
    without a prefix indicates, in the files of the voicebank, that the symbol is not speech. Every
    model table must contain the symbol, because the tables are separate files, and a symbol known
    to only some of the models is worse than a symbol known to none.

    If no table contains a prefixed key, the tables carry no information about language
    independence, because a single-language voicebank writes every phoneme without a prefix; no
    reserved phoneme is then derived. The phonemes specified with --reserved are added in both
    cases and checked the same way.
    """
    if not tables:
        return sorted(extra)

    qualified = any("/" in key for table in tables.values() for key in table)
    derived = set()
    if qualified:
        derived = set.intersection(*({key for key in table if "/" not in key}
                                     for table in tables.values()))
    else:
        report.note("no model table contains a language prefix, so reserved phonemes cannot be "
                    "derived from the tables; only --reserved is used")

    result = set()
    for token in sorted(derived | extra):
        absent = sorted(model for model, table in tables.items() if token not in table)
        if absent:
            # Declaring the phoneme would cause the loader to reject the package, correctly: a
            # phoneme that the models lack is not a marker but produces silence. Reporting it
            # here is more useful than a failure at load time.
            report.warn(f"{token}: specified as reserved, but the {' '.join(absent)} model(s) do "
                        f"not contain it; it is not declared and remains in the phoneme "
                        f"inventory, and a host reports it as a phoneme that these models "
                        f"cannot sing")
            continue
        result.add(token)
    return sorted(result)


def content_phonemes(entries: dict, reserved: set, known: set, handle: str, report: Report):
    """Returns the phonemes that a table can produce, excluding the phonemes reserved by the singer.

    The linguist domain contract excludes reserved phonemes from `exports.phonemes`, which is the
    *content* inventory: a host never passes a reserved phoneme through grapheme-to-phoneme
    conversion, so the inventory must not list it. If reserved phonemes remain in the list, every
    host that compares its singer against the list reports a gap that does not exist.

    The function also reports entries that have the shape of a marker but are not reserved, because
    that combination indicates an otherwise undetected voicebank defect. An entry that maps to
    itself and uses a phoneme that no other entry uses has the shape of a marker. A real syllable
    such as `a` also maps to itself, but `a` occurs inside `ma`, and the phoneme of a marker does
    not occur in any other entry.
    """
    phonemes = set()
    for pronunciation in entries.values():
        phonemes.update(pronunciation)

    shared = set()
    for key, pronunciation in entries.items():
        if len(pronunciation) == 1 and pronunciation[0] == key:
            continue
        shared.update(pronunciation)
    candidates = sorted({key for key, pronunciation in entries.items()
                         if len(pronunciation) == 1 and pronunciation[0] == key
                         and key not in shared and key not in reserved})

    orphans = [key for key in candidates if key not in known]
    named = [key for key in candidates if key in known]
    if orphans:
        # Neither reserved nor singable. Such a phoneme cannot be declared as reserved, because
        # the loader checks each reserved phoneme against every model and would reject the
        # package. It also cannot be removed from the inventory, because no other place would
        # then indicate that it is unusable. It remains in the inventory, and a host reports it
        # as a phoneme that these models cannot sing, which is accurate.
        report.warn(f"{handle}: {' '.join(orphans)} map to themselves like reserved markers, and "
                    f"no model contains them; they are neither reserved nor singable and remain "
                    f"in the inventory so that a host reports them")
    if named:
        report.warn(f"{handle}: {' '.join(named)} have the shape of reserved markers and the "
                    f"models contain them, but no model marks them as language-independent; pass "
                    f"--reserved if they are markers")

    return phonemes - reserved


def read_package_index(directory: Path, report: Report) -> dict:
    """Returns the contributions of each installed language package, keyed by language handle.

    Each package is read instead of assumed. The function determines which of the two shapes a
    package has (a complete linguist, or only a grapheme-to-phoneme stage), because the shape
    determines whether a voicebank binds to the package or builds its own linguist around it.
    """
    index = {}
    if not directory.is_dir():
        report.error(f"{directory}: not a directory")
        return index

    packages = []
    imported = set()
    for package in sorted(directory.iterdir()):
        desc_path = package / "desc.json"
        if not desc_path.is_file():
            continue
        desc = read_json(desc_path, report)
        if desc is None:
            continue
        identifier = desc.get("id", "")
        declarations = {}
        for category, entries in desc.get("contributions", {}).items():
            for entry in entries:
                declaration = read_json(package / entry["path"], report)
                if declaration is None:
                    continue
                declarations[f"{category}/{entry['id']}"] = declaration
                if declaration.get("interface") != G2P_INTERFACE:
                    continue
                for item in declaration.get("imports", []):
                    reference = item.get("ref", "")
                    # A reference inside the same package begins with a colon; both forms are
                    # normalized to the same spelling for comparison.
                    imported.add(identifier + reference if reference.startswith(":")
                                 else reference)
        packages.append((identifier, desc, declarations))

    for identifier, desc, declarations in packages:
        # A linguist declares its language, so if a package contains a linguist, the linguist
        # determines the language and nothing is inferred.
        linguists = {}
        for locator, declaration in declarations.items():
            if locator.startswith("linguist/") \
                    and declaration.get("interface") == LINGUIST_INTERFACE:
                linguists[declaration.get("language")] = (
                    f"{identifier}:{locator}", declaration.get("scheme"))

        for locator, declaration in sorted(declarations.items()):
            if not locator.startswith("inference/") \
                    or declaration.get("interface") != G2P_INTERFACE:
                continue
            # A shared engine is also a G2P and declares every language that it can transcribe,
            # so matching by declaration alone would select the engine for nine languages at once.
            # The engine and the entry point of a language are distinguished neither by variant
            # nor by package name but by position: an engine is imported and wrapped by another
            # G2P, and the entry point of a language is the outermost G2P, which no other G2P
            # imports. The position is determined from the installed set instead of assumed.
            #
            # Only G2P-to-G2P imports count. A linguist that imports a G2P is the normal case,
            # because a linguist is composed of such stages; counting those imports would exclude
            # every G2P that a language package has already wrapped in its own linguist.
            if f"{identifier}:{locator}" in imported:
                continue
            # A G2P declares the pairs that it serves only if its output set is fixed; a G2P whose
            # phonemes are voicebank content declares none, and for such a G2P the package name is
            # the only available evidence.
            pairs = declaration.get("exports", {}).get("languages", [])
            declared = [pair.get("language") for pair in pairs]
            exported_schemes = {pair.get("language"): pair.get("scheme") for pair in pairs
                                if pair.get("scheme")}
            handles = declared or [identifier.rsplit("-", 1)[-1]]
            for handle in handles:
                if handle in index:
                    report.warn(f"{handle}: provided by both {index[handle]['package']} and "
                                f"{identifier}; using the first")
                    continue
                linguist, scheme = linguists.get(handle, (None, None))
                # The package declares the scheme through its linguist or through the pair that
                # its G2P exports; the static table is only the fallback if it declares neither.
                scheme = scheme or exported_schemes.get(handle)
                # The oldest version that the package declares compatibility with, not its
                # current version: a dependency on the current version stops resolving as soon
                # as the packaging revision changes.
                compatible = four_part(desc.get("compatVersion", desc.get("version", "0.0.0.0")),
                                       report, f"{identifier}: desc.json")
                if compatible is None:
                    continue
                index[handle] = {
                    "package": identifier,
                    "version": compatible,
                    "g2p": f"{identifier}:{locator}",
                    "linguist": linguist,
                    "scheme": scheme or SCHEMES.get(handle),
                }
                if scheme and SCHEMES.get(handle) and scheme != SCHEMES[handle]:
                    report.warn(f"{handle}: the package declares the scheme {scheme!r}, but the "
                                f"built-in table records {SCHEMES[handle]!r}; using the package "
                                f"value")
                if not index[handle]["scheme"]:
                    report.error(f"{handle}: no scheme is recorded for this language, and the "
                                 f"package does not declare a scheme")
    return index


def synthesise_linguist(root: Path, singer_dir: Path, entry: dict, served: dict,
                        reserved: set, known: set, report: Report) -> dict:
    """Builds a voicebank-side linguist around the G2P of a language package.

    The three stages of a linguist come from different sources. The grapheme-to-phoneme stage
    belongs to the language and is imported from the package. The syllable-to-phoneme stage and
    the onset rules belong to the voicebank, because the phoneme set of a voicebank is voicebank
    content that no package can contain. The 2.3 declaration references both files for the same
    reason, so the new declarations reference the existing files instead of copying or rewriting
    them.

    Returns the entries that the caller adds to desc.json, or None on failure.
    """
    handle = entry["id"]
    scheme = served["scheme"]
    mode = entry.get("s2pMode", "dict")
    variant = S2P_VARIANTS.get(mode)
    if variant is None:
        report.error(f"{handle}: unknown s2pMode {mode!r}")
        return None

    inferences = []
    imports = [{"role": "linguist/g2p", "ref": served["g2p"]}]

    def relocate(relative: str, target_dir: Path):
        """Re-expresses a path from the singer declaration relative to a new declaration."""
        absolute = (singer_dir / relative).resolve()
        if not absolute.is_file():
            report.error(f"{handle}: {relative} does not exist")
            return None, None
        return absolute, Path(os.path.relpath(absolute, target_dir)).as_posix()

    # The syllable stage. `direct` requires no file: it passes the phonemes of the G2P through,
    # which suits a language whose G2P already produces phonemes instead of syllables.
    s2p_dir = root / "inferences" / f"s2p-{handle}"
    phonemes = set()
    configuration = {}
    if variant != "direct":
        source = entry.get("s2pFile") or entry.get("dict")
        if not source:
            report.error(f"{handle}: the {mode} mode requires a dictionary, and none is "
                         f"specified")
            return None
        absolute, relative = relocate(source, s2p_dir)
        if absolute is None:
            return None
        configuration["file"] = relative
        phonemes = content_phonemes(read_table(absolute, report), reserved, known, handle,
                                    report)
    elif entry.get("dict"):
        # No 2.4 component reads this file, because the G2P that used it now belongs to the
        # language package. The file still records the phonemes that this voicebank uses for the
        # language (for example English), so the inventory is read from it.
        absolute, _ = relocate(entry["dict"], s2p_dir)
        if absolute is not None:
            phonemes = content_phonemes(read_table(absolute, report), reserved, known, handle,
                                    report)

    if not phonemes:
        report.warn(f"{handle}: no phoneme inventory could be read, and a linguist must declare "
                    f"an inventory; the language is dropped; bind a linguist with "
                    f"--language {handle}=<package>:linguist/<id>")
        return None

    s2p_dir.mkdir(parents=True, exist_ok=True)
    write_json(s2p_dir / "inference.json", {
        "interface": S2P_INTERFACE,
        "level": 1,
        "variant": variant,
        "name": f"{handle} syllables",
        "configuration": configuration,
        # Declared even for `direct`, for which the general recommendation is to omit it: a
        # `direct` stage inside a single language of a single voicebank is not a general module,
        # and declaring the pair that it serves allows the match at load time instead of at run
        # time.
        "exports": {"languages": [{"language": handle, "scheme": scheme}]},
    })
    inferences.append({"id": f"s2p-{handle}",
                       "path": f"./inferences/s2p-{handle}/inference.json"})
    imports.append({"role": "linguist/s2p", "ref": f":inference/s2p-{handle}"})

    # The onset stage is optional: a language without a rule resource has no onset stage, and the
    # pronunciation layer still works.
    if entry.get("onsetFile"):
        mode_name = entry.get("onsetMode", "rule")
        if mode_name != "rule":
            report.error(f"{handle}: unknown onsetMode {mode_name!r}")
            return None
        onset_dir = root / "inferences" / f"onset-{handle}"
        absolute, relative = relocate(entry["onsetFile"], onset_dir)
        if absolute is None:
            return None
        onset_dir.mkdir(parents=True, exist_ok=True)
        write_json(onset_dir / "inference.json", {
            "interface": ONSET_INTERFACE,
            "level": 1,
            "variant": "rule",
            "name": f"{handle} onsets",
            "configuration": {"file": relative},
        })
        inferences.append({"id": f"onset-{handle}",
                           "path": f"./inferences/onset-{handle}/inference.json"})
        imports.append({"role": "linguist/onset", "ref": f":inference/onset-{handle}"})

    linguist_id = f"{handle}-{scheme}"
    linguist_dir = root / "linguists" / linguist_id
    linguist_dir.mkdir(parents=True, exist_ok=True)
    write_json(linguist_dir / "linguist.json", {
        "interface": LINGUIST_INTERFACE,
        "level": 1,
        "variant": "wolf",
        "name": handle,
        "language": handle,
        "scheme": scheme,
        # The phonemes of a dictionary are all the phonemes that it can produce, so that set is
        # closed. A `direct` stage passes on the output of the G2P, and a G2P that returns an
        # unconvertible word unchanged can produce any symbol, so that set is open.
        "exports": {"phonemes": sorted(phonemes), "openSet": variant == "direct"},
        "configuration": {},
        "imports": imports,
    })

    report.note(f"{handle}: built {linguist_id} on {served['g2p']}, "
                f"{len(phonemes)} phoneme(s), {len(inferences)} own stage(s)")
    return {
        "linguist": {"id": linguist_id, "path": f"./linguists/{linguist_id}/linguist.json"},
        "inferences": inferences,
        "reference": f":linguist/{linguist_id}",
        "dependency": (served["package"], served["version"]),
    }


# Protobuf field numbers on the path from the start of an ONNX file to the names of its inputs: the
# model graph, the graph inputs, and the name of an input. Following only these three fields and
# skipping all others avoids reading the weights, which make up most of the file.
ONNX_GRAPH_FIELD = 7
ONNX_GRAPH_INPUT_FIELD = 11
ONNX_VALUE_INFO_NAME_FIELD = 1


def read_varint(data: bytes, at: int):
    """Reads one base-128 varint as (value, next offset), or (None, *at*) if none is present."""
    value = 0
    shift = 0
    while at < len(data) and shift < 64:
        byte = data[at]
        at += 1
        value |= (byte & 0x7F) << shift
        if byte < 0x80:
            return value, at
        shift += 7
    return None, at


def read_submessages(data: bytes, number: int) -> list:
    """Returns the payload of every length-delimited field *number* in one protobuf message.

    Every other field is skipped by the size that its wire type specifies, so an unknown field is
    skipped instead of misread, and a malformed message ends the walk instead of being interpreted.
    """
    payloads = []
    at = 0
    while at < len(data):
        key, at = read_varint(data, at)
        if key is None:
            break
        field, wire = key >> 3, key & 7
        if wire == 2:
            length, at = read_varint(data, at)
            if length is None or at + length > len(data):
                break
            if field == number:
                payloads.append(data[at:at + length])
            at += length
        elif wire == 0:
            _, at = read_varint(data, at)
        elif wire == 5:
            at += 4
        elif wire == 1:
            at += 8
        else:
            break
    return payloads


def read_encoder_inputs(path: Path):
    """Returns the input names declared by an ONNX model, or None if the file cannot be read.

    The inputs are a property of the model, so they are read from the model instead of being
    inferred from a 2.3 declaration. No dependency is required: an ONNX file is a protobuf, and the
    input names are at a fixed field path, which is cheaper to walk than loading the model with its
    weights.
    """
    try:
        data = path.read_bytes()
    except OSError:
        return None
    names = []
    for graph in read_submessages(data, ONNX_GRAPH_FIELD):
        for entry in read_submessages(graph, ONNX_GRAPH_INPUT_FIELD):
            for name in read_submessages(entry, ONNX_VALUE_INFO_NAME_FIELD):
                names.append(name.decode("utf-8", "replace"))
    return names or None


def dsconfig_linguistic_mode(directory: Path):
    """Returns the 2.3 `predict_dur` flag from the dsconfig.yaml beside a declaration as a mode.

    The flag is matched only as a plain scalar at the left margin: a nested line or a different
    spelling is not this flag, and a guessed value would be worse than None. True indicates the
    word encoder, and false the phoneme encoder. Returns None if the flag is absent or unreadable.
    """
    try:
        text = (directory / "dsconfig.yaml").read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    for line in text.splitlines():
        match = PREDICT_DUR.match(line)
        if match:
            return "word" if match.group(1) == "true" else "phoneme"
    return None


def mode_to_declare(directory: Path, configuration: dict, kind: str, report: Report):
    """Returns the linguistic mode that the declaration must specify, or None if none is needed.

    A 2.3 voicebank specified the granularity of its encoder with `predict_dur`. The converted
    declaration does not contain that key, so the interpreter falls back to its default and
    prepares the other input set, which fails at run time on a missing input name instead of
    during conversion. The mode is therefore recovered and written only if it differs from the
    default: a role without a choice, or a mode equal to the interpreter default, is left unchanged.

    The model is inspected first, because its declared input names must match; dsconfig.yaml is
    the fallback if the model cannot be read. If the two disagree, the model value is used and the
    disagreement is reported, because the file beside a model is more likely to be outdated.
    """
    default = LINGUISTIC_MODE_DEFAULTS.get(kind)
    if default is None:
        return None

    from_model = None
    encoder = configuration.get("encoder")
    if isinstance(encoder, str):
        names = read_encoder_inputs(directory / encoder)
        if names is not None:
            words = WORD_ENCODER_INPUTS.intersection(names)
            phonemes = PHONEME_ENCODER_INPUTS.intersection(names)
            if words and not phonemes:
                from_model = "word"
            elif phonemes and not words:
                from_model = "phoneme"

    from_file = dsconfig_linguistic_mode(directory)
    if from_model and from_file and from_model != from_file:
        report.warn(f"{directory.name}: the encoder takes {from_model} inputs, but the "
                    f"dsconfig.yaml beside it specifies {from_file}; using the encoder")
    mode = from_model or from_file
    if mode is None or mode == default:
        return None
    return mode


def is_language_path(value) -> bool:
    """Returns whether a multi-language path field has a shape that 2.4 accepts.

    The accepted shapes are a path and a map of paths. The checks are those that the loader applies
    to these fields, in the same order: a map must contain its default under `_`, and every value
    must be a string.
    """
    if isinstance(value, str):
        return True
    if isinstance(value, dict):
        return (isinstance(value.get("_"), str)
                and all(isinstance(item, str) for item in value.values()))
    return False


def is_positive_number(value) -> bool:
    """Returns whether a JSON value is a finite number greater than zero.

    `true` is an `int` in Python but is not a number here, consistent with the check that the
    interpreter applies to the pair before dividing by it.

    Finiteness is checked here although the runtime checks it separately and much later: a
    non-finite value compares greater than zero, so a value accepted on that comparison alone would
    load, be offered for selection and be rejected only during synthesis by the `!std::isfinite`
    guard of the duration task (`DurationTask.cpp`:224-225). This helper exists to exclude such
    selectable but unrenderable packages. `Infinity` and `NaN` can appear in the file only through
    the Python JSON extension for them, which makes this gap easy to overlook.

    An `int` is finite by construction and is not passed to `math.isfinite`, which would convert
    it to a float first and raise `OverflowError` for an integer too large for a float.
    """
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    return value > 0 and (isinstance(value, int) or math.isfinite(value))


def dropped_configuration_keys(configuration: dict) -> set:
    """Returns the keys of DROPPED_CONFIGURATION_KEYS that this configuration can drop.

    `frameWidth` can be dropped only if an interpreter derives the same value without it.
    Duration, Pitch and Variance accept a `frameWidth`, or a positive `sampleRate` and `hopSize`
    from which it is computed (synthrt `docs/dsinfer-level-1-revised.md`:26); the interpreter reads
    `frameWidth` if present and falls back to the pair only otherwise. The two forms are therefore
    alternatives, not an old and a new spelling of one value, and a configuration that contains
    only one of them must keep it. 2.3 stores `frameWidth` alone in the duration, pitch and
    variance declarations of junninghua; dropping it leaves the interpreter with neither form
    (inferutil `Parser_impl.h`:432-465 requires a frame width and finds none), and the whole
    package is rejected. A pair that is absent, not a number or not positive is the same case,
    because the interpreter derives nothing from such a pair either.

    A kept value must be accepted by the runtime, so the width is kept only if it is a positive
    number. A width that the interpreter accepts and the duration task rejects produces a package
    that loads and is offered for selection but cannot render. That is the worse failure, because
    the rejection then gives no indication of the declaration that caused it.
    """
    droppable = set(DROPPED_CONFIGURATION_KEYS)
    usable_pair = (is_positive_number(configuration.get("sampleRate"))
                   and is_positive_number(configuration.get("hopSize")))
    if usable_pair:
        return droppable
    # `frameWidth` is the key with an alternative form. It is kept only if it is a valid width:
    # the interpreter only tests whether `frameWidth` is a number and accepts any number (inferutil
    # `Parser_impl.h`:437-438), and the duration task rejects a non-finite or non-positive value
    # during synthesis (`DurationTask.cpp`:224-225). Dropping such a value moves the rejection to
    # load time, when the configuration is read: with neither form present, the interpreter reports
    # that one of them is required (`Parser_impl.h`:463-465).
    if is_positive_number(configuration.get("frameWidth")):
        droppable.discard("frameWidth")
    return droppable


def convert_declaration(path: Path, report: Report):
    """Rewrites one module declaration in place. Returns (id, kind), or None on failure."""
    declaration = read_json(path, report)
    if declaration is None:
        return None

    kind_name = declaration.get("class")
    if kind_name not in CONTRACTS:
        report.error(f"{path}: unknown class {kind_name!r}")
        return None
    interface, variant, kind = CONTRACTS[kind_name]

    identifier = declaration.get("id")
    if not identifier:
        report.error(f"{path}: the declaration has no id for its package contribution")
        return None

    # The declaration is copied and then edited instead of being rebuilt from the known keys: 2.4
    # preserves unrecognized keys in a framework-defined object, and a declaration is such an object
    # (spec `ds-spec-2.4.md`:74; the categories log unknown fields of a declaration at debug level
    # instead of rejecting them, `SingerContrib.cpp`:231-247). An unknown key, for example the
    # avatar of a singer, would otherwise be removed from the package while the referenced file
    # remains on disk, and the package would describe itself incorrectly.
    #
    # This protection applies only to the declaration, not to the entries: a contribution entry and
    # a dependency entry are each validated against an allow-list that rejects any other field
    # (`SingerContrib.cpp`:22-31, `InferenceContrib.cpp`:49-58, `PackageLoader.cpp`:460-466). Those
    # entries are built from the fields that 2.4 defines instead of being carried over.
    converted = dict(declaration)
    converted["interface"] = interface
    converted["level"] = declaration.get("level", 1)
    converted["variant"] = variant

    # `class` is the 2.3 name for the contract that interface and variant now specify, the id
    # belongs to the contributing package instead of the declaration, and `$version` is the
    # manifest format version, which 2.4 keeps in desc.json only.
    converted.pop("class", None)
    converted.pop("id", None)
    converted.pop("$version", None)

    # The 2.3 `schema` is the 2.4 `exports`: the information that a module publishes for its
    # importers.
    if "schema" in declaration:
        converted.pop("schema", None)
        converted["exports"] = declaration["schema"]

    # A path field that the 2.4 category cannot read is dropped instead of carried over: one
    # packaging tool writes `demoAudio` as a list of named clips, which matches neither the 2.3 nor
    # the 2.4 definition, and keeping it would cause the whole declaration to be rejected.
    for field in SINGER_PATH_FIELDS:
        if field in converted and not is_language_path(converted[field]):
            del converted[field]
            report.note(f"{path.parent.name}: dropped {field}, which is neither a path nor a "
                        f"language map")

    configuration = dict(declaration.get("configuration", {}))
    for dropped in dropped_configuration_keys(configuration):
        if configuration.pop(dropped, None) is not None:
            report.note(f"{path.parent.name}: dropped {dropped}, which is derived from "
                        f"sampleRate and hopSize")
    # Granularity of the encoder input, which 2.3 kept in dsconfig.yaml and a converted
    # declaration would otherwise lose. It is declared only if it differs from the interpreter
    # default, so this step leaves a package that already loads unchanged.
    mode = mode_to_declare(path.parent, configuration, kind, report)
    if mode is not None:
        configuration["linguisticMode"] = mode
        report.note(f"{path.parent.name}: declared a {mode} linguistic encoder")
    if configuration:
        converted["configuration"] = configuration
    else:
        # An empty configuration is not written, because it would indicate variant parameters.
        converted.pop("configuration", None)

    # `imports` are not modified here. The copy above preserves them, and the caller rewrites them
    # for a singer, which is the only category whose imports reference inference kinds.

    write_json(path, converted)
    return identifier, kind


def convert_singer_imports(root: Path, path: Path, kinds: dict, overrides: dict, index: dict,
                           reserved: set, known: set, required: dict, synthesised: dict,
                           report: Report) -> None:
    """Converts 2.3 `inferenceId` imports into 2.4 role and ref pairs and binds singer languages.

    Collects into \a required the packages referenced by any binding, so that the caller can
    declare them, and into \a synthesised the contributions that any linguist built here adds to
    the package, each once: a language produces the same declarations regardless of the singer
    that references it, and within a category an `id` identifies a single contribution.
    """

    def add_built(category: str, entries: list) -> None:
        """Records the built declarations that no earlier singer has built."""
        listed = synthesised.setdefault(category, [])
        named = {entry["id"] for entry in listed}
        for entry in entries:
            if entry["id"] not in named:
                named.add(entry["id"])
                listed.append(entry)

    declaration = read_json(path, report)
    if declaration is None:
        return

    imports = []
    for entry in declaration.get("imports", []):
        target = entry.get("inferenceId")
        if not target:
            report.error(f"{path}: an import has no inferenceId")
            continue
        kind = kinds.get(target)
        if kind is None:
            report.error(f"{path}: import {target!r} does not reference an inference in this "
                         f"package")
            continue
        converted = {"role": f"singer/{kind}", "ref": f":inference/{target}"}
        # An empty options object differs from an absent options object, but no component reads
        # it, and 2.4 allows it to be absent. Dropping it keeps the declaration minimal.
        if entry.get("options"):
            converted["options"] = entry["options"]
        imports.append(converted)

    configuration = dict(declaration.get("configuration", {}))

    # The 2.3 per-language G2P settings describe a system that 2.4 does not have. Carrying them
    # over would leave unread keys, so they are removed, and the affected languages are reported.
    declared = configuration.pop("languages", [])
    default_language = configuration.pop("defaultLanguage", None)

    bound = {}
    for entry in declared:
        if not isinstance(entry, dict) or not entry.get("id"):
            report.error(f"{path}: a declared language has no id")
            continue
        handle = entry["id"]

        if handle in overrides:
            # An explicit binding is used as given: a caller who specifies a linguist has
            # inspected the installed packages, and overriding that choice would be wrong.
            reference, version = overrides[handle]
            package = reference.split(":", 1)[0]
        else:
            served = index.get(handle)
            if served is None:
                report.warn(f"{path.parent.name}: language {handle!r} has no installed package "
                            f"and was dropped; pass --packages or "
                            f"--language {handle}=<package>:linguist/<id>")
                continue
            if served["scheme"] is None:
                continue  # already reported while reading the package
            # The shape is selected by the content of the voicebank, not by the capabilities of
            # the package. If a voicebank specifies its own phoneme stage, those files define its
            # phonetics, and binding to the linguist of a package instead would silently use an
            # inventory different from the inventory that the voicebank ships.
            if entry.get("s2pFile") or entry.get("onsetFile") or entry.get("dict"):
                built = synthesise_linguist(root, path.parent, entry, served, reserved,
                                            known, report)
                if built is None:
                    continue
                add_built("linguist", [built["linguist"]])
                add_built("inference", built["inferences"])
                reference = built["reference"]
                package, version = built["dependency"]
            elif served["linguist"]:
                report.note(f"{handle}: bound to the complete linguist {served['linguist']}")
                reference = served["linguist"]
                package, version = served["package"], served["version"]
            else:
                report.warn(f"{path.parent.name}: language {handle!r} specifies no phoneme stage, "
                            f"and {served['package']} has no complete linguist; dropped")
                continue

        role = f"lang/{handle}"
        imports.append({"role": role, "ref": reference})
        bound[handle] = role
        # Every reference to another package must be declared as a dependency; otherwise the
        # reference does not resolve, and the loader reports the reference instead of the missing
        # dependency declaration. This also applies to a linguist built here: the linguist belongs
        # to this package, but the G2P that it imports does not.
        required.setdefault(package, version)

    # Declared on the singer, because the models of the singer must contain these phonemes and a
    # host operates on the singer. The same set is excluded from the inventory of every language
    # below, so the two declarations cannot contradict each other. It is a singer category field in
    # the declaration root, like the language map below, and not part of the variant configuration.
    configuration.pop("reservedPhonemes", None)
    declaration.pop("reservedPhonemes", None)
    if reserved:
        declaration["reservedPhonemes"] = sorted(reserved)

    # Both fields belong to the singer category; synthrt reads them before selecting any provider
    # and validates their shape and the referenced roles. They are in the declaration root beside
    # the avatar, not inside configuration, which belongs to the singer variant only.
    declaration.pop("languages", None)
    declaration.pop("defaultLanguage", None)
    if bound:
        declaration["languages"] = bound
        # A singer that declares languages must specify a default among them.
        if default_language in bound:
            declaration["defaultLanguage"] = default_language
        else:
            declaration["defaultLanguage"] = next(iter(bound))
            if default_language is not None:
                report.warn(f"{path.parent.name}: default language {default_language!r} was not "
                            f"bound; using {declaration['defaultLanguage']!r} instead")

    declaration["imports"] = imports
    if configuration:
        declaration["configuration"] = configuration
    elif "configuration" in declaration:
        del declaration["configuration"]
    write_json(path, declaration)


def convert(root: Path, overrides: dict, index: dict, extra_reserved: set,
            report: Report) -> None:
    desc_path = root / "desc.json"
    desc = read_json(desc_path, report)
    if desc is None:
        return

    contributes = desc.get("contributes")
    if contributes is None:
        if "contributions" in desc:
            report.error(f"{desc_path}: already in the 2.4 shape")
        else:
            report.error(f"{desc_path}: has neither contributes nor contributions")
        return

    kinds = {}
    singers = []
    contributions = {}
    for source_key, target_key in (("inferences", "inference"), ("singers", "singer")):
        entries = contributes.get(source_key, [])
        if not entries:
            continue
        converted_entries = []
        for relative in entries:
            declaration_path = root / relative
            if not declaration_path.is_file():
                report.error(f"{desc_path}: {relative} does not exist")
                continue
            result = convert_declaration(declaration_path, report)
            if result is None:
                continue
            identifier, kind = result
            if target_key == "inference":
                kinds[identifier] = kind
            else:
                singers.append(declaration_path)
            # The path keeps its original spelling, so no path inside the declaration changes.
            converted_entries.append({"id": identifier, "path": f"./{relative}"})
        if converted_entries:
            contributions[target_key] = converted_entries

    # Read after the inference declarations are converted, because the tables that they reference
    # determine the reserved phonemes, which are required before a singer is written.
    tables = read_model_phonemes(root, contributions, report)
    reserved = reserved_phonemes(tables, extra_reserved, report)
    known = set().union(*tables.values()) if tables else set()
    # A table writes a phoneme of a language as `<language>/<phoneme>`; a dictionary writes it
    # without a prefix because a dictionary belongs to a single language.
    known |= {key.split("/", 1)[1] for key in known if "/" in key}

    required = {}
    synthesised = {}
    for singer in singers:
        convert_singer_imports(root, singer, kinds, overrides, index, set(reserved), known,
                               required, synthesised, report)
    # Contributions added by the linguists built above. They are appended after the loop because
    # they exist only after the singer languages have been read.
    for category, entries in sorted(synthesised.items()):
        contributions.setdefault(category, []).extend(entries)

    version = four_part(desc.get("version", "0.0.0.0"), report, f"{root.name}: desc.json")
    if version is None:
        return
    package_id = desc.get("id", root.name)
    if not check_identifier(package_id, PACKAGE_ID, "package id", report, f"{root.name}: desc.json"):
        return
    for category, entries in contributions.items():
        for entry in entries:
            check_identifier(entry.get("id"), SEGMENT, f"{category} contribution id", report,
                             f"{root.name}: desc.json")
    # Copied for the same reason as a declaration: the package metadata belongs to the package,
    # and the conversion must preserve unknown keys.
    converted = dict(desc)
    converted["$version"] = "1.0"
    converted["id"] = package_id
    converted["version"] = version
    # A converted package declares no compatibility with older versions, because this runtime
    # could not load the source package.
    converted["compatVersion"] = version
    converted["runtimeLevel"] = 1
    # 2.3 listed contributions as bare paths under `contributes`; 2.4 assigns each entry an id,
    # so the 2.3 key is replaced instead of kept beside the new key.
    converted.pop("contributes", None)

    dependencies = []
    for entry in desc.get("dependencies", []):
        if not isinstance(entry, dict):
            continue
        # 2.4 reads a dependency entry as two fields and rejects the whole package for any other
        # field: the loader validates the entry against an allow-list of `id` and `version` and
        # reports "unknown dependency field" for the first other key (synthrt
        # `synthrt/lib/Core/PackageLoader.cpp`:460-466). The entry is therefore rebuilt from those
        # two fields instead of copied: 2.3 dependencies contain `name`, `url` or `optional`, and
        # any of them would make every package with a dependency unloadable.
        dropped = sorted(set(entry) - {"id", "version", "required"})
        if dropped:
            report.note(f"{root.name}: dependency {entry.get('id')!r} dropped "
                        f"{', '.join(dropped)}, which 2.4 does not read")
        # 2.4 has no optional dependencies: every listed dependency must resolve. A 2.3
        # `required: false` therefore becomes a hard requirement, which is reported.
        if entry.get("required") is False:
            report.warn(f"{root.name}: dependency {entry.get('id')!r} was optional in 2.3 and "
                        f"is required in 2.4, which does not support optional dependencies")
        dependencies.append({key: entry[key] for key in ("id", "version") if key in entry})
    declared = {entry.get("id") for entry in dependencies}
    for identifier, version in sorted(required.items()):
        if identifier not in declared:
            dependencies.append({"id": identifier, "version": version})
    if dependencies:
        converted["dependencies"] = dependencies
    else:
        # An empty list is equivalent to an absent list, and the 2.3 key is not carried over.
        converted.pop("dependencies", None)
    converted["contributions"] = contributions
    write_json(desc_path, converted)

    counts = ", ".join(f"{len(entries)} {category}" for category, entries
                       in sorted(contributions.items()))
    report.note(f"{root.name}: {counts}")
    if reserved:
        report.note(f"reserved phonemes, confirmed against every model: {' '.join(reserved)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("package", type=Path)
    parser.add_argument("--output", type=Path,
                        help="write the converted copy here instead of beside the original")
    parser.add_argument("--in-place", action="store_true",
                        help="rewrite the package in place; destructive")
    parser.add_argument("--reserved", default="", metavar="PHONEME[,PHONEME...]",
                        help="additional phonemes to treat as reserved, beyond those that the "
                             "models mark as language-independent. Each is still checked against "
                             "every model table and dropped with a warning if absent")
    parser.add_argument("--packages", type=Path,
                        help="directory of the installed language packages; each declared "
                             "language is bound from here unless --language specifies otherwise")
    parser.add_argument("--language", action="append", default=[],
                        metavar="HANDLE=REFERENCE[@VERSION]",
                        help="bind a language handle to a linguist, for example "
                             "cmn=wolf/lang-cmn:linguist/cmn-pinyin@1.0.0.0. The version is the "
                             "minimum accepted version and defaults to 0.0.0.0 (any version)")
    args = parser.parse_args()

    report = Report()
    if not (args.package / "desc.json").is_file():
        report.error(f"{args.package}: no desc.json")
        return 1

    overrides = {}
    for binding in args.language:
        handle, separator, reference = binding.partition("=")
        if not separator or not handle or not reference:
            report.error(f"--language {binding}: expected HANDLE=REFERENCE[@VERSION]")
            return 1
        reference, _, version = reference.partition("@")
        if ":" not in reference:
            report.error(f"--language {binding}: a linguist in another package is referenced "
                         f"as <package>:linguist/<id>")
            return 1
        pinned = four_part(version or "0.0.0.0", report, f"--language {binding}")
        if pinned is None:
            return 1
        overrides[handle] = (reference, pinned)

    if args.in_place:
        if args.output:
            report.error("--in-place and --output are mutually exclusive")
            return 1
        root = args.package
    else:
        root = args.output or args.package.parent / (args.package.name + "-2.4")
        # The destination is removed before the package is read, so a destination that is the
        # package, contains it or is inside it deletes the source and leaves nothing to convert.
        # Such a destination is a path error, and this path error cannot be undone.
        package_parts = args.package.resolve().parts
        root_parts = root.resolve().parts
        if (root_parts[:len(package_parts)] == package_parts
                or package_parts[:len(root_parts)] == root_parts):
            report.error(f"{root}: is the package, contains it or is inside it, and the "
                         f"destination is removed before the copy is made; specify an --output "
                         f"outside the package, or use --in-place")
            return 1
        if root.exists():
            shutil.rmtree(root)
        shutil.copytree(args.package, root)
        print(f"converting a copy at {root}")

    index = read_package_index(args.packages, report) if args.packages else {}
    if args.packages and not index:
        report.warn(f"{args.packages}: no language packages found")

    extra = {token for token in args.reserved.split(",") if token}
    convert(root, overrides, index, extra, report)
    print(f"{report.errors} error(s), {report.warnings} warning(s)")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
