#!/usr/bin/env python3
"""Converts a voicebank from the 2.3 package format to 2.4, so the main line can load it.

The two lines disagree from the first key: the older one lists contributions under `contributes`
as bare paths, the newer under `contributions` as {id, path} pairs; a module declaration used to
say `class` and carry its own `id`, and now says `interface` plus `variant` and is given its id by
the package; and what used to be the declaration's `schema` is now `exports`. None of that is
negotiable at load time, so a voicebank published for the older line does not load on the newer one
at all.

What this does *not* touch is where files sit. Directory layout and file names stay exactly as they
were, and only the contents of desc.json and the declarations are rewritten. Every path inside a
`configuration` is relative to its own declaration, so leaving the declarations where they are means
not rewriting a single one of them -- which is the whole class of mistake this avoids.

    python3 scripts/convert-voicebank.py <package> [--output DIR] [--in-place]
                                         [--packages DIR] [--language <handle>=<reference>]...

Reserved phonemes are read out of the voicebank rather than guessed at or passed in. A DiffSinger
phoneme table writes a phoneme belonging to a language as `<language>/<phoneme>` and one that does
not as itself, so a bare key is this voicebank saying, in its own files, that the symbol is not
speech -- a breath, a glottal stop, a hum. Every model has to agree before one is believed. Those
go into the singer's `reservedPhonemes`, where the loader checks them again, and they are kept out
of every language's declared inventory, because a host never sends one through
grapheme-to-phoneme and must not be asked to have it as a phoneme.

The language half needs help. A 2.3 voicebank names a G2P package belonging to the older line's own
G2P system, and the newer line has no such system: a language is a linguist contribution, made of a
grapheme-to-phoneme stage, a syllable-to-phoneme stage and an onset stage.

Where those three come from is not the same for every language, and that is not an accident. For a
language whose phonetics are the same for every voicebank -- English in ARPAbet, say -- the whole
linguist ships in the language package and this binds to it. For Mandarin or Japanese the
syllable-to-phoneme dictionary is voicebank content: two voicebanks singing Mandarin may use
different phoneme inventories, so the language package carries only the grapheme-to-phoneme stage
and each voicebank brings the rest. For those, this builds a linguist inside the voicebank that
imports the shared G2P and points the other two stages at the voicebank's own assets -- which the
2.3 declaration already named, since the older line needed the same files for the same reason.

Point --packages at the installed language packages and the choice is made per language by what is
actually there. A handle with no package to serve it is reported and dropped; the voicebank still
synthesises and simply has no grapheme-to-phoneme for that language.
"""

import argparse
import json
import math
import re
import os
import shutil
import sys
from pathlib import Path

# What each 2.3 `class` becomes. The variant is not a guess: these are the values the shipped
# interpreters declare, and the triple has to match exactly or no interpreter is selected.
CONTRACTS = {
    "ai.svs.AcousticInference": ("org.openvpi.dsinfer.inference.Acoustic", "onnx", "acoustic"),
    "ai.svs.DurationInference": ("org.openvpi.dsinfer.inference.Duration", "onnx", "duration"),
    "ai.svs.PitchInference": ("org.openvpi.dsinfer.inference.Pitch", "onnx", "pitch"),
    "ai.svs.VarianceInference": ("org.openvpi.dsinfer.inference.Variance", "onnx", "variance"),
    "ai.svs.VocoderInference": ("org.openvpi.dsinfer.inference.Vocoder", "onnx", "vocoder"),
    "diffsinger": ("org.openvpi.dsinfer.singer.DiffSinger", "openvpi", None),
}

# What the older line called an s2p mode, and the variant that answers it now. The names line up
# because both describe the same three ways of turning a syllable into phonemes.
S2P_VARIANTS = {"dict": "dict", "direct": "direct", "mapping": "mapping"}

# The notation each language's phonemes are written in. A linguist declares this beside its
# language handle, and the pair is what a chain member is matched against, so it cannot be
# invented per voicebank: these are the values wolf settled on, in its A45/A49 decisions, by
# reading the actual symbol inventories. `ds` is wolf's own placeholder for the five it has not
# finished naming, and is carried here rather than replaced so that both sides say the same thing.
SCHEMES = {
    "cmn": "pinyin", "yue": "jyutping", "jpn": "romaji", "eng": "arpabet",
    "zxx": "passthrough", "por": "xsampa", "kor": "romaja", "ita": "xsampa-geminate",
    "deu": "ds", "fra": "ds", "spa": "ds", "rus": "ds", "fil": "ds",
}

