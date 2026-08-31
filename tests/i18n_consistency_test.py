#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCALES = {
    "en-US": ROOT / "resources/i18n/en-US/main.json",
    "fr": ROOT / "resources/i18n/fr/main.json",
}
SCAN_ROOTS = [ROOT / "app", ROOT / "resources/xml"]
TEXT_SUFFIXES = {".cpp", ".hpp", ".h", ".xml"}


def flatten(value, prefix=""):
    result = {}
    if isinstance(value, dict):
        for key, child in value.items():
            child_prefix = f"{prefix}/{key}" if prefix else key
            result.update(flatten(child, child_prefix))
    else:
        result[prefix] = value
    return result


def referenced_artemis_keys():
    keys = set()
    cpp_pattern = re.compile(r'"(artemis/[A-Za-z0-9_./-]+)"_i18n')
    # Some keys are returned as plain strings and resolved later via
    # brls::getStr (e.g. wireguard_problem_i18n_key), so they never appear with
    # the _i18n literal. Catch those too or they can rot unnoticed.
    cpp_runtime_pattern = re.compile(r'return "(artemis/[A-Za-z0-9_./-]+)";')
    xml_pattern = re.compile(r"@i18n/(artemis/[A-Za-z0-9_./-]+)")

    for root in SCAN_ROOTS:
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix not in TEXT_SUFFIXES:
                continue
            text = path.read_text(encoding="utf-8", errors="strict")
            keys.update(cpp_pattern.findall(text))
            keys.update(cpp_runtime_pattern.findall(text))
            keys.update(xml_pattern.findall(text))
    return keys


def no_duplicate_keys(pairs):
    """json.loads silently keeps the last of duplicated keys, so a copy-pasted
    entry can shadow a real translation without any test noticing."""
    seen = set()
    for key, _ in pairs:
        assert key not in seen, f"duplicate key: {key}"
        seen.add(key)
    return dict(pairs)


def main():
    locale_keys = {}
    for locale, path in LOCALES.items():
        try:
            data = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=no_duplicate_keys)
        except AssertionError as exc:
            raise AssertionError(f"{locale}: {exc}") from None
        flattened = flatten(data)
        locale_keys[locale] = {key for key in flattened if key.startswith("artemis/")}
        empty = [key for key in locale_keys[locale] if flattened[key] == ""]
        assert not empty, f"{locale} has empty Artemis translations: {empty}"

    assert locale_keys["en-US"] == locale_keys["fr"], (
        "English/French Artemis key sets differ:\n"
        f"Only English: {sorted(locale_keys['en-US'] - locale_keys['fr'])}\n"
        f"Only French: {sorted(locale_keys['fr'] - locale_keys['en-US'])}"
    )

    referenced = referenced_artemis_keys()
    missing = referenced - locale_keys["en-US"]
    assert not missing, f"Artemis UI references missing translations: {sorted(missing)}"

    print(
        f"Artemis i18n OK: {len(referenced)} referenced keys, "
        f"{len(locale_keys['en-US'])} translated keys in English and French"
    )


if __name__ == "__main__":
    main()
