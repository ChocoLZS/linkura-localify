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

HARDCODED_GRAND_PRIX_ID = 806101


def _load_yaml(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return yaml.load(f, Loader=YAML_LOADER) or []


def generate_schema_ddl() -> str:
    lines = [
        "CREATE TABLE IF NOT EXISTS grand_prix (",
        "    grand_prix_id INTEGER NOT NULL PRIMARY KEY,",
        "    grand_prix_type INTEGER NOT NULL DEFAULT 1,",
        "    start_date TEXT NOT NULL DEFAULT '',",
        "    end_date TEXT NOT NULL DEFAULT '',",
        "    rewards_json TEXT NOT NULL DEFAULT '[]'",
        ");",
        "",
        "CREATE TABLE IF NOT EXISTS grand_prix_series (",
        "    quest_id INTEGER NOT NULL PRIMARY KEY,",
        "    grand_prix_id INTEGER NOT NULL,",
        "    order_id INTEGER NOT NULL DEFAULT 1",
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_gp_series_gp ON grand_prix_series(grand_prix_id);",
        "",
        "CREATE TABLE IF NOT EXISTS grand_prix_stage (",
        "    stage_id INTEGER NOT NULL PRIMARY KEY,",
        "    quest_id INTEGER NOT NULL,",
        "    order_id INTEGER NOT NULL DEFAULT 1,",
        "    quest_musics_type INTEGER NOT NULL DEFAULT 0,",
        "    quest_musics_detail INTEGER NOT NULL DEFAULT 0,",
        "    score_bonus_value INTEGER NOT NULL DEFAULT 10000,",
        "    release_condition_type INTEGER NOT NULL DEFAULT 0,",
        "    release_condition_value INTEGER NOT NULL DEFAULT 0,",
        "    section_skills_json TEXT NOT NULL DEFAULT '[]'",
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_gp_stage_quest ON grand_prix_stage(quest_id);",
        "",
        "CREATE TABLE IF NOT EXISTS grand_prix_progress (",
        "    stage_id INTEGER NOT NULL PRIMARY KEY,",
        "    high_score INTEGER NOT NULL DEFAULT 0,",
        "    daily_best_score INTEGER NOT NULL DEFAULT 0,",
        "    play_count INTEGER NOT NULL DEFAULT 0",
        ");",
        "",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    grand_prix_list = _load_yaml(YAML_DIR / "GrandPrix.yaml")
    series_list = _load_yaml(YAML_DIR / "GrandPrixQuestSeries.yaml")
    stages_list = _load_yaml(YAML_DIR / "GrandPrixQuestStages.yaml")
    release_conds = _load_yaml(YAML_DIR / "GrandPrixReleaseCondition.yaml")
    rewards = _load_yaml(YAML_DIR / "GrandPrixRewards.yaml")
    reward_datas = _load_yaml(YAML_DIR / "GrandPrixRewardDatas.yaml")

    reward_data_map: dict[int, list[dict]] = {}
    for rd in reward_datas:
        rid = rd["GrandPrixRewardsId"]
        if rid not in reward_data_map:
            reward_data_map[rid] = []
        reward_data_map[rid].append({
            "grand_prix_reward_datas_id": rd["Id"],
            "item_type": rd["RewardType"],
            "item_id": rd["RewardItemId"],
            "num": rd["RewardNum"],
        })

    release_map: dict[int, dict] = {}
    for rc in release_conds:
        stage_id = rc["ReleaseGrandPrixId"]
        release_map[stage_id] = {
            "type": rc["ConditionsType"],
            "value": rc["ConditionsValue"],
        }

    sections = _load_yaml(YAML_DIR / "QuestSections.yaml")
    section_map: dict[int, list[dict]] = {}
    for sec in sections:
        sid = sec["QuestStagesId"]
        if sid not in section_map:
            section_map[sid] = []
        section_map[sid].append({
            "section_no": sec["SectionNo"],
            "section_skills_id": sec["SectionSkillsId"],
        })

    statements: list[str] = []

    for gp in grand_prix_list:
        gp_id = gp["Id"]
        if gp_id != HARDCODED_GRAND_PRIX_ID:
            continue
        gp_type = gp["GrandPrixType"]
        start = str(gp["StartTime"]).replace("+00:00", "Z").replace(" ", "T")
        end = str(gp["EndTime"]).replace("+00:00", "Z").replace(" ", "T")

        rw_list = []
        for r in rewards:
            if r["GrandPrixesId"] != gp_id:
                continue
            if r.get("IsDisplay", 1) == 0:
                continue
            rw_entry = {
                "grand_prix_rewards_id": r["Id"],
                "min_target_num": r["MinTargetNum"],
                "max_target_num": r["MaxTargetNum"],
                "reward_datas": reward_data_map.get(r["Id"], []),
            }
            rw_list.append(rw_entry)

        rw_json = json.dumps(rw_list, ensure_ascii=False).replace("'", "''")

        statements.append(
            f"INSERT OR IGNORE INTO grand_prix "
            f"(grand_prix_id, grand_prix_type, start_date, end_date, rewards_json) "
            f"VALUES ({gp_id}, {gp_type}, '{start}', '{end}', '{rw_json}');"
        )

    for sr in series_list:
        if sr["GrandPrixesId"] != HARDCODED_GRAND_PRIX_ID:
            continue
        statements.append(
            f"INSERT OR IGNORE INTO grand_prix_series "
            f"(quest_id, grand_prix_id, order_id) "
            f"VALUES ({sr['Id']}, {sr['GrandPrixesId']}, {sr['OrderId']});"
        )

    for st in stages_list:
        series_id = st["GrandPrixSeriesId"]
        found = any(sr["Id"] == series_id for sr in series_list if sr["GrandPrixesId"] == HARDCODED_GRAND_PRIX_ID)
        if not found:
            continue

        stage_id = st["Id"]
        rc = release_map.get(stage_id, {"type": 0, "value": 0})
        score_bonus = st.get("ScoreBonusValue3", st.get("ScoreBonusValue0", 10000))

        stage_sections = section_map.get(stage_id, [])
        stage_sections.sort(key=lambda x: x["section_no"])
        sec_json = json.dumps(stage_sections, ensure_ascii=False).replace("'", "''")

        statements.append(
            f"INSERT OR IGNORE INTO grand_prix_stage "
            f"(stage_id, quest_id, order_id, quest_musics_type, quest_musics_detail, "
            f"score_bonus_value, release_condition_type, release_condition_value, section_skills_json) "
            f"VALUES ({stage_id}, {series_id}, {st['OrderId']}, "
            f"{st['QuestMusicsType']}, {st['QuestMusicsDetail']}, "
            f"{score_bonus}, {rc['type']}, {rc['value']}, '{sec_json}');"
        )

    return statements


if __name__ == "__main__":
    stmts = generate_seed_statements()
    print(f"Generated {len(stmts)} INSERT statements")
    for s in stmts[:5]:
        print(s[:200])
