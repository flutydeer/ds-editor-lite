#!/usr/bin/env python3
"""Converts a ds-spec 2.3 package to 2.4 with the reference converter and verifies the result.

The conversion rules have a single implementation, `convert-voicebank.py` beside this file (the
reference converter), and this script contains none of them. It loads the reference converter as a
module, passes the command line to it unchanged, and checks the package that it produces. The
mapping from `class` to `interface` and `variant`, the version grammar and the base directory of
contribution paths are taken from that module or from `ds-spec-2.4.md` and never duplicated here,
because a second copy of a rule diverges over time.

    python3 scripts/convert-package.py <package> [<option>...] [--skip-verification] [--patch DIR]

This script adds the checks that the reference converter does not perform, or performs too late.
It rejects destinations that would lose data before the package is read: the reference converter
refuses a destination that already exists unless `--force` is given, and it rejects one that is the
package, contains it or is inside it, so this script fails before a copy is made rather than after.
It also reads the converted package back, because the reference converter converts only
`contributes.inferences` and `contributes.singers`: a 2.3 package that contributes any other
category, for example the linguists of a language package, keeps those declarations unchanged, and
this script has no rules for them either. Such a result is reported as a failure instead of being
passed on as a converted package. The reference converter is the single implementation of the
conversion rules, so this script names its values and its functions instead of citing their lines,
which drifted as soon as the reference grew.

`--patch DIR` requests a patch, which avoids distributing the models twice: the two forms of a
voicebank differ in their declarations, not in their inference files, so only the files whose
content differs are included. A `patch.json` beside them records the package id and version to
which the patch applies and lists every file with its current hash and its size and hash after
patching. `DIR.zip` contains the same entries in a single file for distribution. A `DIR` that
overlaps the package or the converted copy, or that already contains an earlier patch, is rejected.

All other arguments are forwarded to the reference converter unchanged, so the command line is the
same as for a direct call: `--output DIR`, `--in-place` (which rewrites the package in place and is
documented as destructive), `--packages DIR`, `--language HANDLE=REFERENCE[@VERSION]` and
`--reserved PHONEME[,...]`, each described in its `--help`. This script handles four options
itself: `--skip-verification`, `--patch`, `-h` and `--help`. The package must be the first
argument, as in the usage line, because this script does not reorder the forwarded arguments. With
`--output`, the converted copy is verified at the path that the caller specified; with neither
`--output` nor `--in-place`, it is expected at the default destination of the reference converter.

Exit codes: 0 if the package was converted and, unless verification was skipped, the converted
package passed verification; 1 if the reference converter reported an error, a guard rejected the
command line before anything was copied, verification found a defect that prevents loading, or a
requested patch could not be written; 2 from the argument parser, following its convention, for a
command line that this script cannot parse. Any non-zero code means that the destination does not
contain a usable 2.4 package, and stderr reports the reason in the `error: ...` / `warning: ...`
format of the reference converter; the Report class below is taken from that module so that both
outputs have the same format. The reference converter prints its own count line when it runs; the
count line of this script is then prefixed with `convert-package:` so that the two lines are not
read as a single count that changed.
"""

import argparse
import hashlib
import importlib.util
import shutil
import sys
import zipfile
from pathlib import Path

# AUTHORITY SYNC POINT, the sync point with the authoritative implementation. Nothing that the
# reference converter defines is restated here: every constant, helper and report class is read from
# the module this script loads. The names that are read are enumerated by the test of this rule
# (`test_convert_package.py`: `test_the_wrapper_restates_no_value_of_the_reference`), and no list is
# copied here, because such a list goes stale: an earlier version restated the values with line
# citations, and the citations went stale as soon as the reference grew, which is the second copy of
# a rule that the docstring above forbids. The two definitions below are this script's own.
AUTHORITY_FILENAME = "convert-voicebank.py"
PATCH_MANIFEST = "patch.json"


