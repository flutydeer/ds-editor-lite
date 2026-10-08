#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Compare wolf language-package releases to audit the provider's version discipline.

Why this exists: a voicebank can only rely on version numbers if the provider actually uses them.
This tool makes that auditable instead of anecdotal. See ../wolf-release-history.md for what the
first run of this comparison found.

What it reports, per package present in more than one release:
  * declared ``version`` / ``compatVersion`` from ``desc.json`` (plus the version in the archive
    name, because the two are not always the same),
  * which files were added / removed / changed (by SHA256),
  * flags that matter for a consumer: "content changed but the version did not",
    "compatVersion raised", "declared version differs from the archive name".

Self-verification (run this first, it must report zero differences everywhere):
    python compare-wolf-releases.py --releases wolf-releases.json --cache <dir> --self-check lang-v0.1.2.0

Normal use:
    python compare-wolf-releases.py --releases wolf-releases.json --cache <dir> \
        --tags lang-v0.1.0.0 lang-v0.1.2.0

Without --releases the release list is fetched from the GitHub API (needs network).
Archives are cached per tag and verified against the API's sha256 digest when one is available.
"""

import argparse
import hashlib
import io
import json
import os
import re
import sys
import urllib.request
import zipfile

API = "https://api.github.com/repos/{repo}/releases?per_page=100"
ARCHIVE = re.compile(r"^(?P<name>wolf-.+?)-(?P<version>\d+(?:\.\d+)+)\.zip$")


def fetch_releases(repo):
    url = API.format(repo=repo)
    req = urllib.request.Request(url, headers={"User-Agent": "wolf-release-audit"})
    with urllib.request.urlopen(req, timeout=180) as resp:
        return json.load(io.TextIOWrapper(resp, encoding="utf-8"))


def load_releases(path, repo):
    if path:
        with io.open(path, encoding="utf-8") as handle:
            return json.load(handle)
    return fetch_releases(repo)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        while True:
            chunk = handle.read(1 << 20)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def ensure_archive(cache, tag, asset):
    folder = os.path.join(cache, tag)
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, asset["name"])
    expected = (asset.get("digest") or "").replace("sha256:", "")
    if not os.path.exists(path) or os.path.getsize(path) != asset["size"]:
        req = urllib.request.Request(asset["browser_download_url"], headers={"User-Agent": "wolf-release-audit"})
        with urllib.request.urlopen(req, timeout=600) as resp, open(path, "wb") as handle:
            while True:
                chunk = resp.read(1 << 20)
                if not chunk:
                    break
                handle.write(chunk)
    actual = sha256_file(path)
    return path, actual, expected


def ensure_extract(path, side):
    """Extract into a per-side folder, so comparing a tag with itself still reads two real trees."""
    folder = os.path.join(os.path.dirname(path), side + "_" + os.path.basename(path)[:-4])
    if not os.path.isdir(folder):
        with zipfile.ZipFile(path) as archive:
            archive.extractall(folder)
    return folder


def find_file(root, filename):
    for base, _, files in os.walk(root):
        if filename in files:
            return os.path.join(base, filename)
    return None


def describe(root):
    """Read desc.json if present and hash every file below root."""
    listing = {}
    for base, _, files in os.walk(root):
        for name in files:
            full = os.path.join(base, name)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            listing[rel] = sha256_file(full)
    desc_path = find_file(root, "desc.json")
    desc = {}
    if desc_path:
        with io.open(desc_path, encoding="utf-8") as handle:
            desc = json.load(handle)
    return listing, desc


def main():
    parser = argparse.ArgumentParser(description="Compare wolf language-package releases.")
    parser.add_argument("--releases", help="local releases JSON (skips the network call)")
    parser.add_argument("--repo", default="diffscope/wolf")
    parser.add_argument("--cache", required=True, help="directory holding <tag>/<archive>.zip")
    parser.add_argument("--tags", nargs="*", help="two tags to compare, newest first")
    parser.add_argument("--self-check", metavar="TAG",
                        help="compare TAG with itself, must report zero differences")
    parser.add_argument("--fail-on-violation", action="store_true",
                        help="exit non-zero when content changed without a version change")
    args = parser.parse_args()

    releases = load_releases(args.releases, args.repo)
    if isinstance(releases, dict):
        print("GitHub API error: %s" % releases.get("message"))
        return 2
    available = {rel["tag_name"]: rel for rel in releases}

    tags = args.tags or []
    if args.self_check:
        if args.self_check not in available:
            print("tag not found: %s" % args.self_check)
            return 2
        tags = [args.self_check, args.self_check]
    if len(tags) != 2:
        lang_tags = [t for t in available if t.startswith("lang-v")]
        lang_tags.sort(key=lambda t: available[t].get("published_at") or "", reverse=True)
        if len(lang_tags) < 2:
            print("need at least two lang-v tags, found: %s" % lang_tags)
            return 2
        tags = lang_tags[:2]
        print("using the two newest language releases: %s" % ", ".join(tags))

    labels = list(tags)
    packages = {}
    bad_digest = []
    for side, tag in enumerate(tags):
        marker = "x" if side == 0 else "y"
        for asset in available[tag].get("assets") or []:
            if not asset["name"].endswith(".zip"):
                continue
            path, actual, expected = ensure_archive(args.cache, tag, asset)
            if expected and actual != expected:
                bad_digest.append((tag, asset["name"], actual, expected))
            folder = ensure_extract(path, marker)
            listing, desc = describe(folder)
            key = desc.get("id") or ARCHIVE.match(asset["name"]).group("name")
            packages.setdefault(key, {})[side] = {
                "archive_version": ARCHIVE.match(asset["name"]).group("version"),
                "declared_version": desc.get("version"),
                "compat_version": desc.get("compatVersion"),
                "dependencies": desc.get("dependencies"),
                "files": listing,
            }

    if bad_digest:
        print("SHA256 mismatch against the API digest:")
        for tag, name, actual, expected in bad_digest:
            print("  %s %s: got %s want %s" % (tag, name, actual, expected))
        return 1

    violations = 0
    compared = 0
    print("")
    print("%-24s %-22s %-22s %s" % ("package", labels[0], labels[1], "files changed/added/removed"))
    for key in sorted(packages):
        entry = packages[key]
        if len(entry) < 2:
            only = sorted(entry)[0]
            print("%-24s only in %s (declared %s)" % (key, labels[only], entry[only]["declared_version"]))
            continue
        compared += 1
        old, new = entry[0], entry[1]
        old_files, new_files = old["files"], new["files"]
        changed = sorted(k for k in set(old_files) & set(new_files) if old_files[k] != new_files[k])
        added = sorted(set(new_files) - set(old_files))
        removed = sorted(set(old_files) - set(new_files))
        old_ver = "%s (%s)" % (old["declared_version"], old["archive_version"])
        new_ver = "%s (%s)" % (new["declared_version"], new["archive_version"])
        print("%-24s %-22s %-22s %d/%d/%d" % (key, old_ver, new_ver, len(changed), len(added), len(removed)))
        flags = []
        if changed or added or removed:
            if old["declared_version"] == new["declared_version"]:
                flags.append("CONTENT CHANGED WITH NO VERSION CHANGE")
                violations += 1
        if old["compat_version"] != new["compat_version"]:
            flags.append("compatVersion %s -> %s" % (old["compat_version"], new["compat_version"]))
        if (old["declared_version"] != old["archive_version"]
                or new["declared_version"] != new["archive_version"]):
            flags.append("declared version differs from the archive name")
        if old["dependencies"] != new["dependencies"]:
            flags.append("dependencies: %s -> %s" % (json.dumps(old["dependencies"], ensure_ascii=False),
                                                     json.dumps(new["dependencies"], ensure_ascii=False)))
        for flag in flags:
            print("%-24s   ! %s" % ("", flag))
        for label, names in (("changed", changed), ("added", added), ("removed", removed)):
            if names:
                print("%-24s   %s: %s" % ("", label, ", ".join(names[:6]) + (" ..." if len(names) > 6 else "")))

    if compared == 0:
        print("\nno package was compared, refusing to report a result")
        return 1
    if args.self_check:
        if violations:
            print("\nself-check FAILED: comparing %s with itself reported differences" % args.self_check)
            return 1
        print("\nself-check passed: %s compared with itself, %d package(s) compared, no differences"
              % (args.self_check, compared))
    if violations and args.fail_on_violation:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
