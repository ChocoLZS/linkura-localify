from __future__ import annotations


def generate_schema_ddl() -> str:
    lines = [
        "CREATE TABLE IF NOT EXISTS deck (",
        "    d_deck_datas_id TEXT NOT NULL PRIMARY KEY,",
        "    deck_name TEXT NOT NULL DEFAULT '',",
        "    deck_no INTEGER NOT NULL DEFAULT 0,",
        "    generations_id INTEGER NOT NULL DEFAULT 0,",
        "    ace_card TEXT NOT NULL DEFAULT '',",
        "    deck_cards_json TEXT NOT NULL DEFAULT '[]'",
        ");",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    return []