G2P_INTERFACE = "org.openvpi.wolf.inference.G2P"
S2P_INTERFACE = "org.openvpi.wolf.inference.S2P"
ONSET_INTERFACE = "org.openvpi.wolf.inference.Onset"
LINGUIST_INTERFACE = "org.openvpi.wolf.linguist.WolfLinguist"

# Keys a 2.3 configuration carried that the newer interpreters do not read. Dropped rather than
# passed through, because a key nothing reads is a key that lies about what the model does. Which
# of them a given configuration can lose is not the same question for every one of them, so
# `dropped_configuration_keys` answers it per configuration.
DROPPED_CONFIGURATION_KEYS = {
    # Derivable from `sampleRate` and `hopSize`, but only where that pair is there and usable.
    "frameWidth",
}

# The fields the singer category adds as multi-language paths, and the shapes 2.4 reads for them: a
# path, or a map of paths with a string `_` as the default. They are known fields with a closed
# shape, so unlike a key this converter does not recognise they cannot be carried through as they
# are -- the loader refuses the whole declaration over one it cannot read, which is worse than
# losing the field.
SINGER_PATH_FIELDS = ("avatar", "background", "demoAudio")

# Which granularity a linguistic encoder takes, told by the names it declares: a word encoder is
# given the words a lyric divides into, a phoneme encoder the durations of its phonemes. The names
# are the authority rather than the convention, because the names are what has to match at run
# time -- the interpreter prepares one set or the other and fails on the ones the model lacks.
WORD_ENCODER_INPUTS = {"word_div", "word_dur"}
PHONEME_ENCODER_INPUTS = {"ph_dur"}

# Which roles have a granularity to choose, and which one their interpreter chooses when nothing
# says otherwise. Only a role listed here can be prepared wrongly, and only a difference from the
# default is worth writing: a declaration that restates the default claims to change something it
# does not. Duration is absent because its interpreter always prepares a word encoder, so for it
# the field could only ever restate what is already true.
LINGUISTIC_MODE_DEFAULTS = {"variance": "phoneme", "pitch": "phoneme"}

# What 2.3 wrote instead, in the dsconfig.yaml beside the declaration: `predict_dur` true for the
# word encoder and false for the phoneme one, which is the pair the older line's encoder code
# switched between. It is read as the one scalar it is rather than by parsing YAML, and it is the
# second opinion only -- the model is asked first.
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
    """Checks a version against the grammar 2.4 gives and pads it to four components.

    One to four decimal components, no leading zeros. A version the grammar refuses is reported
    and returns None, since padding it would only move the refusal to load time.
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
    """Reports a structural identifier or role the specification's grammar refuses."""
    if isinstance(value, str) and pattern.fullmatch(value):
        return True
    report.error(f"{where}: {kind} {value!r} may only use ASCII letters, digits, '_' and '-'"
                 + (" in '/' separated segments" if pattern is PACKAGE_ID else ""))
    return False


