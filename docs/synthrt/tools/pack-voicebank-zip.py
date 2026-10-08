#!/usr/bin/env python3
"""Pack a 2.4 voicebank directory into a zip and verify the archive against the directory.

The layout is flat: desc.json and the top-level directories sit at the archive root, which is what
the published 2.4 deliverable of this package used. The archives kept beside the voicebanks
(qixuan@2.7.0.0.zip, zhibin@26.7.16.0.zip) carry a wrapper directory, but they are 2.3 sources, so
they are not the convention for a 2.4 deliverable.

    python docs/synthrt/tools/pack-voicebank-zip.py <package directory> <archive path>

Verification compares every member against the directory on disk by content hash, and refuses an
archive whose member names are unsafe (absolute, drive-lettered, containing a backslash or a
dot-dot segment, duplicated, or outside ASCII), or whose member set differs from the directory.

The verifier proves itself before it is trusted: a negative control builds a probe archive, verifies
it clean, then corrupts one stored byte and requires the verification to fail. A run whose negative
control misbehaves aborts with exit code 2 instead of producing a deliverable that nothing checked.
"""

import argparse
import hashlib
import os
import sys
import tempfile
import zlib
import zipfile


def member_name(root, path):
    return os.path.relpath(path, root).replace(os.sep, "/")


def collect(root):
    names = []
    for current, dirs, files in os.walk(root):
        dirs.sort()
        for name in sorted(files):
            names.append(member_name(root, os.path.join(current, name)))
    return names


def hash_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def unsafe(name):
    if name.startswith("/") or name.startswith("\\"):
        return "absolute path"
    if ":" in name.split("/")[0]:
        return "drive-lettered or colon in the first segment"
    if "\\" in name:
        return "backslash separator"
    if any(part in ("", ".", "..") for part in name.split("/")):
        return "empty or dot segment"
    try:
        name.encode("ascii")
    except UnicodeEncodeError:
        return "non-ASCII name"
    return None


def verify(root, archive):
    problems = []
    with zipfile.ZipFile(archive) as zf:
        members = zf.infolist()
        seen = {}
        for info in members:
            if info.is_dir():
                continue
            bad = unsafe(info.filename)
            if bad:
                problems.append("unsafe member name %s (%s)" % (info.filename, bad))
            if info.filename in seen:
                problems.append("duplicate member %s" % info.filename)
            seen[info.filename] = info
        on_disk = collect(root)
        for name in on_disk:
            full = os.path.join(root, name.replace("/", os.sep))
            if name not in seen:
                problems.append("missing from archive: %s" % name)
                continue
            if seen[name].file_size != os.path.getsize(full):
                problems.append("size differs: %s" % name)
                continue
            digest = hashlib.sha256()
            try:
                with zf.open(name) as handle:
                    for chunk in iter(lambda: handle.read(1 << 20), b""):
                        digest.update(chunk)
            except (zipfile.BadZipFile, zlib.error) as error:
                problems.append("member is unreadable (%s): %s" % (error, name))
                continue
            if digest.hexdigest() != hash_file(full):
                problems.append("content differs: %s" % name)
        for name in seen:
            if name not in on_disk:
                problems.append("extra member: %s" % name)
        return members, problems


def negative_control():
    with tempfile.TemporaryDirectory() as tmp:
        # The archive must not live inside the directory that is verified: it is not a member of
        # itself, and a verifier that reported it as missing would fail on a clean probe.
        root = os.path.join(tmp, "root")
        os.makedirs(root)
        payload = os.path.join(root, "payload.bin")
        with open(payload, "wb") as handle:
            handle.write(bytes(range(256)) * 8)
        archive = os.path.join(tmp, "probe.zip")
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_STORED) as zf:
            zf.write(payload, "payload.bin")
        _, clean = verify(root, archive)
        with open(archive, "r+b") as handle:
            raw = handle.read()
            offset = raw.rfind(bytes(range(256))[0:16])
            handle.seek(offset + 4)
            handle.write(b"\xff\xfe\xfd\xfc")
        _, dirty = verify(root, archive)
        print("SELFTEST clean-probe problems : %d" % len(clean), flush=True)
        print("SELFTEST corrupted-probe      : %d problems" % len(dirty), flush=True)
        return len(clean) == 0 and len(dirty) > 0


def main():
    parser = argparse.ArgumentParser(description="Pack and verify a 2.4 voicebank zip")
    parser.add_argument("package", help="the package directory to pack")
    parser.add_argument("archive", help="the zip to create, which must not exist yet")
    args = parser.parse_args()

    root = os.path.abspath(args.package)
    archive = os.path.abspath(args.archive)
    if not os.path.isdir(root):
        print("FATAL: not a directory: %s" % root, flush=True)
        return 2
    if not os.path.isfile(os.path.join(root, "desc.json")):
        print("FATAL: no desc.json in %s, this is not a package directory" % root, flush=True)
        return 2
    if not negative_control():
        print("FATAL: the verifier failed its own negative control", flush=True)
        return 2

    names = collect(root)
    total = sum(os.path.getsize(os.path.join(root, name)) for name in names)
    print("SOURCE files=%d bytes=%d" % (len(names), total), flush=True)
    if os.path.exists(archive):
        print("FATAL: refusing to overwrite %s" % archive, flush=True)
        return 2
    os.makedirs(os.path.dirname(archive), exist_ok=True)
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, allowZip64=True) as zf:
        for name in names:
            zf.write(os.path.join(root, name.replace("/", os.sep)), name)

    members, problems = verify(root, archive)
    print("ARCHIVE %s" % archive, flush=True)
    print("ARCHIVE files=%d dirs=%d bytes=%d" % (
        sum(1 for item in members if not item.is_dir()),
        sum(1 for item in members if item.is_dir()),
        os.path.getsize(archive)), flush=True)
    print("ARCHIVE sha256=%s" % hash_file(archive).upper(), flush=True)
    for problem in problems:
        print("PROBLEM %s" % problem, flush=True)
    print("SUMMARY files=%d problems=%d" % (len(names), len(problems)), flush=True)
    if problems:
        print("STOP: the archive does not match the directory, do not distribute it", flush=True)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
