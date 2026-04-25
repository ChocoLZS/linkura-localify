from __future__ import annotations

import argparse
import json
import sys
import uuid
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import sql_utils

import yaml

try:
    YAML_LOADER = yaml.CSafeLoader
except AttributeError:
    YAML_LOADER = yaml.SafeLoader

TOOLS_DIR = Path(__file__).resolve().parent
HTTP_MOCK_DIR = TOOLS_DIR.parent.parent
ITEMS_YAML = TOOLS_DIR / "link-like-diff" / "Items.yaml"
DEFAULT_JSON_OUTPUT = HTTP_MOCK_DIR / "builtin" / "user_items_get_list.json"


def _load_yaml(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return yaml.load(f, Loader=YAML_LOADER) or []


def _sequential_uuid(index: int) -> str:
    return str(uuid.UUID(int=index))


def _build_records() -> list[dict]:
    """Build ordered list of records with UUIDs assigned in category/item_id order.

    The UUID assigned to each record is the d_item_datas_id, and it is also
    used as user_item_id in the JSON output — both must stay in sync.
    """
    items = sorted(_load_yaml(ITEMS_YAML), key=lambda i: i["Id"])
    by_category: dict[int, list] = defaultdict(list)
    for item in items:
        by_category[item["ItemCategory"]].append(item)

    counter = 0
    records = []
    for category in sorted(by_category.keys()):
        for item in by_category[category]:
            records.append({
                "d_item_datas_id": _sequential_uuid(counter),
                "item_id": item["Id"],
                "item_type": item["ItemType"],
                "item_category": item["ItemCategory"],
                "rarity": item["Rarity"],
                "name": item.get("Name", ""),
                "name_furigana": item.get("NameFurigana", ""),
                "description": item.get("Description", ""),
                "effect_value": item.get("EffectValue", 0),
                "limit_num": item["LimitNum"],
                "requestable_num": item.get("RequestableNum", 0),
                "start_time": str(item["StartTime"]) if item.get("StartTime") else "",
                "end_time": str(item["EndTime"]) if item.get("EndTime") else "",
                "item_num": item["LimitNum"],
                "limit_date_time": None,
                "quest_list": [],
                "resource_file_name": "",
                "transition_scene": "",
            })
            counter += 1
    return records


def generate_schema_ddl() -> str:
    lines = [
        "CREATE TABLE IF NOT EXISTS item (",
        "    d_item_datas_id TEXT NOT NULL PRIMARY KEY,",
        "    response_json TEXT NOT NULL,",
        "    item_id INTEGER NOT NULL DEFAULT 0,",
        "    item_type INTEGER NOT NULL DEFAULT 0,",
        "    item_category INTEGER NOT NULL DEFAULT 0,",
        "    rarity INTEGER NOT NULL DEFAULT 0,",
        "    name TEXT NOT NULL DEFAULT '',",
        "    name_furigana TEXT NOT NULL DEFAULT '',",
        "    description TEXT NOT NULL DEFAULT '',",
        "    effect_value INTEGER NOT NULL DEFAULT 0,",
        "    limit_num INTEGER NOT NULL DEFAULT 0,",
        "    requestable_num INTEGER NOT NULL DEFAULT 0,",
        "    start_time TEXT NOT NULL DEFAULT '',",
        "    end_time TEXT NOT NULL DEFAULT '',",
        "    item_num INTEGER NOT NULL DEFAULT 0,",
        "    limit_date_time TEXT,",
        "    quest_list TEXT NOT NULL DEFAULT '[]',",
        "    resource_file_name TEXT NOT NULL DEFAULT '',",
        "    transition_scene TEXT NOT NULL DEFAULT ''",
        ");",
        "",
        "CREATE INDEX IF NOT EXISTS idx_item_item_id ON item(item_id);",
        "CREATE INDEX IF NOT EXISTS idx_item_item_type ON item(item_type);",
        "CREATE INDEX IF NOT EXISTS idx_item_item_category ON item(item_category);",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    records = _build_records()
    columns = [
        "d_item_datas_id", "response_json",
        "item_id", "item_type", "item_category", "rarity",
        "name", "name_furigana", "description", "effect_value",
        "limit_num", "requestable_num", "start_time", "end_time",
        "item_num", "limit_date_time", "quest_list", "resource_file_name", "transition_scene",
    ]
    statements: list[str] = []
    for rec in records:
        response_json = json.dumps({
            "d_item_datas_id": rec["d_item_datas_id"],
            "description": rec["description"],
            "item_num": rec["item_num"],
            "item_type": rec["item_type"],
            "limit_date_time": rec["limit_date_time"],
            "name": rec["name"],
            "quest_list": rec["quest_list"],
            "resource_file_name": rec["resource_file_name"],
            "transition_scene": rec["transition_scene"],
        }, ensure_ascii=False, separators=(",", ":"))
        values = [
            rec["d_item_datas_id"],
            response_json,
            rec["item_id"],
            rec["item_type"],
            rec["item_category"],
            rec["rarity"],
            rec["name"],
            rec["name_furigana"],
            rec["description"],
            rec["effect_value"],
            rec["limit_num"],
            rec["requestable_num"],
            rec["start_time"],
            rec["end_time"],
            rec["item_num"],
            rec["limit_date_time"],
            rec["quest_list"],
            rec["resource_file_name"],
            rec["transition_scene"],
        ]
        encoded = ", ".join(sql_utils.encode_sql_value(v) for v in values)
        statements.append(
            "INSERT OR REPLACE INTO item ("
            + ", ".join(columns)
            + f") VALUES ({encoded});"
        )
    return statements


def generate_json() -> None:
    _generate_json(DEFAULT_JSON_OUTPUT)


def _generate_json(output_path: Path) -> None:
    records = _build_records()

    seen_categories: list[int] = []
    by_category: dict[int, list] = {}
    for rec in records:
        cat = rec["item_category"]
        if cat not in by_category:
            seen_categories.append(cat)
            by_category[cat] = []
        by_category[cat].append({
            "user_item_id": rec["d_item_datas_id"],
            "item_id": rec["item_id"],
            "item_type": rec["item_type"],
            "rarity": rec["rarity"],
            "item_num": rec["item_num"],
            "resource_file_name": rec["resource_file_name"],
        })

    item_category_list = [
        {"item_category": cat, "user_item_list": by_category[cat]}
        for cat in seen_categories
    ]
    output = {"item_category_list": item_category_list}
    output_path.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    total = sum(len(v) for v in by_category.values())
    print(f"Written {total} items across {len(item_category_list)} categories → {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Generate user_items_get_list.json from Items masterdata.")
    parser.add_argument(
        "--output",
        default=str(DEFAULT_JSON_OUTPUT),
        help="Output JSON path.",
    )
    args = parser.parse_args()
    _generate_json(Path(args.output))


if __name__ == "__main__":
    main()