def read_table(path: Path, report: Report):
    """A two column, tab separated table as {key: [phoneme, ...]}.

    Both of the table shapes a voicebank carries are read the same way: the left column is a key
    -- a syllable for a dictionary, a word for a grapheme table -- and the right is a space
    separated pronunciation.

    Byte order marks and CRLF line endings are normal in these files, and both are stripped, which
    is what the loader does when it reads the same file.
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
    """Each model's phoneme table, by contribution id.

    The table is the authority on what a model can be given, and it is also the only place this
    voicebank says which of those are language independent: a key is written `<language>/<phoneme>`
    for a phoneme that belongs to a language, and bare for one that does not.
    """
    tables = {}
    for entry in contributions.get("inference", []):
        declaration = read_json(root / entry["path"], report)
        if declaration is None:
            continue
        table = declaration.get("configuration", {}).get("phonemes")
        if not isinstance(table, str):
            continue  # the vocoder has none, which is not a fault
        path = (root / entry["path"]).parent / table
        values = read_json(path, report)
        if isinstance(values, dict):
            tables[entry["id"]] = set(values)
        elif isinstance(values, list):
            tables[entry["id"]] = set(values)
    return tables


def reserved_phonemes(tables: dict, extra: set, report: Report) -> list:
    """The phonemes a lyric may name directly, as this voicebank's own models define them.

    Not guessed and not passed in: a DiffSinger phoneme table writes a phoneme that belongs to a
    language as `<language>/<phoneme>` and one that does not as itself, so a bare key is the
    voicebank saying, in its own files, that this symbol is not speech. Every model has to agree,
    because the tables are separate files and a symbol only half the models know is worse than one
    none of them do.

    A table with no prefixed key at all says nothing about language independence -- a single
    language voicebank writes every phoneme bare -- so nothing is derived from one. Whatever
    --reserved names is added either way, and checked the same.
    """
    if not tables:
        return sorted(extra)

    qualified = any("/" in key for table in tables.values() for key in table)
    derived = set()
    if qualified:
        derived = set.intersection(*({key for key in table if "/" not in key}
                                     for table in tables.values()))
    else:
        report.note("no model names a language, so which phonemes are reserved cannot be read "
                    "from the tables; only --reserved is taken")

    result = set()
    for token in sorted(derived | extra):
        absent = sorted(model for model, table in tables.items() if token not in table)
        if absent:
            # Declaring it would refuse the package at load time, and rightly: a phoneme the
            # models lack is not a marker but a silence. Saying so here is more use than saying
            # it later.
            report.warn(f"{token}: named as reserved, but the {' '.join(absent)} model(s) do not "
                        f"have it; not declared, and it stays in the phoneme inventory where a "
                        f"host will report it as one these models cannot sing")
            continue
        result.add(token)
    return sorted(result)


def content_phonemes(entries: dict, reserved: set, known: set, handle: str, report: Report):
    """The phonemes a table can produce, less the ones this singer reserves.

    The linguist domain contract keeps reserved phonemes out of `exports.phonemes`, which is the
    *content* inventory: a host never sends one through grapheme-to-phoneme, so it must not be
    asked to have it as a phoneme either. Leaving them in makes every host measuring its singer
    against that list report a gap where there is none.

    What this also does is point out the entries that have a marker's shape and are not reserved,
    because that combination is how a voicebank goes quietly wrong. An entry that spells itself,
    using a phoneme no other entry uses, is the shape every marker has -- a real syllable like `a`
    spells itself too, but `a` turns up inside `ma`, and a marker's phoneme never does.
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
        # Neither reserved nor singable. It cannot be declared -- the loader checks a reserved
        # phoneme against every model and would refuse the package -- and it cannot be removed
        # from the inventory either, because then nothing would ever say it is unusable. It stays,
        # and a host reports it as a phoneme these models cannot sing, which is what it is.
        report.warn(f"{handle}: {' '.join(orphans)} spell themselves like reserved markers and no "
                    f"model has them; they are neither reserved nor singable, and stay in the "
                    f"inventory so that a host says so")
    if named:
        report.warn(f"{handle}: {' '.join(named)} look like reserved markers, and the models have "
                    f"them, but no model marks them language independent; pass --reserved if they "
                    f"are markers")

    return phonemes - reserved


