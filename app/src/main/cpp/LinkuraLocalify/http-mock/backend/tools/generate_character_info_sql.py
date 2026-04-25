from __future__ import annotations

import json
import sys
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


def _load_yaml(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return yaml.load(f, Loader=YAML_LOADER) or []


def _build_character_data() -> dict[str, dict]:
    characters = {row["Id"]: row for row in _load_yaml(YAML_DIR / "Characters.yaml")}
    card_datas = _load_yaml(YAML_DIR / "CardDatas.yaml")

    movies_by_series: dict[int, list] = {}
    for movie in _load_yaml(YAML_DIR / "StyleMovies.yaml"):
        movies_by_series.setdefault(movie["CardSeriesId"], []).append(movie)

    voices_by_series: dict[int, list] = {}
    for voice in _load_yaml(YAML_DIR / "StyleVoices.yaml"):
        voices_by_series.setdefault(voice["CardSeriesId"], []).append(voice)

    char_series_max: dict[int, dict[int, dict]] = {}
    for card in card_datas:
        char_id = card["CharactersId"]
        series_id = card["CardSeriesId"]
        bucket = char_series_max.setdefault(char_id, {})
        if series_id not in bucket or card["Id"] > bucket[series_id]["Id"]:
            bucket[series_id] = card

    result: dict[str, dict] = {}
    for char_id, char_data in characters.items():
        if char_id not in char_series_max:
            continue

        card_list = []
        for series_id, card in sorted(char_series_max[char_id].items()):
            evolve_times = card["EvolveTimes"]

            voice_list = [
                {"voices_id": v["Id"], "priority": v["Priority"], "is_opened": True}
                for v in sorted(voices_by_series.get(series_id, []), key=lambda v: v["Priority"])
            ]

            movie_list = [
                {
                    "movies_id": m["Id"],
                    "priority": 1,
                    "is_opened": (m["MovieType"] - 1) <= evolve_times,
                }
                for m in sorted(movies_by_series.get(series_id, []), key=lambda m: m["MovieType"])
            ]

            card_list.append({
                "card_datas_id": card["Id"],
                "voice_list": voice_list,
                "movie_list": movie_list,
            })

        result[str(char_id)] = {
            "collection_character_info": {
                "character_id": char_id,
                "name_last": char_data.get("NameLast", ""),
                "name_first": char_data.get("NameFirst", ""),
                "latin_alphabet_name_last": char_data.get("LatinAlphabetNameLast", ""),
                "latin_alphabet_name_first": char_data.get("LatinAlphabetNameFirst", ""),
                "character_voice": char_data.get("CharacterVoice", ""),
                "theme_color": char_data.get("ThemeColor", ""),
                "card_list": card_list,
            }
        }

    merge_map = {1020: 1021, 1030: 1031, 1044: 1051, 1040: 1041, 1050: 1051}
    for src_id, dst_id in merge_map.items():
        src_key, dst_key = str(src_id), str(dst_id)
        if src_key not in result:
            continue
        if dst_key in result:
            result[dst_key]["collection_character_info"]["card_list"].extend(
                result[src_key]["collection_character_info"]["card_list"]
            )
        del result[src_key]

    return result


def _load() -> tuple[list[str], dict[str, list[object]], dict[str, dict]]:
    data = _build_character_data()

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


def generate_json() -> None:
    data = _build_character_data()
    output_path = DATA_DIR / "get_character_info.json"
    output_path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"  Written {len(data)} characters -> {output_path}")


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
