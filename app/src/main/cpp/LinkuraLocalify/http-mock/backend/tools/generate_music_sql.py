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


def _load_yaml(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return yaml.load(f, Loader=YAML_LOADER) or []


def generate_schema_ddl() -> str:
    lines = [
        "CREATE TABLE IF NOT EXISTS music (",
        "    music_id INTEGER NOT NULL PRIMARY KEY,",
        "    generations_id INTEGER NOT NULL DEFAULT 0",
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_music_generations_id ON music(generations_id);",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    musics = _load_yaml(YAML_DIR / "Musics.yaml")
    scores = _load_yaml(YAML_DIR / "MusicScores.yaml")
    score_ids = {s["Id"] for s in scores}

    statements: list[str] = []
    for m in musics:
        music_id = m["Id"]
        if music_id not in score_ids:
            continue
        gen_id = m["GenerationsId"]
        statements.append(
            f"INSERT OR IGNORE INTO music (music_id, generations_id) "
            f"VALUES ({music_id}, {gen_id});"
        )

    return statements


if __name__ == "__main__":
    stmts = generate_seed_statements()
    print(f"Generated {len(stmts)} INSERT statements")
    if stmts:
        print(stmts[0])
