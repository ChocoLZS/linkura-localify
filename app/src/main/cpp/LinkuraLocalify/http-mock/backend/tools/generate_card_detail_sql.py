from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import sql_utils

ROOT = Path(__file__).resolve().parents[4]
CARD_LIST_JSON = ROOT / "LinkuraLocalify" / "http-mock" / "backend" / "tools" / "data" / "user_card_get_list.json"


def _fill_character_bonus(card: dict) -> dict:
    """Return card with character_bonus populated if it is empty."""
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


def _load() -> tuple[list[str], dict[str, list[object]], dict[str, dict]]:
    """Returns (field_order, field_values, records) where records maps d_card_datas_id -> card dict."""
    raw = json.loads(CARD_LIST_JSON.read_text(encoding="utf-8"))
    card_list: list[dict] = raw["user_card_data_list"]

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