def read_package_index(directory: Path, report: Report) -> dict:
    """What each installed language package offers, keyed by language handle.

    A package is read rather than assumed: which of the two shapes it has -- a whole linguist, or
    a grapheme-to-phoneme stage and nothing else -- is the thing this has to find out, and it is
    the thing that decides whether a voicebank binds to the package or builds its own linguist
    around it.
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
                    # A reference inside the same package is written with a leading colon; both
                    # forms have to end up spelled the same way to be compared.
                    imported.add(identifier + reference if reference.startswith(":")
                                 else reference)
        packages.append((identifier, desc, declarations))

    for identifier, desc, declarations in packages:
        # A linguist names its own language, so where there is one it is the authority and nothing
        # has to be guessed.
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
            # A shared engine is a G2P too, and it declares every language it can transcribe, so
            # going by the declaration alone would make the engine look like the answer for nine
            # languages at once. What separates the two is not the variant or the package name but
            # position: an engine is what another G2P imports and wraps, and a language's entry
            # point is the outermost G2P, which no other G2P imports. That is readable from the
            # installed set, so it is read rather than assumed.
            #
            # Only G2P-to-G2P imports count. A linguist importing a G2P is the ordinary case --
            # it is what a linguist is made of -- and would otherwise rule out every G2P that a
            # language package has already wrapped in a linguist of its own.
            if f"{identifier}:{locator}" in imported:
                continue
            # A G2P states which pairs it serves only when its output set is fixed enough to say
            # so; the ones whose phonemes are voicebank content state nothing, and for those the
            # package name is the only evidence there is.
            declared = [pair.get("language")
                        for pair in declaration.get("exports", {}).get("languages", [])]
            handles = declared or [identifier.rsplit("-", 1)[-1]]
            for handle in handles:
                if handle in index:
                    report.warn(f"{handle}: served by both {index[handle]['package']} and "
                                f"{identifier}; keeping the first")
                    continue
                linguist, scheme = linguists.get(handle, (None, None))
                # The version the package says it is compatible back to, not the version it
                # happens to be: a dependency written against the current build stops
                # resolving the day the packaging revision moves.
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
                    report.warn(f"{handle}: the package writes the scheme {scheme!r} where this "
                                f"knows it as {SCHEMES[handle]!r}; using the package's")
                if not index[handle]["scheme"]:
                    report.error(f"{handle}: no scheme is known for this language, and the "
                                 f"package does not declare one")
    return index


def synthesise_linguist(root: Path, singer_dir: Path, entry: dict, served: dict,
                        reserved: set, known: set, report: Report) -> dict:
    """Builds a voicebank side linguist around a language package's G2P.

    The three stages of a linguist do not all come from the same place. The grapheme-to-phoneme
    stage is the language's and is imported from the package; the syllable-to-phoneme stage and
    the onset rules are the voicebank's, because which phonemes this voicebank sings is its own
    decision and no package can hold that. The 2.3 declaration named both of those files for the
    same reason, so this points the new declarations at the files that are already there rather
    than copying or rewriting anything.

    Returns what the caller has to add to desc.json, or None.
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
        """Re-expresses a path the singer declaration held, relative to a new declaration."""
        absolute = (singer_dir / relative).resolve()
        if not absolute.is_file():
            report.error(f"{handle}: {relative} is not there")
            return None, None
        return absolute, Path(os.path.relpath(absolute, target_dir)).as_posix()

    # The syllable stage. `direct` needs no file: it passes the G2P's own phonemes through, which
    # is what a language whose G2P already emits phonemes rather than syllables wants.
    s2p_dir = root / "inferences" / f"s2p-{handle}"
    phonemes = set()
    configuration = {}
    if variant != "direct":
        source = entry.get("s2pFile") or entry.get("dict")
        if not source:
            report.error(f"{handle}: the {mode} mode needs a dictionary and none is named")
            return None
        absolute, relative = relocate(source, s2p_dir)
        if absolute is None:
            return None
        configuration["file"] = relative
        phonemes = content_phonemes(read_table(absolute, report), reserved, known, handle,
                                    report)
    elif entry.get("dict"):
        # Nothing reads this file on the newer line -- the G2P that would have used it belongs to
        # the language package now -- but it is still this voicebank's own statement of which
        # phonemes its English works on, so the inventory is taken from it.
        absolute, _ = relocate(entry["dict"], s2p_dir)
        if absolute is not None:
            phonemes = content_phonemes(read_table(absolute, report), reserved, known, handle,
                                    report)

    if not phonemes:
        report.warn(f"{handle}: no phoneme inventory could be read, and a linguist must declare "
                     f"one; dropped; bind one with --language {handle}=<package>:linguist/<id>")
        return None

    s2p_dir.mkdir(parents=True, exist_ok=True)
    write_json(s2p_dir / "inference.json", {
        "interface": S2P_INTERFACE,
        "level": 1,
        "variant": variant,
        "name": f"{handle} syllables",
        "configuration": configuration,
        # Declared even for `direct`, where the general advice is to leave it off: a `direct`
        # stage that sits inside one voicebank's one language is not a general module, and saying
        # which pair it serves is what lets the match happen at load time instead of at runtime.
        "exports": {"languages": [{"language": handle, "scheme": scheme}]},
    })
    inferences.append({"id": f"s2p-{handle}",
                       "path": f"./inferences/s2p-{handle}/inference.json"})
    imports.append({"role": "linguist/s2p", "ref": f":inference/s2p-{handle}"})

    # The onset stage, which is optional: a language with no rule resource simply has none, and
    # the pronunciation layer still works.
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
        # A dictionary's phonemes are all the phonemes it can produce, so that set is closed. A
        # `direct` stage hands on whatever the G2P produced, and a G2P that returns a word it
        # could not convert can produce anything, so that one is not.
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


# The path from the start of an ONNX file to the names of its inputs, in protobuf field numbers:
# the model's graph, the graph's inputs, and an input's own name. Following those three and
# stepping over everything else is what keeps this off the weights, which are most of the file.
ONNX_GRAPH_FIELD = 7
ONNX_GRAPH_INPUT_FIELD = 11
ONNX_VALUE_INFO_NAME_FIELD = 1


