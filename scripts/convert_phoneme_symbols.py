import argparse
import json
import sys

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required. Install with: pip install pyyaml", file=sys.stderr)
    sys.exit(1)

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


def convert(input_path, output_path, prefix):
    with open(input_path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    prefix_with_slash = prefix + "/"
    phoneme_types = {}
    for entry in data.get("symbols", []):
        symbol = entry.get("symbol", "")
        ph_type = entry.get("type", "")
        if not symbol or not ph_type:
            continue
        if symbol in ("AP", "SP"):
            phoneme_types[symbol] = "vowel"
            continue
        if not symbol.startswith(prefix_with_slash):
            continue
        name = symbol[len(prefix_with_slash):]
        mapped_type = TYPE_MAP.get(ph_type, "consonant")
        phoneme_types[name] = mapped_type

    result = {"phonemeTypes": phoneme_types, "rules": DEFAULT_RULES}

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"Converted {len(phoneme_types)} phonemes to {output_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert phoneme symbols YAML to JSON")
    parser.add_argument("input", help="Input YAML file")
    parser.add_argument("-o", "--output", required=True, help="Output JSON file")
    parser.add_argument(
        "--prefix", default="en", help="Language prefix to filter (default: en)"
    )
    args = parser.parse_args()
    convert(args.input, args.output, args.prefix)
