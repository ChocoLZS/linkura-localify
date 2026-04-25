from __future__ import annotations

import json
from pathlib import Path

import yaml

try:
    YAML_LOADER = yaml.CSafeLoader
except AttributeError:
    YAML_LOADER = yaml.SafeLoader

TOOLS_DIR = Path(__file__).resolve().parent
YAML_DIR = TOOLS_DIR / "link-like-diff"
BUILTIN_DIR = TOOLS_DIR.parent.parent / "builtin"


def _load_yaml(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return yaml.load(f, Loader=YAML_LOADER) or []


def generate_json() -> None:
    stickers = _load_yaml(YAML_DIR / "Stickers.yaml")

    sticker_info_list = [
        {
            "stickers_id": s["Id"],
            "category_type": s["CategoryType"],
            "category_name": s["CategoryName"],
            "name": s["Name"],
            "text": s["Text"],
            "character_id": s.get("CharactersId", 0),
            "is_variant": s.get("IsVariant", 0),
            "season_id": s.get("SeasonId", 0),
            "requirement_text": s.get("RequirementText", ""),
            "requirement_num": s.get("RequirementValue", 0),
            "requirement_progress": s.get("RequirementValue", 0),
            "variant_requirement_text": s.get("EditRequirementText", ""),
            "variant_requirement_num": s.get("EditRequirementValue", 0),
            "variant_requirement_progress": 0,
            "is_owned": True,
            "is_available_variant": False,
            "create_time": "2023-06-16T00:58:57.052672Z",
        }
        for s in sorted(stickers, key=lambda s: s["Id"])
    ]

    output = {"sticker_info_list": sticker_info_list}
    output_path = BUILTIN_DIR / "collection_get_sticker_list.json"
    output_path.write_text(
        json.dumps(output, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(f"  Written {len(sticker_info_list)} stickers -> {output_path}")


if __name__ == "__main__":
    generate_json()