def read_varint(data: bytes, at: int):
    """One base-128 varint, as (value, next offset), or (None, *at*) when there is not one there."""
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
    """The payload of every length-delimited field *number* in one protobuf message.

    Every other field is stepped over by the size its wire type dictates, so a field this knows
    nothing about is passed rather than misread, and a message that stops making sense ends the
    walk instead of being guessed at.
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
    """The names an ONNX model declares as its inputs, or None when it cannot be read as one.

    What a model takes is a property of the model, so it is asked directly rather than taken from
    how a 2.3 declaration happened to be written. No dependency is needed for that: an ONNX file
    is a protobuf and the names sit at a fixed path through it, which is cheaper to walk than the
    weights it would otherwise be loaded with.
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
    """2.3's `predict_dur`, from the dsconfig.yaml beside a declaration, as a mode or None.

    The flag is one plain scalar at the left margin and is read as one: a line nested inside
    something else, or spelled otherwise, is not this flag, and answering from a guess would be
    worse than not answering. True meant the word encoder and false the phoneme one.
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
    """The linguistic mode this declaration has to name, or None when it has nothing to say.

    A 2.3 voicebank said which granularity its encoder takes with `predict_dur`, the declaration
    built from it lost the key, and the interpreter then falls back to its own default and
    prepares the other set of inputs -- which fails at run time, on a missing input name, rather
    than here. So the mode is recovered, and written only where it is a difference: a role with no
    choice, or a mode that is already the interpreter's default, is left exactly as it was.

    The model is asked first, because the names it declares are what has to match; dsconfig.yaml
    is the second opinion, taken when the model cannot be read. Where the two disagree the model
    wins and the disagreement is reported, since the stale file beside a model is the likelier of
    the two to be wrong.
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
        report.warn(f"{directory.name}: the encoder takes {from_model} inputs where the "
                    f"dsconfig.yaml beside it says {from_file}; going by the encoder")
    mode = from_model or from_file
    if mode is None or mode == default:
        return None
    return mode


def is_language_path(value) -> bool:
    """Whether a multi-language path field holds a shape 2.4 reads: a path, or a map of paths.

    The same two questions the loader asks of these fields, in the same order: a map has to name
    its default under `_`, and every value has to be a string to be a path at all.
    """
    if isinstance(value, str):
        return True
    if isinstance(value, dict):
        return (isinstance(value.get("_"), str)
                and all(isinstance(item, str) for item in value.values()))
    return False


def is_positive_number(value) -> bool:
    """Whether a JSON value is a finite number greater than zero.

    `true` is an `int` in Python and is not a number here, which is the same question the
    interpreter puts to the pair it divides before it divides by them.

    Finite is asked here too, though the runtime asks it separately and much later: a non-finite
    value compares greater than zero, so one kept on that reading alone would load, offer itself
    for selection and be refused only once a synthesis was under way, by the duration task's
    `!std::isfinite` guard (`DurationTask.cpp`:224-225) -- the "selectable, never renderable"
    shape this helper exists to keep out. `Infinity` and `NaN` reach the file only through
    Python's JSON extension for them, which is exactly why the gap is easy to leave open.

    An `int` is finite by construction and is not put to `math.isfinite`: that would convert it
    to a float first and raise `OverflowError` on an integer too large to be one.
    """
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    return value > 0 and (isinstance(value, int) or math.isfinite(value))


def dropped_configuration_keys(configuration: dict) -> set:
    """The keys of DROPPED_CONFIGURATION_KEYS this configuration can lose.

    `frameWidth` is droppable only where an interpreter derives the same value without it.
    Duration, Pitch and Variance take a `frameWidth`, or a positive `sampleRate` and `hopSize` to
    compute one from (synthrt `docs/dsinfer-level-1-revised.md`:26), and the interpreter reads the
    `frameWidth` when it is there and falls back to the pair only when it is not. So the two are
    alternatives rather than one being an older spelling of the other, and a configuration that
    carries one without the other has to keep it: 2.3 packs `frameWidth` alone in junninghua's
    duration, pitch and variance declarations, and dropping it leaves the interpreter with neither
    (inferutil's `Parser_impl.h`:432-465, which asks for a frame width and finds none) and refuses
    the whole package. A pair that is absent, not a number, or not positive is the same case: the
    interpreter derives nothing from such a pair either.

    Whatever is kept in that case has to be a value the runtime accepts, though, which is why the
    width is kept only where it is a positive number. A width the interpreter would take and the
    duration task would refuse leaves a package that loads, offers itself for selection and cannot
    render -- the worse of the two failures, since the refusal then arrives with no way to tell
    which declaration caused it.
    """
    droppable = set(DROPPED_CONFIGURATION_KEYS)
    usable_pair = (is_positive_number(configuration.get("sampleRate"))
                   and is_positive_number(configuration.get("hopSize")))
    if usable_pair:
        return droppable
    # `frameWidth` is the one with a second form to be read from instead. It is kept only if it is
    # a width: the interpreter tests a `frameWidth` for being a number and nothing more, taking any
    # number it finds (inferutil's `Parser_impl.h`:437-438), and a non-finite or non-positive one is
    # refused by the duration task once a synthesis is under way (`DurationTask.cpp`:224-225).
    # Dropping such a value instead is what puts the refusal back at load time, where the
    # configuration is read at all: with neither form present the interpreter says one is required
    # (`Parser_impl.h`:463-465).
    if is_positive_number(configuration.get("frameWidth")):
        droppable.discard("frameWidth")
    return droppable