def load_authority(path: Path):
    """Loads the reference converter as a module without passing it a command line.

    The file name contains a hyphen, so the module cannot be imported by name and is loaded from a
    spec built from its path. Loading it converts nothing and does not exit: its entry point is
    guarded by `if __name__ == "__main__"`, and its top level only defines constants, compiled
    patterns and functions. No bytecode is written beside it: this script runs inside the repository
    that contains the reference converter, and a cache directory would be an unrequested change to
    that repository.
    """
    saved = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        spec = importlib.util.spec_from_file_location("convert_voicebank", path)
        if spec is None or spec.loader is None:
            raise ImportError(f"{path}: no module can be built from this file")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        sys.dont_write_bytecode = saved


def run_authority(module, path: Path, arguments: list) -> int:
    """Runs the entry point of the reference converter on *arguments* in this interpreter.

    The call runs in this interpreter instead of a child process. `main()` reads the command line
    from `sys.argv`, so the arguments are placed there for the duration of the call and restored
    afterwards. Returns the documented exit code of the reference converter, which prints its
    error and warning counts itself.
    """
    saved = sys.argv
    sys.argv = [str(path)] + list(arguments)
    try:
        return module.main()
    finally:
        sys.argv = saved


def finish(report, code: int, after_authority: bool = False) -> int:
    """Prints the counts of this script, labeled if the reference converter printed its own.

    The reference converter prints a count line whenever it runs, and
    it counts the findings of the conversion, not the findings of the verification. An unlabeled
    second line would read as the same count contradicting itself, for example four warnings
    followed by none. With the label, the two lines report two separate counts, and neither
    obscures the other. Returns *code*.
    """
    label = "convert-package: " if after_authority else ""
    print(f"{label}{report.errors} error(s), {report.warnings} warning(s)")
    return code


def spelling(text: str) -> Path:
    """Returns a declaration path in the spelling of this file system.

    The two formats write the same relative path differently (2.3 as a bare path, 2.4 with a
    leading `./`), and a declaration may use either slash (`ds-spec-2.4.md`《声明路径》), so the paths
    cannot be compared as strings.
    """
    pieces = [piece for piece in text.replace("\\", "/").split("/") if piece not in ("", ".")]
    return Path(*pieces) if pieces else Path(".")


def contribution_paths(listing) -> list:
    """Returns every path in a contribution listing as (category, path) pairs.

    2.3 lists bare path strings, and 2.4 lists objects with a `path` field
    (`ds-spec-2.4.md`《描述文件》, the `contributions` example). A category whose entries have no path (the
    specification leaves this to the category, `ds-spec-2.4.md`《功能贡献》) contributes no pairs.
    """
    pairs = []
    if not isinstance(listing, dict):
        return pairs
    for category, entries in listing.items():
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if isinstance(entry, str):
                pairs.append((category, entry))
            elif isinstance(entry, dict) and isinstance(entry.get("path"), str):
                pairs.append((category, entry["path"]))
    return pairs


def landing_of(arguments: list, package: Path, authority):
    """Returns the destination of the converted copy for the forwarded command line.

    Returns (landing, guessed, in_place). *guessed* indicates that no destination was specified and
    that the default destination of the reference converter was assumed. This default is the only
    part of the command-line semantics of the reference converter that this script must know,
    because a package that cannot be found afterwards cannot be verified. A missing package at an
    assumed destination is a warning, not a failure, because the assumption belongs to this script.
    """
    for index, token in enumerate(arguments):
        if token.startswith("--output="):
            return Path(token.split("=", 1)[1]), False, False
        if token == "--output" and index + 1 < len(arguments):
            return Path(arguments[index + 1]), False, False
    if "--in-place" in arguments:
        # The package is rewritten in place, which is the destination that the reference
        # converter uses for --in-place, after it converted a staging copy beside the package.
        return package, False, True
    return package.parent / (package.name + authority.DEFAULT_OUTPUT_SUFFIX), True, False


