from __future__ import annotations

import json


def sql_quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def infer_sql_type(values: list[object]) -> tuple[str, str]:
    has_list_or_dict = any(isinstance(v, (list, dict)) for v in values)
    if has_list_or_dict:
        return "TEXT", "'[]'"

    has_str = any(isinstance(v, str) for v in values)
    if has_str:
        return "TEXT", "''"

    has_bool = any(isinstance(v, bool) for v in values)
    if has_bool:
        return "INTEGER", "0"

    has_float = any(isinstance(v, float) for v in values)
    if has_float:
        return "REAL", "0"

    return "INTEGER", "0"


def default_python_value(sql_type: str) -> object:
    if sql_type == "TEXT":
        return ""
    if sql_type == "REAL":
        return 0.0
    return 0


def encode_sql_value(value: object) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return repr(value)
    if isinstance(value, str):
        return sql_quote(value)
    return sql_quote(json.dumps(value, ensure_ascii=False, separators=(",", ":")))