def convert_declaration(path: Path, report: Report):
    """Rewrites one module declaration in place. Returns (id, kind) or None."""
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
        report.error(f"{path}: declaration has no id to give the package")
        return None

    # The declaration is copied and then edited, rather than rebuilt from the keys this knows: 2.4
    # keeps what it does not recognise in a framework defined object, and a declaration is one of
    # those (spec `ds-spec-2.4.md`:74; the categories read a declaration's unknown fields and say so
    # at debug level rather than refuse them, `SingerContrib.cpp`:231-247). A key this script has
    # never heard of -- a singer's avatar is the one that shows -- would otherwise leave the package
    # while the file it names stays on disk, which is a package describing itself wrongly.
    #
    # That protection is the declaration's alone, and not the entries': a contribution entry and a
    # dependency entry are each read against an allow-list that refuses anything outside it
    # (`SingerContrib.cpp`:22-31, `InferenceContrib.cpp`:49-58, `PackageLoader.cpp`:460-466). Those
    # two are built from the fields 2.4 defines instead of being carried through.
    converted = dict(declaration)
    converted["interface"] = interface
    converted["level"] = declaration.get("level", 1)
    converted["variant"] = variant

    # `class` is the older line's name for the contract the interface and variant now give, the id
    # belongs to the package that contributes the module rather than to the declaration, and
    # `$version` is the manifest format version, which 2.4 keeps in desc.json alone.
    converted.pop("class", None)
    converted.pop("id", None)
    converted.pop("$version", None)

    # 2.3's `schema` is 2.4's `exports`: the same thing, which is what a module publishes about
    # itself for an importer to read.
    if "schema" in declaration:
        converted.pop("schema", None)
        converted["exports"] = declaration["schema"]

    # A path field the newer category cannot read is dropped rather than carried: one packaging
    # tool writes `demoAudio` as a list of named clips, which is neither the shape the older line
    # defined nor the one the newer line does, and keeping it would refuse the whole declaration.
    for field in SINGER_PATH_FIELDS:
        if field in converted and not is_language_path(converted[field]):
            del converted[field]
            report.note(f"{path.parent.name}: dropped {field}, which is neither a path nor a "
                        f"language map")

    configuration = dict(declaration.get("configuration", {}))
    for dropped in dropped_configuration_keys(configuration):
        if configuration.pop(dropped, None) is not None:
            report.note(f"{path.parent.name}: dropped {dropped}, which is derived now")
    # The granularity the encoder is prepared in, which 2.3 kept in dsconfig.yaml and a converted
    # declaration otherwise loses. Declared only where it differs from what the interpreter would
    # do on its own, so a package that already loads comes out of this unchanged.
    mode = mode_to_declare(path.parent, configuration, kind, report)
    if mode is not None:
        configuration["linguisticMode"] = mode
        report.note(f"{path.parent.name}: declared a {mode} linguistic encoder")
    if configuration:
        converted["configuration"] = configuration
    else:
        # An empty configuration is not written: it would claim the variant was given parameters.
        converted.pop("configuration", None)

    # `imports` are not touched here. The copy above leaves them as they were, and the caller
    # rewrites them for a singer, which is the only category whose imports name inference kinds.

    write_json(path, converted)
    return identifier, kind


