# backend/tools — Data Generation Pipeline

将 `link-like-diff` 子模块中的 YAML masterdata 转换为 mock 服务器所需的 JSON 和 SQL。

## 目录结构

```
backend/tools/
├── link-like-diff/              # git submodule (vertesan/link-like-diff)
│   ├── Items.yaml
│   ├── CardDatas.yaml
│   ├── Characters.yaml
│   └── ...
├── data/                        # 中间产物 (gitignored)
│   ├── user_card_get_list.json
│   ├── get_character_info.json
│   └── archive_archive_details.json  (手动放入)
├── generate_card_detail_sql.py      # CardDatas → JSON + SQL (card_detail 表)
├── generate_character_info_sql.py   # Characters → JSON + SQL (character_info 表)
├── generate_user_items.py           # Items → JSON + SQL (item 表)
├── generate_custom_setting.py       # CardDatas+Stickers → JSON (无 SQL)
├── generate_archive_detail_sql.py   # data/archive_archive_details.json → SQL (archive_detail 表)
├── generate_all.py                  # 统一入口
└── sql_utils.py                     # SQL 工具函数
```

## 数据流

每个 `generate_*` 脚本直接读取 `link-like-diff/` 中的 YAML，同时生成 JSON 和 SQL：

```
link-like-diff/*.yaml
        │
        ├─ generate_card_detail_sql.py ──→ data/ + builtin/user_card_get_list.json
        │                              └─→ SQL card_detail 表
        │
        ├─ generate_character_info_sql.py ─→ data/get_character_info.json
        │                                └─→ SQL character_info 表
        │
        ├─ generate_user_items.py ──→ builtin/user_items_get_list.json
        │                         └─→ SQL item 表
        │
        └─ generate_custom_setting.py ──→ builtin/get_custom_setting.json
                                         (仅 JSON，无 SQL)

data/archive_archive_details.json (手动)
        │
        └─ generate_archive_detail_sql.py ──→ SQL archive_detail 表
```

### 输出一览

| 脚本 | 读取 YAML | JSON 输出 | SQL 表 |
|------|----------|----------|--------|
| `generate_card_detail_sql.py` | CardDatas, Characters, CardRarities, CardLimitBreakMaterials, CardSkills, RhythmGameSkills | `data/` + `builtin/` | `card_detail` |
| `generate_character_info_sql.py` | Characters, CardDatas, StyleMovies, StyleVoices | `data/` | `character_info` |
| `generate_user_items.py` | Items | `builtin/` | `item` |
| `generate_custom_setting.py` | CardDatas, Stickers | `builtin/` | — |
| `generate_archive_detail_sql.py` | — (读 `data/` JSON) | — | `archive_detail` |

## 使用方式

### 前置条件

```bash
pip install pyyaml
```

### 一键生成

```bash
cd backend/tools
python generate_all.py
```

### 单独运行

```bash
# 每个脚本均可独立运行，会同时生成 JSON 和 SQL
python generate_card_detail_sql.py
python generate_character_info_sql.py
python generate_user_items.py
python generate_custom_setting.py
```

## 更新 link-like-diff 子模块

当上游 masterdata 更新时：

```bash
# 在仓库根目录执行

# 1. 更新子模块
git submodule update --remote app/src/main/cpp/LinkuraLocalify/http-mock/backend/tools/link-like-diff

# 2. 重新生成
python app/src/main/cpp/LinkuraLocalify/http-mock/backend/tools/generate_all.py

# 3. 检查变更
git diff --stat

# 4. 提交
git add app/src/main/cpp/LinkuraLocalify/http-mock/backend/tools/link-like-diff
git add app/src/main/cpp/LinkuraLocalify/http-mock/backend/schema/
git add app/src/main/cpp/LinkuraLocalify/http-mock/backend/seed/
git add app/src/main/cpp/LinkuraLocalify/http-mock/builtin/
git commit -m "update: sync link-like-diff masterdata"
```

### 一键脚本

在仓库根目录创建 `scripts/update_masterdata.sh`：

```bash
#!/usr/bin/env bash
set -euo pipefail

TOOLS="app/src/main/cpp/LinkuraLocalify/http-mock/backend/tools"

echo "Updating link-like-diff submodule..."
git submodule update --remote "$TOOLS/link-like-diff"

echo "Regenerating data..."
python "$TOOLS/generate_all.py"

echo "Done. Review changes with: git diff --stat"
```

## 注意事项

- `data/` 中的 `.json` 和 `.yaml` 文件被 `.gitignore` 排除
- `archive_archive_details.json` 没有自动生成脚本，需要手动放入 `data/`
- `item` 表的 `d_item_datas_id` 与 `builtin/user_items_get_list.json` 中的 `user_item_id` 使用相同 UUID 生成逻辑，保证一一对应
- `card_detail` 表的 `d_card_datas_id` 同理