def read_23_package(package: Path, authority, report):
    """Returns the parsed desc.json of the 2.3 package, or None if there is nothing to convert.

    Applies the rejections of the reference converter for the manifest file and for its shape
    before the tree is copied instead of after, so that a command line that cannot produce a
    converted package does not first leave a copy of the 2.3 package behind.
    """
    desc_path = package / "desc.json"
    if not desc_path.is_file():
        report.error(f"{package}: no desc.json")
        return None
    desc = authority.read_json(desc_path, report)
    if desc is None:
        return None
    if "contributes" not in desc:
        if "contributions" in desc:
            report.error(f"{desc_path}: already in the 2.4 shape")
        else:
            report.error(f"{desc_path}: has neither contributes nor contributions")
        return None
    return desc


def check_manifest(converted, desc_path: Path, authority, report) -> None:
    """Checks the fields that the specification reads first and the root key that 2.4 renamed.

    `$version` and `runtimeLevel` are read before `contributions` by design
    (`ds-spec-2.4.md`《描述文件》的读取顺序):
    a package with an invalid value in either field is rejected as a whole before any contribution
    is examined, so a converted package with such a value must not be passed on. The two version
    fields and the package id are checked with the functions of the reference converter instead of
    a second implementation of the grammar.
    """
    if converted.get("$version") != authority.MANIFEST_VERSION:
        report.error(f"{desc_path}: $version is {converted.get('$version')!r}, but 2.4 requires "
                     f"{authority.MANIFEST_VERSION!r} (`ds-spec-2.4.md`:144)")
    if "contributes" in converted:
        report.error(f"{desc_path}: still lists contributions under `contributes`, which 2.4 "
                     f"renamed to `contributions` (`ds-spec-2.4.md`:16)")
    level = converted.get("runtimeLevel")
    if not isinstance(level, int) or not authority.is_positive_number(level):
        report.error(f"{desc_path}: runtimeLevel is {level!r}, and 2.4 requires a positive integer "
                     f"(`ds-spec-2.4.md`:210)")
    identifier = converted.get("id")
    if not isinstance(identifier, str) or not identifier:
        report.error(f"{desc_path}: no id, which every package must declare "
                     f"(`ds-spec-2.4.md`:145)")
    else:
        authority.check_identifier(identifier, authority.PACKAGE_ID, "package id", report,
                                   f"{desc_path}")
    if "version" not in converted:
        report.error(f"{desc_path}: no version, which every package must declare "
                     f"(`ds-spec-2.4.md`:146)")
    else:
        authority.four_part(converted["version"], report, f"{desc_path}: version")
    if "compatVersion" in converted:
        # Optional; defaults to `version` if absent (`ds-spec-2.4.md`《描述文件》).
        authority.four_part(converted["compatVersion"], report, f"{desc_path}: compatVersion")


def check_contributions(desc, converted, landing: Path, authority, report) -> int:
    """Checks that every declaration listed by the 2.3 package exists in the converted package.

    The reference converter converts only the two categories of a voicebank, so the check must
    detect any 2.3 contribution that is missing from the result. The comparison uses paths instead
    of category names: the mapping from 2.3 to 2.4 categories is defined by the reference converter,
    and the name of a category that the specification leaves open (`ds-spec-2.4.md`《贡献类别是开放的》)
    is chosen by its registrant.

    Returns the number of contributions in the converted package, including those that the
    reference converter built during the conversion; those are additions, so the comparison is
    one-directional.
    """
    contributed = contribution_paths(converted.get("contributions"))
    kept = {spelling(path) for _, path in contributed}
    dropped = {}
    for category, path in contribution_paths(desc.get("contributes")):
        if spelling(path) not in kept:
            dropped.setdefault(category, []).append(path)
    for category in sorted(dropped):
        report.error(f"{landing}/desc.json: contributes.{category} listed "
                     f"{', '.join(sorted(dropped[category]))}, but the converted package "
                     f"contributes no declaration at that path: the reference converter converts "
                     f"only the two voicebank categories, and this "
                     f"script has no rules for other categories")

    for path in sorted(kept):
        declaration_path = landing / path
        if not declaration_path.is_file():
            report.error(f"{declaration_path}: listed as a contribution but does not exist")
            continue
        declaration = authority.read_json(declaration_path, report)
        if not isinstance(declaration, dict):
            continue
        if "class" in declaration:
            report.error(f"{declaration_path}: still contains `class`, which 2.4 replaces with "
                         f"`interface` and `variant` (`ds-spec-2.4.md`:25)")
        if "$version" in declaration:
            report.error(f"{declaration_path}: still contains `$version`, which 2.4 keeps in "
                         f"desc.json only (`ds-spec-2.4.md`:29)")
    return len(kept)


