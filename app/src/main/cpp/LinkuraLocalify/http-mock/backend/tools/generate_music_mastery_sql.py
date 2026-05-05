from __future__ import annotations

from pathlib import Path

import yaml

try:
    YAML_LOADER = yaml.CSafeLoader
except AttributeError:
    YAML_LOADER = yaml.SafeLoader

TOOLS_DIR = Path(__file__).resolve().parent
YAML_DIR = TOOLS_DIR / "link-like-diff"


def _load_yaml(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return yaml.load(f, Loader=YAML_LOADER) or []


def generate_schema_ddl() -> str:
    lines = [
        "CREATE TABLE IF NOT EXISTS music_mastery (",
        "    music_id INTEGER NOT NULL PRIMARY KEY,",
        "    music_exp_level INTEGER NOT NULL DEFAULT 50,",
        "    earned_music_exp INTEGER NOT NULL DEFAULT 250300,",
        "    is_mastery INTEGER NOT NULL DEFAULT 1",
        ");",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    musics = _load_yaml(YAML_DIR / "Musics.yaml")
    scores = _load_yaml(YAML_DIR / "MusicScores.yaml")
    score_ids = {s["Id"] for s in scores}

    levels = _load_yaml(YAML_DIR / "MusicLevels.yaml")
    max_level = max(l["Level"] for l in levels)
    max_exp = max(l["CumulativeExperience"] for l in levels if l["Level"] == max_level)

    statements: list[str] = []
    for m in musics:
        music_id = m["Id"]
        if music_id not in score_ids:
            continue
        statements.append(
            f"INSERT OR IGNORE INTO music_mastery (music_id, music_exp_level, earned_music_exp, is_mastery) "
            f"VALUES ({music_id}, {max_level}, {max_exp}, 1);"
        )

    return statements


if __name__ == "__main__":
    stmts = generate_seed_statements()
    print(f"Generated {len(stmts)} INSERT statements (level={50}, exp=250300)")
    if stmts:
        print(stmts[0])
