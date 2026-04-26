from __future__ import annotations

import json
import sys
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import sql_utils

import yaml

try:
    YAML_LOADER = yaml.CSafeLoader
except AttributeError:
    YAML_LOADER = yaml.SafeLoader

TOOLS_DIR = Path(__file__).resolve().parent
YAML_DIR = TOOLS_DIR / "link-like-diff"
DATA_DIR = TOOLS_DIR / "data"
BUILTIN_DIR = TOOLS_DIR.parent.parent / "builtin"


def _load_yaml(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return yaml.load(f, Loader=YAML_LOADER) or []


def _sequential_uuid(index: int) -> str:
    return str(uuid.UUID(int=index))


def _build_skill_list(card: dict, skill_level_max_by_series: dict[int, int]) -> list[dict]:
    skill_defs = [
        (1, card.get("SpecialAppealSeriesId", 0)),
        (2, card.get("SkillSeriesId", 0)),
        (3, card.get("AttributeId", 0)),
    ]
    skill_list = []
    for skill_type, series_id in skill_defs:
        if not series_id:
            continue
        max_skill_level = skill_level_max_by_series.get(series_id, 1)
        skill_list.append({
            "skill_type": skill_type,
            "card_skill_series_id": series_id,
            "skill_level": max_skill_level,
            "max_skill_level": max_skill_level,
        })
    return skill_list


def _build_rhythm_game_skill_list(
    card: dict,
    center_skill_max: dict[int, int],
    rhythm_skill_max: dict[int, int],
) -> list[dict]:
    result: list[dict] = []
    center_id = card.get("CenterSkillSeriesId", 0)
    if center_id and center_id in center_skill_max:
        result.append({"rhythm_game_skill_type": 1, "skill_level": center_skill_max[center_id]})
    rg_id = card.get("RhythmGameSkillSeriesId", 0)
    if rg_id and rg_id in rhythm_skill_max:
        result.append({"rhythm_game_skill_type": 2, "skill_level": rhythm_skill_max[rg_id]})
    return result


def _fill_character_bonus(card: dict) -> dict:
    bonus = card.get("character_bonus")
    if bonus:
        return card
    card = dict(card)
    card["character_bonus"] = {
        "character_id": card["character_id"],
        "music_mastery_bonus": 0,
        "love_correction_value": 0,
        "music_mastery_bonus_list": [],
        "season_fan_level": 100,
    }
    return card


def _build_card_list() -> list[dict]:
    card_datas = _load_yaml(YAML_DIR / "CardDatas.yaml")
    characters = {row["Id"]: row for row in _load_yaml(YAML_DIR / "Characters.yaml")}
    card_rarities = {row["Id"]: row for row in _load_yaml(YAML_DIR / "CardRarities.yaml")}

    limit_break_max_by_series: dict[int, int] = {}
    global_limit_break_max = 0
    for row in _load_yaml(YAML_DIR / "CardLimitBreakMaterials.yaml"):
        series_id = row["CardSeriesId"]
        limit_break_times = row["LimitBreakTimes"]
        limit_break_max_by_series[series_id] = max(limit_break_max_by_series.get(series_id, 0), limit_break_times)
        global_limit_break_max = max(global_limit_break_max, limit_break_times)

    skill_level_max_by_series: dict[int, int] = {}
    for row in _load_yaml(YAML_DIR / "CardSkills.yaml"):
        series_id = row["CardSkillSeriesId"]
        skill_level = row["SkillLevel"]
        skill_level_max_by_series[series_id] = max(skill_level_max_by_series.get(series_id, 0), skill_level)

    center_skill_max: dict[int, int] = {}
    for row in _load_yaml(YAML_DIR / "CenterSkills.yaml"):
        series_id = row["CenterSkillSeriesId"]
        center_skill_max[series_id] = max(center_skill_max.get(series_id, 0), row["SkillLevel"])

    rhythm_skill_max: dict[int, int] = {}
    for row in _load_yaml(YAML_DIR / "RhythmGameSkills.yaml"):
        series_id = row["RhythmGameSkillSeriesId"]
        rhythm_skill_max[series_id] = max(rhythm_skill_max.get(series_id, 0), row["SkillLevel"])

    series_to_card: dict[int, dict] = {}
    for card in card_datas:
        series_id = card["CardSeriesId"]
        if series_id not in series_to_card or card["Id"] > series_to_card[series_id]["Id"]:
            series_to_card[series_id] = card
    deduplicated_cards = sorted(series_to_card.values(), key=lambda c: c["Id"])

    user_card_data_list = []
    for index, card in enumerate(deduplicated_cards, start=1):
        character = characters.get(card["CharactersId"], {})
        rarity = card_rarities.get(card["Rarity"], {})
        evolution_key = f"Evolution{card['EvolveTimes']}_MaxLevel"
        max_style_level = rarity.get(evolution_key, 0)
        limit_break_times = limit_break_max_by_series.get(card["CardSeriesId"], global_limit_break_max)

        user_card_data_list.append({
            "d_card_datas_id": _sequential_uuid(index),
            "card_datas_id": card["Id"],
            "card_name": card["Name"],
            "style_level": max_style_level,
            "max_style_level": max_style_level,
            "limit_break_times": limit_break_times,
            "max_limit_break_times": limit_break_times,
            "card_parameters": {
                "smile": card["InitialSmile"],
                "pure": card["InitialPure"],
                "cool": card["InitialCool"],
                "mental": card["InitialMental"],
                "beat_point": card["BeatPoint"],
            },
            "skill_list": _build_skill_list(card, skill_level_max_by_series),
            "character_id": card["CharactersId"],
            "generations_id": character.get("GenerationsId", 0),
            "series_type": character.get("SeriesType", 0),
            "card_sort_order": 0,
            "character_bonus": {},
            "is_evolve_possible": False,
            "is_evolve_max": False,
            "member_fan_level": 100,
            "is_limit_break": True,
            "is_style_level_up": True,
            "rhythm_game_skill_list": _build_rhythm_game_skill_list(card, center_skill_max, rhythm_skill_max),
        })

    return user_card_data_list


def _load() -> tuple[list[str], dict[str, list[object]], dict[str, dict]]:
    card_list = _build_card_list()

    field_order: list[str] = []
    field_values: dict[str, list[object]] = {}
    records: dict[str, dict] = {}

    for card in card_list:
        card = _fill_character_bonus(card)
        key = card["d_card_datas_id"]
        if key in records:
            raise RuntimeError(f"duplicate d_card_datas_id: {key}")
        records[key] = card
        for field, value in card.items():
            if field == "d_card_datas_id":
                continue
            if field not in field_values:
                field_order.append(field)
                field_values[field] = []
            field_values[field].append(value)

    return field_order, field_values, records


def generate_json() -> None:
    card_list = _build_card_list()
    output = {"user_card_data_list": card_list}
    content = json.dumps(output, ensure_ascii=False, indent=2)
    for path in [DATA_DIR / "user_card_get_list.json", BUILTIN_DIR / "user_card_get_list.json"]:
        path.write_text(content, encoding="utf-8")
        print(f"  Written {len(card_list)} cards -> {path}")


def generate_schema_ddl() -> str:
    field_order, field_values, _ = _load()
    column_defs = [
        "d_card_datas_id TEXT NOT NULL PRIMARY KEY",
        "response_json TEXT NOT NULL",
    ]
    for field in field_order:
        sql_type, default_value = sql_utils.infer_sql_type(field_values[field])
        column_defs.append(f"{field} {sql_type} NOT NULL DEFAULT {default_value}")

    lines = [
        "CREATE TABLE IF NOT EXISTS card_detail (",
        "    " + ",\n    ".join(column_defs),
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_card_detail_card_datas_id ON card_detail(card_datas_id);",
        "CREATE INDEX IF NOT EXISTS idx_card_detail_character_id ON card_detail(character_id);",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    field_order, field_values, records = _load()
    python_defaults: dict[str, object] = {}
    for field in field_order:
        sql_type, _ = sql_utils.infer_sql_type(field_values[field])
        python_defaults[field] = sql_utils.default_python_value(sql_type)

    insert_columns = ["d_card_datas_id", "response_json", *field_order]
    statements: list[str] = []
    for d_card_datas_id, card in records.items():
        response_json = json.dumps({"user_card_data": card}, ensure_ascii=False, separators=(",", ":"))
        values = [d_card_datas_id, response_json, *(card.get(field, python_defaults[field]) for field in field_order)]
        encoded_values = ", ".join(sql_utils.encode_sql_value(v) for v in values)
        statements.append(
            "INSERT OR REPLACE INTO card_detail ("
            + ", ".join(insert_columns)
            + f") VALUES ({encoded_values});"
        )
    return statements


if __name__ == "__main__":
    import generate_all
    generate_all.main()
