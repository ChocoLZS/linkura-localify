from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import sql_utils

ROOT = Path(__file__).resolve().parents[4]
ARCHIVE_JSON = ROOT / "LinkuraLocalify" / "http-mock" / "backend" / "tools" / "data" / "archive_archive_details.json"


def _load() -> tuple[list[str], dict[str, list[object]]]:
    data = json.loads(ARCHIVE_JSON.read_text(encoding="utf-8"))
    field_order: list[str] = []
    field_values: dict[str, list[object]] = {}
    for archive_id, detail in data.items():
        if "archives_id" in detail:
            raise RuntimeError(f"record {archive_id} unexpectedly contains archives_id field")
        for key, value in detail.items():
            if key not in field_values:
                field_order.append(key)
                field_values[key] = []
            field_values[key].append(value)
    return field_order, field_values


def generate_schema_ddl() -> str:
    field_order, field_values = _load()
    column_defs = [
        "archives_id TEXT NOT NULL PRIMARY KEY",
        "response_json TEXT NOT NULL",
    ]
    for field in field_order:
        sql_type, default_value = sql_utils.infer_sql_type(field_values[field])
        column_defs.append(f"{field} {sql_type} NOT NULL DEFAULT {default_value}")

    lines = [
        "CREATE TABLE IF NOT EXISTS archive_detail (",
        "    " + ",\n    ".join(column_defs),
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_archive_detail_title ON archive_detail(title);",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    data = json.loads(ARCHIVE_JSON.read_text(encoding="utf-8"))
    field_order, field_values = _load()
    python_defaults: dict[str, object] = {}
    for field in field_order:
        sql_type, _ = sql_utils.infer_sql_type(field_values[field])
        python_defaults[field] = sql_utils.default_python_value(sql_type)

    insert_columns = ["archives_id", "response_json", *field_order]
    statements: list[str] = []
    for archive_id, detail in data.items():
        response_json = json.dumps(detail, ensure_ascii=False, separators=(",", ":"))
        values = [archive_id, response_json, *(detail.get(field, python_defaults[field]) for field in field_order)]
        encoded_values = ", ".join(sql_utils.encode_sql_value(v) for v in values)
        statements.append(
            "INSERT OR REPLACE INTO archive_detail ("
            + ", ".join(insert_columns)
            + f") VALUES ({encoded_values});"
        )
    return statements


if __name__ == "__main__":
    import generate_all
    generate_all.main()
