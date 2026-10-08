#!/usr/bin/env python3
"""Converts a phoneme-symbol YAML source into the JSON inventory that a voicebank declares.

This is not part of the 2.3 to 2.4 conversion (`convert-voicebank.py` beside this file is): it is a
one-shot helper for the upstream phoneme-symbol list, which is written as YAML while a voicebank
declares `phonemeTypes` as JSON. English is the case it was written for, so `--prefix` selects the
language whose symbols are kept: an entry is kept when its symbol starts with `<prefix>/`, and the
markers `AP` and `SP` are kept for every language and typed as vowels.

The output adds the fixed syllabification rules, which the source does not carry. As in the
reference converter, an existing `--output` is refused unless `--force` is given: the conversion is
cheap and repeatable, so overwriting a file that a caller may already use is never the only option.

    python3 scripts/convert_phoneme_symbols.py <symbols.yaml> -o <inventory.json> [--prefix en]
"""

import argparse
import json
import sys
from pathlib import Path

try:
    import yaml
except ImportError:  # reported by main(), so that --help and the tests still work without it
    yaml = None

DEFAULT_RULES = [
    {"pattern": ["vowel"], "onsets": [0]},
    {"pattern": ["consonant", "liquid", "vowel"], "onsets": [1]},
    {"pattern": ["liquid", "liquid", "vowel"], "onsets": [1]},
]

TYPE_MAP = {
    "vowel": "vowel",
    "liquid": "liquid",
    "fricative": "consonant",
}

MARKERS = ("AP", "SP")


def convert(input_path: Path, output_path: Path, prefix: str) -> int:
    """Writes the inventory of *prefix* to *output_path* and returns the number of phonemes.

    Raises `ValueError` when the source does not have the shape this script reads, and lets
    `yaml.YAMLError` through: the caller reports both instead of a traceback.
    """
    with open(input_path, "r", encoding="utf-8") as stream:
        data = yaml.safe_load(stream)
    if not isinstance(data, dict):
        raise ValueError("the source must be a map with a `symbols` list")

    prefix_with_slash = prefix + "/"
    phoneme_types = {}
    for entry in data.get("symbols", []):
        if not isinstance(entry, dict):
            continue
        symbol = entry.get("symbol", "")
        ph_type = entry.get("type", "")
        if not symbol or not ph_type:
            continue
        if symbol in MARKERS:
            phoneme_types[symbol] = "vowel"
            continue
        if not symbol.startswith(prefix_with_slash):
            continue
        name = symbol[len(prefix_with_slash):]
        phoneme_types[name] = TYPE_MAP.get(ph_type, "consonant")

    result = {"phonemeTypes": phoneme_types, "rules": DEFAULT_RULES}
    with open(output_path, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    return len(phoneme_types)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="Convert phoneme symbols YAML to JSON")
    parser.add_argument("input", type=Path, help="Input YAML file")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output JSON file")
    parser.add_argument("--force", action="store_true",
                        help="replace the output file if it already exists")
    parser.add_argument("--prefix", default="en",
                        help="Language prefix to filter (default: en)")
    options = parser.parse_args(argv)

    if yaml is None:
        print("error: PyYAML is required. Install with: pip install pyyaml", file=sys.stderr)
        return 1
    if not options.input.is_file():
        print(f"error: {options.input}: no such file", file=sys.stderr)
        return 1
    if options.output.exists() and not options.force:
        print(f"error: {options.output}: exists, and this script writes the whole file; remove it, "
              f"choose another --output, or pass --force to replace it", file=sys.stderr)
        return 1
    if not options.output.parent.is_dir():
        print(f"error: {options.output.parent}: no such directory", file=sys.stderr)
        return 1

    try:
        count = convert(options.input, options.output, options.prefix)
    except yaml.YAMLError as problem:
        print(f"error: {options.input}: {problem}", file=sys.stderr)
        return 1
    except ValueError as problem:
        print(f"error: {options.input}: {problem}", file=sys.stderr)
        return 1

    print(f"Converted {count} phonemes to {options.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