def verify(desc, landing: Path, guessed: bool, authority, report) -> None:
    """Verifies the converted package against the 2.3 source package and the specification."""
    desc_path = landing / "desc.json"
    if not desc_path.is_file():
        if guessed:
            report.warn(f"{landing}: no desc.json; the converted package was not found at the "
                        f"default destination of the reference converter and was not verified; "
                        f"specify --output to verify the result")
            return
        report.error(f"{desc_path}: the converter reported no error but wrote no desc.json")
        return
    converted = authority.read_json(desc_path, report)
    if converted is None:
        return
    check_manifest(converted, desc_path, authority, report)
    contributed = check_contributions(desc, converted, landing, authority, report)
    report.note(f"{landing}: {contributed} contribution(s) verified")


def tree(root: Path) -> dict:
    """Returns every file under *root* as a map from package-relative path to size."""
    listing = {}
    for path in sorted(root.rglob("*")):
        if path.is_file():
            listing[path.relative_to(root).as_posix()] = path.stat().st_size
    return listing


def digest(path: Path) -> str:
    """Returns the SHA-256 of one file, read in blocks instead of at once."""
    running = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            running.update(block)
    return running.hexdigest()


def differences(package: Path, landing: Path) -> tuple:
    """Returns the changes of the conversion as (changed, removed) package-relative paths.

    The comparison uses content instead of timestamps: a conversion rewrites files in place, and a
    file with unchanged bytes does not belong in a patch. Sizes are compared first, and hashes only
    if the sizes are equal, so the model files are read once.
    """
    before = tree(package)
    after = tree(landing)
    changed = []
    for relative, size in sorted(after.items()):
        if relative not in before:
            changed.append(relative)
        elif size != before[relative] or digest(landing / relative) != digest(package / relative):
            changed.append(relative)
    removed = sorted(relative for relative in before if relative not in after)
    return changed, removed


def patch_entry(package: Path, landing: Path, relative: str) -> dict:
    """Returns the manifest entry of one file: its hash before and its size and hash after patching.

    `before` allows an applier to check the package before patching it, and is null for a file
    that the conversion adds, which must be absent from the package.
    """
    source = package / relative
    target = landing / relative
    return {"path": relative,
            "before": digest(source) if source.is_file() else None,
            "size": target.stat().st_size,
            "sha256": digest(target)}


