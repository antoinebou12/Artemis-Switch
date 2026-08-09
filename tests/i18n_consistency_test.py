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
    xml_pattern = re.compile(r'@i18n/(artemis/[A-Za-z0-9_./-]+)')

    for root in SCAN_ROOTS:
        for path in root.rglob("*"):
            if not path.is_file() or path.suffix not in TEXT_SUFFIXES:
                continue
            text = path.read_text(encoding="utf-8", errors="strict")
            keys.update(cpp_pattern.findall(text))
            keys.update(xml_pattern.findall(text))
    return keys


def main():
    locale_keys = {}
    for locale, path in LOCALES.items():
        data = json.loads(path.read_text(encoding="utf-8"))
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
