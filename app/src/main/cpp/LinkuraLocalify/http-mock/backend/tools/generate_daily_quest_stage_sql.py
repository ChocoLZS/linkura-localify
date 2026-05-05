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
        "CREATE TABLE IF NOT EXISTS daily_quest_stage (",
        "    stage_id INTEGER NOT NULL PRIMARY KEY,",
        "    series_id INTEGER NOT NULL DEFAULT 0,",
        "    quest_musics_type INTEGER NOT NULL DEFAULT 0,",
        "    quest_musics_detail INTEGER NOT NULL DEFAULT 0,",
        "    score1 INTEGER NOT NULL DEFAULT 0,",
        "    score2 INTEGER NOT NULL DEFAULT 0,",
        "    score3 INTEGER NOT NULL DEFAULT 0,",
        "    gain_style_point INTEGER NOT NULL DEFAULT 0,",
        "    use_num INTEGER NOT NULL DEFAULT 0,",
        "    section_skills_json TEXT NOT NULL DEFAULT '[]'",
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_daily_quest_stage_series_id ON daily_quest_stage(series_id);",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    stages = _load_yaml(YAML_DIR / "DailyQuestStages.yaml")
    sections = _load_yaml(YAML_DIR / "QuestSections.yaml")

    section_map: dict[int, list[dict]] = {}
    for s in sections:
        sid = s["QuestStagesId"]
        if sid not in section_map:
            section_map[sid] = []
        section_map[sid].append({
            "section_no": s["SectionNo"],
            "section_skills_id": s["SectionSkillsId"],
        })

    statements: list[str] = []
    for stage in stages:
        stage_id = stage["Id"]
        series_id = stage["DailyQuestSeriesId"]
        music_type = stage["QuestMusicsType"]
        music_detail = stage["QuestMusicsDetail"]
        score1 = stage["Score1"]
        score2 = stage["Score2"]
        score3 = stage["Score3"]
        gain_style_point = stage["GainStylePoint"]
        use_num = stage["UseNum"]

        stage_sections = section_map.get(stage_id, [])
        stage_sections.sort(key=lambda x: x["section_no"])
        sections_json = json.dumps(stage_sections, ensure_ascii=False)

        escaped_json = sections_json.replace("'", "''")
        statements.append(
            f"INSERT OR IGNORE INTO daily_quest_stage "
            f"(stage_id, series_id, quest_musics_type, quest_musics_detail, score1, score2, score3, gain_style_point, use_num, section_skills_json) "
            f"VALUES ({stage_id}, {series_id}, {music_type}, {music_detail}, {score1}, {score2}, {score3}, {gain_style_point}, {use_num}, '{escaped_json}');"
        )

    return statements


if __name__ == "__main__":
    stmts = generate_seed_statements()
    print(f"Generated {len(stmts)} INSERT statements")
    if stmts:
        print(stmts[0])
