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
    series_min: dict[int, int] = {}
    for card in _load_yaml(YAML_DIR / "CardDatas.yaml"):
        series_id = card["CardSeriesId"]
        card_id = card["Id"]
        if series_id not in series_min or card_id < series_min[series_id]:
            series_min[series_id] = card_id
    user_card_data_list = sorted(series_min.values())

    sticker_ids = sorted(row["Id"] for row in _load_yaml(YAML_DIR / "Stickers.yaml"))

    output = {
        "user_card_data_list": user_card_data_list,
        "sticker_info_list": sticker_ids,
    }

    output_path = BUILTIN_DIR / "get_custom_setting.json"
    output_path.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"  Written {len(user_card_data_list)} cards, {len(sticker_ids)} stickers -> {output_path}")


if __name__ == "__main__":
    generate_json()
