from __future__ import annotations

import json
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
TEMPLATE_DIR = TOOLS_DIR / "template"
BUILTIN_DIR = TOOLS_DIR.parent.parent / "builtin"


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def generate_json() -> None:
    template = _load_json(TEMPLATE_DIR / "home_get_home.json")
    profile_info = _load_json(TEMPLATE_DIR / "profile_info.json")

    output = dict(template)
    output["profile_info"] = profile_info

    output_path = BUILTIN_DIR / "home_get_home.json"
    output_path.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"  Written -> {output_path}")


if __name__ == "__main__":
    generate_json()