def convert_singer_imports(root: Path, path: Path, kinds: dict, overrides: dict, index: dict,
                           reserved: set, known: set, required: dict, synthesised: dict,
                           report: Report) -> None:
    """Turns 2.3 `inferenceId` imports into 2.4 role and ref pairs, and gives the singer languages.

    Collects into \a required the packages any binding referenced, so the caller can declare them,
    and into \a synthesised the contributions any linguist built here adds to the package, once
    each: one language builds the same declarations whichever singer names it, and within a
    category an `id` names one contribution.
    """

    def add_built(category: str, entries: list) -> None:
        """Records built declarations that no singer before this one has already built."""
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
            report.error(f"{path}: an import names no inferenceId")
            continue
        kind = kinds.get(target)
        if kind is None:
            report.error(f"{path}: import {target!r} does not name an inference in this package")
            continue
        converted = {"role": f"singer/{kind}", "ref": f":inference/{target}"}
        # An empty options object is not the same as none, but nothing reads one, and 2.4 lets it
        # be absent. Dropping it keeps the declaration to what it means.
        if entry.get("options"):
            converted["options"] = entry["options"]
        imports.append(converted)

    configuration = dict(declaration.get("configuration", {}))

    # The older line's per-language G2P settings describe a system the newer line does not have.
    # Carrying them would leave keys nothing reads, so they go, and what was there is reported.
    declared = configuration.pop("languages", [])
    default_language = configuration.pop("defaultLanguage", None)

    bound = {}
    for entry in declared:
        if not isinstance(entry, dict) or not entry.get("id"):
            report.error(f"{path}: a declared language has no id")
            continue
        handle = entry["id"]

        if handle in overrides:
            # An explicit binding is taken as given: someone who names a linguist has looked at
            # what is installed, and second guessing that would only ever be wrong.
            reference, version = overrides[handle]
            package = reference.split(":", 1)[0]
        else:
            served = index.get(handle)
            if served is None:
                report.warn(f"{path.parent.name}: language {handle!r} has no installed package to "
                            f"serve it and was dropped; pass --packages, or "
                            f"--language {handle}=<package>:linguist/<id>")
                continue
            if served["scheme"] is None:
                continue  # already reported while reading the package
            # Which of the two shapes applies is decided by what this voicebank brought, not by
            # what the package could have done. A voicebank that named its own phoneme stage meant
            # it: those files are its phonetics, and binding past them to a package's linguist
            # would quietly sing a different inventory than the one it ships.
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
                report.note(f"{handle}: bound to {served['linguist']}, which is whole")
                reference = served["linguist"]
                package, version = served["package"], served["version"]
            else:
                report.warn(f"{path.parent.name}: language {handle!r} brought no phoneme stage and "
                            f"{served['package']} has no whole linguist; dropped")
                continue

        role = f"lang/{handle}"
        imports.append({"role": role, "ref": reference})
        bound[handle] = role
        # Whatever reaches out of this package has to be declared as a dependency, or the
        # reference does not resolve and the loader complains about the reference rather than
        # about the declaration that is missing. That is true of a linguist built here too: the
        # linguist is this package's own, but the G2P it imports is not.
        required.setdefault(package, version)

    # Declared on the singer, because it is the singer's models that have to have them and the
    # singer is what a host holds. The same set is kept out of every language's inventory below,
    # so the two statements cannot disagree. It is a singer category field in the declaration
    # root, like the language map below, not part of the variant's configuration.
    configuration.pop("reservedPhonemes", None)
    declaration.pop("reservedPhonemes", None)
    if reserved:
        declaration["reservedPhonemes"] = sorted(reserved)

    # The two fields are the singer category's own, read by synthrt before any provider is chosen
    # and checked there for shape and for roles that exist. They sit in the declaration root beside
    # the avatar, not inside configuration, which belongs to the singer variant alone.
    declaration.pop("languages", None)
    declaration.pop("defaultLanguage", None)
    if bound:
        declaration["languages"] = bound
        # A singer that declares languages must name a default among them.
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
                report.error(f"{desc_path}: {relative} is not there")
                continue
            result = convert_declaration(declaration_path, report)
            if result is None:
                continue
            identifier, kind = result
            if target_key == "inference":
                kinds[identifier] = kind
            else:
                singers.append(declaration_path)
            # The path keeps its original spelling, so nothing inside the declaration has to move.
            converted_entries.append({"id": identifier, "path": f"./{relative}"})
        if converted_entries:
            contributions[target_key] = converted_entries

    # Read after the inference declarations have been converted, because the table each one names
    # is what says which phonemes are reserved, and that is needed before a singer is written.
    tables = read_model_phonemes(root, contributions, report)
    reserved = reserved_phonemes(tables, extra_reserved, report)
    known = set().union(*tables.values()) if tables else set()
    # A table writes a phoneme belonging to a language as `<language>/<phoneme>`; a dictionary
    # writes it bare, because a dictionary is already inside one language.
    known |= {key.split("/", 1)[1] for key in known if "/" in key}

    required = {}
    synthesised = {}
    for singer in singers:
        convert_singer_imports(root, singer, kinds, overrides, index, set(reserved), known,
                               required, synthesised, report)
    # Anything a linguist built above added to the package. Appended rather than merged into the
    # loop above because it does not exist until the singer's languages have been read.
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
    # Copied for the same reason a declaration is: what a package says about itself belongs to it,
    # and a key this script has never heard of has to survive the conversion rather than vanish.
    converted = dict(desc)
    converted["$version"] = "1.0"
    converted["id"] = package_id
    converted["version"] = version
    # A converted package claims compatibility with nothing older, which is the honest answer:
    # what it was converted from could not be loaded by this runtime at all.
    converted["compatVersion"] = version
    converted["runtimeLevel"] = 1
    # The older line listed contributions as bare paths under `contributes`; the newer one gives
    # each entry an id, so the older key is replaced rather than kept beside the new one.
    converted.pop("contributes", None)

    dependencies = []
    for entry in desc.get("dependencies", []):
        if not isinstance(entry, dict):
            continue
        # 2.4 reads a dependency entry as two fields and refuses the whole package over any other
        # one: the loader holds the entry against an allow-list of `id` and `version` and answers
        # "unknown dependency field" for the first key outside it (synthrt
        # `synthrt/lib/Core/PackageLoader.cpp`:460-466). So the entry is rebuilt from those two
        # rather than copied: 2.3 dependencies carry `name`, `url` or `optional`, and any one of
        # them left in place makes every package that names a dependency unloadable.
        dropped = sorted(set(entry) - {"id", "version", "required"})
        if dropped:
            report.note(f"{root.name}: dependency {entry.get('id')!r} dropped "
                        f"{', '.join(dropped)}, which 2.4 does not read")
        # 2.4 has no optional dependency: every one listed must resolve. A 2.3 `required: false`
        # therefore turns into a hard requirement, which is said rather than done in silence.
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
        # An empty list says the same as an absent one, and the older key is not carried over.
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
                        help="rewrite the package where it is; destructive")
    parser.add_argument("--reserved", default="", metavar="PHONEME[,PHONEME...]",
                        help="further phonemes to treat as reserved, beyond the ones the models "
                             "themselves mark as language independent. Each is still checked "
                             "against every model table, and dropped with a warning if absent")
    parser.add_argument("--packages", type=Path,
                        help="where the installed language packages are; each declared language "
                             "is served from here unless --language says otherwise")
    parser.add_argument("--language", action="append", default=[],
                        metavar="HANDLE=REFERENCE[@VERSION]",
                        help="bind a language handle to a linguist, e.g. "
                             "cmn=wolf/lang-cmn:linguist/cmn-pinyin@1.0.0.0. The version is the "
                             "lowest accepted and defaults to 0.0.0.0, meaning any")
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
            report.error(f"--language {binding}: a linguist in another package is named "
                         f"<package>:linguist/<id>")
            return 1
        pinned = four_part(version or "0.0.0.0", report, f"--language {binding}")
        if pinned is None:
            return 1
        overrides[handle] = (reference, pinned)

    if args.in_place:
        if args.output:
            report.error("--in-place and --output are not both")
            return 1
        root = args.package
    else:
        root = args.output or args.package.parent / (args.package.name + "-2.4")
        # The destination is removed before anything is read out of the package, so a destination
        # that is the package, holds it, or sits inside it takes the source with it and leaves
        # nothing behind to convert. A caller who named one of those named the wrong path, not a
        # conversion -- and the wrong path here is the one mistake that cannot be undone.
        package_parts = args.package.resolve().parts
        root_parts = root.resolve().parts
        if (root_parts[:len(package_parts)] == package_parts
                or package_parts[:len(root_parts)] == root_parts):
            report.error(f"{root}: is the package, holds it, or sits inside it, and the destination "
                         f"is removed before the copy is made; name an --output outside the package, "
                         f"or use --in-place")
            return 1
        if root.exists():
            shutil.rmtree(root)
        shutil.copytree(args.package, root)
        print(f"converting a copy at {root}")

    index = read_package_index(args.packages, report) if args.packages else {}
    if args.packages and not index:
        report.warn(f"{args.packages}: no language packages found there")

    extra = {token for token in args.reserved.split(",") if token}
    convert(root, overrides, index, extra, report)
    print(f"{report.errors} error(s), {report.warnings} warning(s)")
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
