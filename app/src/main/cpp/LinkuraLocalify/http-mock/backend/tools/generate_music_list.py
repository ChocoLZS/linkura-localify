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
    musics = _load_yaml(YAML_DIR / "Musics.yaml")

    music_info_list = [
        {
            "musics_id": m["Id"],
            "is_mastery": True,
            "play_count": 0,
            "latest_play_time": "0001-01-01T00:00:00Z",
        }
        for m in sorted(musics, key=lambda m: m["Id"])
    ]

    output = {"music_info_list": music_info_list}
    output_path = BUILTIN_DIR / "collection_get_music_list.json"
    output_path.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"  Written {len(music_info_list)} musics -> {output_path}")


if __name__ == "__main__":
    generate_json()