def write_patch(package: Path, landing: Path, destination: Path, desc, authority, report) -> None:
    """Writes the files changed by the conversion, a manifest listing them, and a zip of both.

    The manifest makes a patch safe to distribute: it records the package id and version to which
    the patch applies and gives every file both its current hash and its hash after patching, so an
    applier can reject a package from which the patch was not made instead of producing a mixture.
    """
    desc_path = landing / "desc.json"
    if not desc_path.is_file():
        report.error(f"{destination}: no patch written: {desc_path} does not exist, so the "
                     f"package cannot be compared with the converted copy")
        return
    converted = authority.read_json(desc_path, report)
    if converted is None:
        return

    changed, removed = differences(package, landing)
    for relative in changed:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(landing / relative, target)

    manifest = {
        # The manifest version identifies the format to which the patch applies: a 2.3 package has
        # no manifest version (`ds-spec-2.4.md`《描述文件》).
        "from": {"id": desc.get("id"), "version": desc.get("version"),
                 "manifestVersion": desc.get("$version")},
        "to": {"id": converted.get("id"), "version": converted.get("version"),
               "manifestVersion": converted.get("$version"),
               "runtimeLevel": converted.get("runtimeLevel")},
        "files": [patch_entry(package, landing, relative) for relative in changed],
        "removed": [{"path": relative, "before": digest(package / relative)}
                    for relative in removed],
    }
    authority.write_json(destination / PATCH_MANIFEST, manifest)

    archive = destination.parent / (destination.name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zipped:
        for path in sorted(destination.rglob("*")):
            if path.is_file():
                zipped.write(path, path.relative_to(destination).as_posix())

    report.note(f"{archive}: {len(changed)} file(s) written, "
                f"{sum(entry['size'] for entry in manifest['files'])} byte(s), "
                f"{len(removed)} to be removed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     allow_abbrev=False,
                                     usage="convert-package.py <package> [<option>...] "
                                           "[--skip-verification] [--patch DIR]")
    parser.add_argument("--skip-verification", action="store_true",
                        help="do not read the converted package back; the result of the "
                             "reference converter is accepted without verification")
    parser.add_argument("--patch", metavar="DIR",
                        help="also write the files the conversion changed into DIR, with a "
                             f"{PATCH_MANIFEST} manifest and a DIR.zip beside it")
    options, forwarded = parser.parse_known_args()
    arguments = list(forwarded)

    authority_path = Path(__file__).resolve().parent / AUTHORITY_FILENAME
    try:
        authority = load_authority(authority_path)
    except (ImportError, OSError) as problem:
        print(f"error: {authority_path}: the reference converter for the 2.3 to 2.4 rules "
              f"cannot be loaded: {problem}", file=sys.stderr)
        return 1
    report = authority.Report()

    if not arguments or arguments[0].startswith("-"):
        report.error(f"the package must be the first argument, as in the usage of "
                     f"{AUTHORITY_FILENAME}; this script forwards the command line unchanged and "
                     f"does not infer which argument is the package")
        return finish(report, 1)

    package = Path(arguments[0])
    landing, guessed, in_place = landing_of(arguments, package, authority)
    # The relation is the one the reference converter rejects, but checking it here fails before the
    # package is read, so a command line that would lose the package leaves no partial copy behind.
    if not in_place and authority.overlapping(package, landing):
        report.error(f"{landing}: is {package}, contains it or is inside it, and the converter "
                     f"removes its destination before copying the package into it, which would "
                     f"delete the package; specify an --output outside the package, or use "
                     f"--in-place")
        return finish(report, 1)

    patch = Path(options.patch) if options.patch is not None else None
    if patch is not None:
        if in_place:
            report.error("--patch compares the package with the converted copy, and --in-place "
                         "rewrites the package itself, which leaves nothing to compare; specify "
                         "an --output so that both forms exist")
            return finish(report, 1)
        for other, what in ((package, "the package"), (landing, "the converted copy")):
            if authority.overlapping(other, patch):
                report.error(f"{patch}: is {what}, contains it or is inside it, and the patch "
                             f"is written file by file into this directory; specify a directory "
                             f"outside both")
                return finish(report, 1)
        if patch.exists() and not patch.is_dir():
            report.error(f"{patch}: is not a directory")
            return finish(report, 1)
        if patch.is_dir() and any(patch.iterdir()):
            report.error(f"{patch}: already contains files, and a patch written over an earlier "
                         f"patch would contain both; remove the directory first")
            return finish(report, 1)

    desc = read_23_package(package, authority, report)
    if desc is None:
        return finish(report, 1)

    if run_authority(authority, authority_path, arguments) != 0:
        report.error(f"{landing}: the converter reported errors; the result is not passed on")
        return finish(report, 1, after_authority=True)

    if options.skip_verification:
        report.note(f"{landing}: converted; verification skipped as requested")
        if patch is not None:
            write_patch(package, landing, patch, desc, authority, report)
        return finish(report, 1 if report.errors else 0, after_authority=True)

    verify(desc, landing, guessed, authority, report)
    if patch is not None:
        if report.errors:
            report.note(f"{patch}: no patch written, because the converted package did not "
                        f"pass verification")
        else:
            write_patch(package, landing, patch, desc, authority, report)
    return finish(report, 1 if report.errors else 0, after_authority=True)


if __name__ == "__main__":
    sys.exit(main())
