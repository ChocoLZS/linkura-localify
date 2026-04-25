from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import sql_utils

ROOT = Path(__file__).resolve().parents[4]
CHARACTER_INFO_JSON = ROOT / "LinkuraLocalify" / "http-mock" / "backend" / "tools" / "data" / "get_character_info.json"


def _load() -> tuple[list[str], dict[str, list[object]], dict[str, dict]]:
    """Returns (field_order, field_values, records) where records maps character_id_str -> collection_character_info dict."""
    data = json.loads(CHARACTER_INFO_JSON.read_text(encoding="utf-8"))

    field_order: list[str] = []
    field_values: dict[str, list[object]] = {}
    records: dict[str, dict] = {}

    for char_id_str, entry in data.items():
        info: dict = entry["collection_character_info"]
        if char_id_str in records:
            raise RuntimeError(f"duplicate character_id: {char_id_str}")
        records[char_id_str] = info
        for key, value in info.items():
            if key == "character_id":
                continue
            if key not in field_values:
                field_order.append(key)
                field_values[key] = []
            field_values[key].append(value)

    return field_order, field_values, records


def generate_schema_ddl() -> str:
    field_order, field_values, _ = _load()
    column_defs = [
        "character_id TEXT NOT NULL PRIMARY KEY",
        "response_json TEXT NOT NULL",
    ]
    for field in field_order:
        sql_type, default_value = sql_utils.infer_sql_type(field_values[field])
        column_defs.append(f"{field} {sql_type} NOT NULL DEFAULT {default_value}")

    lines = [
        "CREATE TABLE IF NOT EXISTS character_info (",
        "    " + ",\n    ".join(column_defs),
        ");",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    field_order, field_values, records = _load()
    python_defaults: dict[str, object] = {}
    for field in field_order:
        sql_type, _ = sql_utils.infer_sql_type(field_values[field])
        python_defaults[field] = sql_utils.default_python_value(sql_type)

    insert_columns = ["character_id", "response_json", *field_order]
    statements: list[str] = []
    for char_id_str, info in records.items():
        response_json = json.dumps({"collection_character_info": info}, ensure_ascii=False, separators=(",", ":"))
        values = [char_id_str, response_json, *(info.get(field, python_defaults[field]) for field in field_order)]
        encoded_values = ", ".join(sql_utils.encode_sql_value(v) for v in values)
        statements.append(
            "INSERT OR REPLACE INTO character_info ("
            + ", ".join(insert_columns)
            + f") VALUES ({encoded_values});"
        )
    return statements


if __name__ == "__main__":
    import generate_all
    generate_all.main()
