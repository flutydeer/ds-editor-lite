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
# passed through, because a key nothing reads is a key that lies about what the model does.
DROPPED_CONFIGURATION_KEYS = {
    # Derivable from sampleRate and hopSize, and the newer interpreters derive it.
    "frameWidth",
}


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


def four_part(version: str) -> str:
    """Pads a version to the four components 2.4 requires."""
    parts = [part for part in str(version).split(".") if part != ""]
    while len(parts) < 4:
        parts.append("0")
    return ".".join(parts[:4])


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
                index[handle] = {
                    "package": identifier,
                    # The version the package says it is compatible back to, not the version it
                    # happens to be: a dependency written against the current build stops
                    # resolving the day the packaging revision moves.
                    "version": four_part(desc.get("compatVersion", desc.get("version", "0.0.0.0"))),
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

    if not phonemes:
        report.error(f"{handle}: no phoneme inventory could be read, and a linguist must declare "
                     f"one")
        return None

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

    converted = {
        "interface": interface,
        "level": declaration.get("level", 1),
        "variant": variant,
    }
    if "name" in declaration:
        converted["name"] = declaration["name"]

    # 2.3's `schema` is 2.4's `exports`: the same thing, which is what a module publishes about
    # itself for an importer to read.
    if "schema" in declaration:
        converted["exports"] = declaration["schema"]

    configuration = dict(declaration.get("configuration", {}))
    for dropped in DROPPED_CONFIGURATION_KEYS:
        if configuration.pop(dropped, None) is not None:
            report.note(f"{path.parent.name}: dropped {dropped}, which is derived now")
    if configuration:
        converted["configuration"] = configuration

    # imports are handled by the caller for singers, since they need the inference kinds.
    if "imports" in declaration:
        converted["imports"] = declaration["imports"]

    write_json(path, converted)
    return identifier, kind


def convert_singer_imports(root: Path, path: Path, kinds: dict, overrides: dict, index: dict,
                           reserved: set, known: set, required: dict, synthesised: dict,
                           report: Report) -> None:
    """Turns 2.3 `inferenceId` imports into 2.4 role and ref pairs, and gives the singer languages.

    Collects into \a required the packages any binding referenced, so the caller can declare them,
    and into \a synthesised the contributions any linguist built here adds to the package.
    """
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
                synthesised.setdefault("linguist", []).append(built["linguist"])
                synthesised.setdefault("inference", []).extend(built["inferences"])
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
    # so the two statements cannot disagree.
    if reserved:
        configuration["reservedPhonemes"] = sorted(reserved)
    else:
        configuration.pop("reservedPhonemes", None)

    if bound:
        configuration["languages"] = bound
        # A singer that declares languages must name a default among them.
        if default_language in bound:
            configuration["defaultLanguage"] = default_language
        else:
            configuration["defaultLanguage"] = next(iter(bound))
            if default_language is not None:
                report.warn(f"{path.parent.name}: default language {default_language!r} was not "
                            f"bound; using {configuration['defaultLanguage']!r} instead")

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

    version = four_part(desc.get("version", "0.0.0.0"))
    converted = {
        "$version": "1.0",
        "id": desc.get("id", root.name),
        "version": version,
        # A converted package claims compatibility with nothing older, which is the honest answer:
        # what it was converted from could not be loaded by this runtime at all.
        "compatVersion": version,
        "runtimeLevel": 1,
    }
    for carried in ("vendor", "copyright", "description", "readme", "name", "url", "vars"):
        if carried in desc:
            converted[carried] = desc[carried]

    dependencies = [entry for entry in desc.get("dependencies", []) if isinstance(entry, dict)]
    declared = {entry.get("id") for entry in dependencies}
    for identifier, version in sorted(required.items()):
        if identifier not in declared:
            dependencies.append({"id": identifier, "version": version})
    if dependencies:
        converted["dependencies"] = dependencies
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
        overrides[handle] = (reference, four_part(version or "0.0.0.0"))

    if args.in_place:
        if args.output:
            report.error("--in-place and --output are not both")
            return 1
        root = args.package
    else:
        root = args.output or args.package.parent / (args.package.name + "-2.4")
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
