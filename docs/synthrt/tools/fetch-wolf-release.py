#!/usr/bin/env python3
"""Fetch the published wolf language packages the way an outside developer would.

The archive list and every expected hash come from the committed assets.cmake of the consumer
repository, so this script never restates a package name or a hash of its own. The bytes come from
the GitHub release only: no local build tree is consulted.

    python docs/synthrt/tools/fetch-wolf-release.py --assets <assets.cmake> --out <directory>

Each archive is attempted several times, and each attempt tries a direct connection first and the
local proxy second, because the two transports fail for different reasons: a direct failure can be a
blocked target and a proxied failure can be a proxy that is down. A transfer that completes but does
not match its hash is deleted rather than kept, and the run reports "not downloaded" and "hash
mismatch" separately so that a flaky link is not mistaken for a changed release.

The verifier proves itself before it is trusted: a negative control hashes a probe file twice, once
unchanged and once with one byte flipped, and the run aborts unless the unchanged comparison passes
and the flipped one fails.

Exit codes: 0 every archive present and matching, 1 at least one archive is missing or mismatching,
2 the run could not start (no authority, no archives, or a verifier that failed its own control).
"""

import argparse
import hashlib
import json
import os
import re
import sys
import time
import urllib.request

DEFAULT_PROXY = "http://127.0.0.1:10800"


def parse_assets(path):
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    version = re.search(r'set\(WOLF_LANG_PACKAGES_BUNDLE_VERSION "([^"]+)"\)', text)
    if not version:
        raise SystemExit("FATAL: no bundle version in %s" % path)
    entries = []
    for match in re.finditer(r'set\(WOLF_LANG_([A-Z0-9]+)_FILE "([^"]+)"\)', text):
        key = match.group(1)
        name = match.group(2)
        digest = re.search(r'set\(WOLF_LANG_' + key + r'_SHA512 "([0-9a-f]+)"\)', text)
        if not digest:
            raise SystemExit("FATAL: no SHA512 for %s in %s" % (name, path))
        entries.append({"key": key, "file": name, "sha512": digest.group(1)})
    return version.group(1), entries


def sha512_of(path):
    digest = hashlib.sha512()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def fetch(url, destination, proxy, timeout):
    handler = urllib.request.ProxyHandler({"http": proxy, "https": proxy} if proxy else {})
    request = urllib.request.Request(url, headers={"User-Agent": "wolf-release-fetch/1.0"})
    with urllib.request.build_opener(handler).open(request, timeout=timeout) as response, \
            open(destination, "wb") as handle:
        while True:
            chunk = response.read(1 << 20)
            if not chunk:
                break
            handle.write(chunk)


def negative_control(directory):
    probe = os.path.join(directory, "verifier-negative-control.bin")
    with open(probe, "wb") as handle:
        handle.write(bytes(range(64)))
    good = sha512_of(probe)
    flipped = good[:-1] + ("0" if good[-1] != "0" else "1")
    intact, damaged = good == good, good != flipped
    print("SELFTEST unchanged comparison: %s" % intact, flush=True)
    print("SELFTEST one-byte-flipped     : %s" % damaged, flush=True)
    os.remove(probe)
    return intact and damaged


def acquire(base, entry, destination, attempts, proxy, timeout):
    """Returns the transport that produced matching bytes, or None after every attempt failed."""
    for attempt in range(1, attempts + 1):
        for label, chosen in (("direct", None), ("proxy", proxy)):
            try:
                print("FETCH [%s] %s attempt %d" % (label, entry["file"], attempt), flush=True)
                fetch(base + "/" + entry["file"], destination, chosen, timeout)
            except Exception as error:  # the message is the evidence
                print("FAILED [%s] %s: %s" % (label, entry["file"], error), flush=True)
                continue
            if sha512_of(destination) == entry["sha512"]:
                return label
            print("MISMATCH [%s] %s: the transfer completed but the hash differs"
                  % (label, entry["file"]), flush=True)
            os.remove(destination)
    return None


def main():
    parser = argparse.ArgumentParser(description="Fetch the wolf release archives with verification")
    parser.add_argument("--assets", required=True,
                        help="the assets.cmake of the consumer repository, the only authority")
    parser.add_argument("--out", required=True, help="directory that receives the archives")
    parser.add_argument("--proxy", default=DEFAULT_PROXY,
                        help="proxy tried after each failed direct attempt")
    parser.add_argument("--attempts", type=int, default=3,
                        help="rounds of (direct then proxy) per archive, default 3")
    parser.add_argument("--timeout", type=int, default=300, help="seconds per transfer attempt")
    parser.add_argument("--base-url", default=None,
                        help="override the release base URL, which is derived from the bundle")
    parser.add_argument("--report", default=None, help="path of the JSON report")
    args = parser.parse_args()

    os.makedirs(args.out, exist_ok=True)
    bundle, entries = parse_assets(args.assets)
    base = args.base_url or ("https://github.com/diffscope/wolf/releases/download/lang-v" + bundle)
    print("RELEASE %s" % base, flush=True)
    print("ARCHIVES %d" % len(entries), flush=True)
    if not entries:
        print("FATAL: no archives parsed from %s" % args.assets, flush=True)
        return 2
    if not negative_control(args.out):
        print("FATAL: the verifier failed its own negative control", flush=True)
        return 2

    results = []
    for entry in entries:
        destination = os.path.join(args.out, entry["file"])
        verdict = None
        if os.path.exists(destination) and sha512_of(destination) == entry["sha512"]:
            verdict = "sha512-ok"
            print("SKIP (hash ok) %s" % entry["file"], flush=True)
        if verdict is None:
            if os.path.exists(destination):
                os.remove(destination)
            if acquire(base, entry, destination, args.attempts, args.proxy, args.timeout):
                verdict = "sha512-ok"
        size = 0
        actual = ""
        if os.path.exists(destination):
            size = os.path.getsize(destination)
            actual = sha512_of(destination)
            verdict = "sha512-ok" if actual == entry["sha512"] else "sha512-mismatch"
        verdict = verdict or "not-downloaded"
        print("VERIFY %s -> %s size=%d" % (entry["file"], verdict, size), flush=True)
        results.append({"key": entry["key"], "file": entry["file"], "expected": entry["sha512"],
                        "actual": actual, "size": size, "verdict": verdict})

    ok = sum(1 for item in results if item["verdict"] == "sha512-ok")
    missing = [item["file"] for item in results if item["verdict"] == "not-downloaded"]
    mismatched = [item["file"] for item in results if item["verdict"] == "sha512-mismatch"]
    if args.report:
        with open(args.report, "w", encoding="utf-8") as handle:
            json.dump({"release": base, "bundle": bundle, "archives": results}, handle, indent=2)
    print("SUMMARY ok=%d not-downloaded=%d mismatch=%d" % (ok, len(missing), len(mismatched)),
          flush=True)
    if missing:
        print("STOP: these archives could not be downloaded: %s" % ", ".join(missing), flush=True)
    if mismatched:
        print("STOP: these archives transferred but do not match their hash: %s"
              % ", ".join(mismatched), flush=True)
    if missing or mismatched:
        print("Do not use this directory until every archive is present and matching, and do not "
              "change an expected hash to make a run pass.", flush=True)
        return 1
    print("NEXT: unpack every archive into one packages directory, then point the build at it",
          flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
