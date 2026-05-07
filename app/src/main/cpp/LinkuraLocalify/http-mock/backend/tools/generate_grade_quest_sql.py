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
        "CREATE TABLE IF NOT EXISTS grade_quest_season (",
        "    season_id INTEGER NOT NULL PRIMARY KEY,",
        "    generation INTEGER NOT NULL DEFAULT 0,",
        "    season INTEGER NOT NULL DEFAULT 0,",
        "    order_id INTEGER NOT NULL DEFAULT 0",
        ");",
        "",
        "CREATE TABLE IF NOT EXISTS grade_quest_series (",
        "    series_id INTEGER NOT NULL PRIMARY KEY,",
        "    season_id INTEGER NOT NULL,",
        "    order_id INTEGER NOT NULL DEFAULT 0,",
        "    default_action_point INTEGER NOT NULL DEFAULT 15,",
        "    squares_json TEXT NOT NULL DEFAULT '[]',",
        "    rewards_json TEXT NOT NULL DEFAULT '[]'",
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_grade_quest_series_season ON grade_quest_series(season_id);",
        "",
        "CREATE TABLE IF NOT EXISTS grade_quest_stage (",
        "    stage_id INTEGER NOT NULL PRIMARY KEY,",
        "    quest_musics_type INTEGER NOT NULL DEFAULT 2,",
        "    quest_musics_detail INTEGER NOT NULL DEFAULT 0,",
        "    live_point INTEGER NOT NULL DEFAULT 0,",
        "    section_skills_json TEXT NOT NULL DEFAULT '[]'",
        ");",
        "",
        "CREATE TABLE IF NOT EXISTS grade_add_skill (",
        "    skill_id INTEGER NOT NULL PRIMARY KEY,",
        "    tier INTEGER NOT NULL DEFAULT 1",
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_grade_add_skill_tier ON grade_add_skill(tier);",
        "",
        "CREATE TABLE IF NOT EXISTS grade_quest_progress (",
        "    series_id INTEGER NOT NULL PRIMARY KEY,",
        "    clear_status INTEGER NOT NULL DEFAULT 0,",
        "    bonus_cleared INTEGER NOT NULL DEFAULT 0",
        ");",
        "",
    ]
    return "\n".join(lines)


TIER_1_SKILLS = [
    41003005, 41004050, 41004100, 41006001,
    41008001, 41008002, 41008003, 41009005, 41015001,
]
TIER_2_SKILLS = [
    41003010, 41003015, 41004200, 41006002,
    41008004, 41008005, 41008006, 41009010, 41009015,
    41014001, 41015002,
]
TIER_3_SKILLS = [
    41003025, 41004300, 41008007, 41008008, 41008009, 41008010,
    41009025, 41014211, 41014221, 41014231, 41014311, 41014321,
    41014331, 41014411, 41014421, 41014431, 41014511, 41014521,
    41041610, 41041611, 41041612, 41041620, 41041621, 41041622,
]


def generate_seed_statements() -> list[str]:
    seasons = _load_yaml(YAML_DIR / "GradeQuestSeason.yaml")
    series_list = _load_yaml(YAML_DIR / "GradeQuestSeries.yaml")
    squares = _load_yaml(YAML_DIR / "GradeQuestSquare.yaml")
    square_datas = _load_yaml(YAML_DIR / "GradeQuestSquareDatas.yaml")
    stages = _load_yaml(YAML_DIR / "GradeQuestStages.yaml")
    rewards = _load_yaml(YAML_DIR / "GradeQuestRewards.yaml")
    sections = _load_yaml(YAML_DIR / "QuestSections.yaml")

    square_data_map = {d["Id"]: d for d in square_datas}

    section_map: dict[int, list[dict]] = {}
    for sec in sections:
        sid = sec["QuestStagesId"]
        if sid not in section_map:
            section_map[sid] = []
        section_map[sid].append({
            "section_no": sec["SectionNo"],
            "section_skills_id": sec["SectionSkillsId"],
        })

    rewards_by_series: dict[int, list[dict]] = {}
    for r in rewards:
        sid = r["GradeQuestSeriesId"]
        if sid not in rewards_by_series:
            rewards_by_series[sid] = []
        rewards_by_series[sid].append({
            "grade_quest_rewards_id": r["Id"],
        })

    squares_by_series: dict[int, list[dict]] = {}
    for sq in squares:
        sid = sq["GradeQuestSeriesId"]
        if sid not in squares_by_series:
            squares_by_series[sid] = []
        sd = square_data_map.get(sq["SquareId"], {})
        open_ids_raw = str(sq.get("OpenGradeQuestSquareIds", "")).strip()
        open_ids = [int(x) for x in open_ids_raw.split(",") if x.strip().isdigit()]
        squares_by_series[sid].append({
            "grade_quest_square_id": sq["Id"],
            "square_type": sd.get("SquareType", 0),
            "target_id": sd.get("TargetId", 0),
            "min_action_point": sd.get("MinActionPoint", 0),
            "max_action_point": sd.get("MaxActionPoint", 0),
            "open_square_ids": open_ids,
        })

    statements: list[str] = []

    for s in seasons:
        statements.append(
            f"INSERT OR IGNORE INTO grade_quest_season "
            f"(season_id, generation, season, order_id) "
            f"VALUES ({s['Id']}, {s['Generation']}, {s['Season']}, {s['OrderId']});"
        )

    for sr in series_list:
        series_id = sr["Id"]
        season_id = sr["GradeQuestSeasonId"]
        order_id = sr["OrderId"]
        action_point = sr["DefaultActionPoint"]

        sq_list = squares_by_series.get(series_id, [])
        sq_json = json.dumps(sq_list, ensure_ascii=False).replace("'", "''")

        rw_list = rewards_by_series.get(series_id, [])
        rw_json = json.dumps(rw_list, ensure_ascii=False).replace("'", "''")

        statements.append(
            f"INSERT OR IGNORE INTO grade_quest_series "
            f"(series_id, season_id, order_id, default_action_point, squares_json, rewards_json) "
            f"VALUES ({series_id}, {season_id}, {order_id}, {action_point}, '{sq_json}', '{rw_json}');"
        )

    for st in stages:
        stage_id = st["Id"]
        musics_type = st["QuestMusicsType"]
        musics_detail = st["QuestMusicsDetail"]
        live_point = st["LivePoint"]

        stage_sections = section_map.get(stage_id, [])
        stage_sections.sort(key=lambda x: x["section_no"])
        sec_json = json.dumps(stage_sections, ensure_ascii=False).replace("'", "''")

        statements.append(
            f"INSERT OR IGNORE INTO grade_quest_stage "
            f"(stage_id, quest_musics_type, quest_musics_detail, live_point, section_skills_json) "
            f"VALUES ({stage_id}, {musics_type}, {musics_detail}, {live_point}, '{sec_json}');"
        )

    for skill_id in TIER_1_SKILLS:
        statements.append(
            f"INSERT OR IGNORE INTO grade_add_skill (skill_id, tier) VALUES ({skill_id}, 1);"
        )
    for skill_id in TIER_2_SKILLS:
        statements.append(
            f"INSERT OR IGNORE INTO grade_add_skill (skill_id, tier) VALUES ({skill_id}, 2);"
        )
    for skill_id in TIER_3_SKILLS:
        statements.append(
            f"INSERT OR IGNORE INTO grade_add_skill (skill_id, tier) VALUES ({skill_id}, 3);"
        )

    return statements


if __name__ == "__main__":
    stmts = generate_seed_statements()
    print(f"Generated {len(stmts)} INSERT statements")
    if stmts:
        print(stmts[0])
