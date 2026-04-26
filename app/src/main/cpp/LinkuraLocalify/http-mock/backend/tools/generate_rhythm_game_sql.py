from __future__ import annotations


def generate_schema_ddl() -> str:
    lines = [
        "CREATE TABLE IF NOT EXISTS rhythm_music_score (",
        "    music_id INTEGER NOT NULL PRIMARY KEY,",
        "    high_score INTEGER NOT NULL DEFAULT 0,",
        "    high_score_achievement_status INTEGER NOT NULL DEFAULT 0,",
        "    clear_count INTEGER NOT NULL DEFAULT 0,",
        "    total_score_accumulated INTEGER NOT NULL DEFAULT 0,",
        "    music_mastery_level INTEGER NOT NULL DEFAULT 0,",
        "    difficulty_scores_json TEXT NOT NULL DEFAULT '[]'",
        ");",
        "",
        "CREATE TABLE IF NOT EXISTS rhythm_game_deck (",
        "    rhythm_game_deck_id TEXT NOT NULL PRIMARY KEY,",
        "    name TEXT NOT NULL DEFAULT '',",
        "    deck_no INTEGER NOT NULL DEFAULT 0,",
        "    deck_card_list_json TEXT NOT NULL DEFAULT '[]'",
        ");",
    ]
    return "\n".join(lines)


def generate_seed_statements() -> list[str]:
    return []
