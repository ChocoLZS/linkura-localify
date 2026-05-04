#include "HttpMockBackend.hpp"

#include "../../HookMain.h"
#include "../../Local.h"
#include "http_mock_backend_builtin_sql.hpp"
#include "offline_api_mock_builtin.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <string>

#include "sqlite3.h"

namespace LinkuraLocal::HttpMock {
    namespace {
        static std::filesystem::path GetBackendDatabasePath() {
            const auto base = LinkuraLocal::Local::GetBasePath();
            const auto configDir = base.parent_path();
            return (configDir / "HasuKikaisann.sqlite3").lexically_normal();
        }

        static std::string ExtractJsonIntegerFieldAsString(std::string_view payloadJson, std::string_view fieldName) {
            if (payloadJson.empty()) {
                return {};
            }

            const auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
            if (!payload.is_object()) {
                return {};
            }

            const auto it = payload.find(std::string(fieldName));
            if (it == payload.end()) {
                return {};
            }

            if (it->is_number_integer()) {
                return std::to_string(it->get<int64_t>());
            }
            if (it->is_string()) {
                return it->get<std::string>();
            }

            return {};
        }

        static std::string ExtractJsonStringField(std::string_view payloadJson, std::string_view fieldName) {
            if (payloadJson.empty()) {
                return {};
            }

            const auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
            if (!payload.is_object()) {
                return {};
            }

            const auto it = payload.find(std::string(fieldName));
            if (it == payload.end() || !it->is_string()) {
                return {};
            }

            return it->get<std::string>();
        }
        static const nlohmann::json kDefaultCharacterBonus = {
            {"character_id", 0},
            {"music_mastery_bonus", 0},
            {"love_correction_value", 0},
            {"music_mastery_bonus_list", nullptr},
            {"season_fan_level", 0},
        };

        static const nlohmann::json kDefaultRentalDeckData = {
            {"rental_deck_id", 0},
            {"rental_deck_cards_list", nullptr},
            {"grade_bonus_value", 0},
            {"is_released", false},
        };
    }

    struct HttpMockBackend::Impl {
        std::mutex mutex;
        bool initialized = false;
        bool persistentStorageAvailable = false;
        sqlite3* db = nullptr;

        ~Impl() {
            if (db) {
                sqlite3_close(db);
                db = nullptr;
            }
        }

        bool EnsureReadyLocked();
        bool ResetLocked();
        bool RebuildLocked();
        std::optional<MockStoredResponse> GetArchiveDetailByIdLocked(std::string_view archivesId);
        std::optional<MockStoredResponse> GetCardDetailByDCardIdLocked(std::string_view dCardDatasId);
        std::optional<MockStoredResponse> GetItemDetailByDItemIdLocked(std::string_view dItemDatasId);
        std::optional<MockStoredResponse> GetCharacterInfoByIdLocked(std::string_view characterId);
        std::optional<MockStoredResponse> GetDeckListResponseLocked();
        std::optional<MockStoredResponse> ModifyDeckListLocked(std::string_view payloadJson);
        int currentRhythmMusicId = 0;
        int currentRhythmDifficulty = 1;

        struct QuestLiveState {
            std::string questLiveId;
            int questLiveType = 0;
            int stageId = 0;
            int musicId = 0;
            bool isChallengeMode = false;
            nlohmann::json deckData;
            nlohmann::json characterBonus;
            std::string startTime;
            int score = 0;
            std::string playReport;
            bool finished = false;
        };
        QuestLiveState currentQuestLive;

        std::optional<MockStoredResponse> QuestStageSelectLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestStageDataLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestGetLiveSettingLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSetLiveSettingLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSetStartLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestGetLiveInfoLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSetFinishLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestGetResultLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSkipLocked(std::string_view payloadJson);

        std::optional<MockStoredResponse> GetRhythmGameHomeLocked();
        std::optional<MockStoredResponse> RhythmGameSetStartLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> RhythmGameSetFinishLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> ModifyRhythmGameDeckListLocked(std::string_view payloadJson);
    };

    namespace {
        static bool ExecSql(sqlite3* db, const char* sql) {
            char* errorMessage = nullptr;
            const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errorMessage);
            if (rc == SQLITE_OK) {
                return true;
            }

            Log::ErrorFmt("[HttpMockBackend] sqlite exec failed rc=%d err=%s",
                          rc,
                          errorMessage ? errorMessage : "(null)");
            if (errorMessage) {
                sqlite3_free(errorMessage);
            }
            return false;
        }

        static bool BindText(sqlite3_stmt* stmt, int index, std::string_view value) {
            return sqlite3_bind_text(stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        }

        static std::string GenerateUuid4() {
            static thread_local std::mt19937_64 rng(std::random_device{}());
            std::uniform_int_distribution<uint64_t> dist;
            const uint64_t hi = dist(rng);
            const uint64_t lo = dist(rng);

            uint8_t bytes[16];
            for (int i = 0; i < 8; ++i) bytes[i] = static_cast<uint8_t>(hi >> (56 - i * 8));
            for (int i = 0; i < 8; ++i) bytes[8 + i] = static_cast<uint8_t>(lo >> (56 - i * 8));

            bytes[6] = (bytes[6] & 0x0F) | 0x40;
            bytes[8] = (bytes[8] & 0x3F) | 0x80;

            char buf[37];
            snprintf(buf, sizeof(buf),
                     "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     bytes[0], bytes[1], bytes[2], bytes[3],
                     bytes[4], bytes[5], bytes[6], bytes[7],
                     bytes[8], bytes[9], bytes[10], bytes[11],
                     bytes[12], bytes[13], bytes[14], bytes[15]);
            return std::string(buf, 36);
        }

        static bool ExecSqlScript(sqlite3* db, const HttpMockBackendBuiltInSql::BuiltinSqlScript& script) {
            const std::string sql(script.sql);
            if (sql.empty()) {
                return true;
            }

            if (ExecSql(db, sql.c_str())) {
                return true;
            }

            Log::ErrorFmt("[HttpMockBackend] failed builtin SQL script: %.*s",
                          static_cast<int>(script.path.size()),
                          script.path.data());
            return false;
        }

        template <size_t N>
        static bool ExecBuiltInSqlScripts(sqlite3* db,
                                          const std::array<HttpMockBackendBuiltInSql::BuiltinSqlScript, N>& scripts,
                                          std::string_view groupName) {
            for (const auto& script : scripts) {
                if (!ExecSqlScript(db, script)) {
                    Log::ErrorFmt("[HttpMockBackend] builtin SQL group failed: %.*s",
                                  static_cast<int>(groupName.size()),
                                  groupName.data());
                    return false;
                }
            }
            return true;
        }

        static bool TableHasAnyRows(sqlite3* db, const char* tableName) {
            sqlite3_stmt* stmt = nullptr;
            const std::string sql = "SELECT EXISTS(SELECT 1 FROM " + std::string(tableName) + " LIMIT 1);";
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
                Log::ErrorFmt("[HttpMockBackend] sqlite prepare failed for table probe: %s", sqlite3_errmsg(db));
                if (stmt) {
                    sqlite3_finalize(stmt);
                }
                return false;
            }

            const int rc = sqlite3_step(stmt);
            const bool hasRows = (rc == SQLITE_ROW) && (sqlite3_column_int(stmt, 0) != 0);
            sqlite3_finalize(stmt);
            return hasRows;
        }
    }

    bool HttpMockBackend::Impl::EnsureReadyLocked() {
        if (initialized) {
            return true;
        }
        initialized = true;

        const auto dbPath = GetBackendDatabasePath();
        std::error_code ec;
        std::filesystem::create_directories(dbPath.parent_path(), ec);
        if (ec) {
            Log::WarnFmt("[HttpMockBackend] failed to create db directory: %s", ec.message().c_str());
        }

        const auto dbPathString = dbPath.string();
        const int openRc = sqlite3_open_v2(
            dbPathString.c_str(),
            &db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
            nullptr);
        if (openRc != SQLITE_OK || !db) {
            Log::ErrorFmt("[HttpMockBackend] sqlite open failed rc=%d path=%s", openRc, dbPathString.c_str());
            if (db) {
                sqlite3_close(db);
                db = nullptr;
            }
            persistentStorageAvailable = false;
            return false;
        }

        persistentStorageAvailable = true;

        if (!ExecBuiltInSqlScripts(db, HttpMockBackendBuiltInSql::SchemaScripts, "schema")) {
            persistentStorageAvailable = false;
            return false;
        }

        const auto resetCmdPath = dbPath.parent_path() / "mock_db_reset_cmd";
        bool hasPendingReset = std::filesystem::exists(resetCmdPath, ec);
        if (hasPendingReset) {
            Log::InfoFmt("[HttpMockBackend] pending reset command found, re-seeding");
            ExecSql(db, "DELETE FROM archive_detail;");
            ExecSql(db, "DELETE FROM card_detail;");
            ExecSql(db, "DELETE FROM character_info;");
            ExecSql(db, "DELETE FROM item;");
            ExecSql(db, "DELETE FROM deck;");
            ExecSql(db, "DELETE FROM rhythm_music_score;");
            ExecSql(db, "DELETE FROM rhythm_game_deck;");
            ExecSql(db, "DELETE FROM quest_stage;");
            ExecSql(db, "DELETE FROM music;");
            std::filesystem::remove(resetCmdPath, ec);
        }

        if ((hasPendingReset || !TableHasAnyRows(db, "archive_detail") || !TableHasAnyRows(db, "card_detail") || !TableHasAnyRows(db, "character_info") || !TableHasAnyRows(db, "item") || !TableHasAnyRows(db, "quest_stage") || !TableHasAnyRows(db, "music"))
            && !ExecBuiltInSqlScripts(db, HttpMockBackendBuiltInSql::SeedScripts, "seed")) {
            persistentStorageAvailable = false;
            return false;
        }

        return true;
    }

    bool HttpMockBackend::Impl::ResetLocked() {
        if (!EnsureReadyLocked() || !db) {
            return false;
        }

        if (!ExecSql(db, "DELETE FROM archive_detail;") || !ExecSql(db, "DELETE FROM card_detail;") || !ExecSql(db, "DELETE FROM character_info;") || !ExecSql(db, "DELETE FROM item;") || !ExecSql(db, "DELETE FROM deck;") || !ExecSql(db, "DELETE FROM rhythm_music_score;") || !ExecSql(db, "DELETE FROM rhythm_game_deck;") || !ExecSql(db, "DELETE FROM quest_stage;") || !ExecSql(db, "DELETE FROM music;")) {
            return false;
        }

        return ExecBuiltInSqlScripts(db, HttpMockBackendBuiltInSql::SeedScripts, "seed");
    }

    bool HttpMockBackend::Impl::RebuildLocked() {
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }

        const auto dbPath = GetBackendDatabasePath();
        std::error_code ec;
        std::filesystem::remove(dbPath, ec);
        if (ec) {
            Log::WarnFmt("[HttpMockBackend] failed to remove db file: %s", ec.message().c_str());
        }

        initialized = false;
        persistentStorageAvailable = false;
        return EnsureReadyLocked();
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GetArchiveDetailByIdLocked(std::string_view archivesId) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }

        if (!db || archivesId.empty()) {
            return std::nullopt;
        }

        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT response_json "
            "FROM archive_detail "
            "WHERE archives_id = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
            Log::ErrorFmt("[HttpMockBackend] sqlite prepare failed for archive_detail query: %s", sqlite3_errmsg(db));
            if (stmt) sqlite3_finalize(stmt);
            return std::nullopt;
        }

        if (!BindText(stmt, 1, archivesId)) {
            Log::ErrorFmt("[HttpMockBackend] sqlite bind failed for archive_detail query: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const auto* responseJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        MockStoredResponse response;
        response.body = responseJson ? responseJson : "{}";
        sqlite3_finalize(stmt);
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GetCardDetailByDCardIdLocked(std::string_view dCardDatasId) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }

        if (!db || dCardDatasId.empty()) {
            return std::nullopt;
        }

        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT response_json "
            "FROM card_detail "
            "WHERE d_card_datas_id = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
            Log::ErrorFmt("[HttpMockBackend] sqlite prepare failed for card_detail query: %s", sqlite3_errmsg(db));
            if (stmt) sqlite3_finalize(stmt);
            return std::nullopt;
        }

        if (!BindText(stmt, 1, dCardDatasId)) {
            Log::ErrorFmt("[HttpMockBackend] sqlite bind failed for card_detail query: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const auto* responseJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        MockStoredResponse response;
        response.body = responseJson ? responseJson : "{}";
        sqlite3_finalize(stmt);
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GetItemDetailByDItemIdLocked(std::string_view dItemDatasId) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }

        if (!db || dItemDatasId.empty()) {
            return std::nullopt;
        }

        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT response_json "
            "FROM item "
            "WHERE d_item_datas_id = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
            Log::ErrorFmt("[HttpMockBackend] sqlite prepare failed for item query: %s", sqlite3_errmsg(db));
            if (stmt) sqlite3_finalize(stmt);
            return std::nullopt;
        }

        if (!BindText(stmt, 1, dItemDatasId)) {
            Log::ErrorFmt("[HttpMockBackend] sqlite bind failed for item query: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const auto* responseJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        MockStoredResponse response;
        response.body = responseJson ? responseJson : "{}";
        sqlite3_finalize(stmt);
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GetCharacterInfoByIdLocked(std::string_view characterId) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }

        if (!db || characterId.empty()) {
            return std::nullopt;
        }

        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT response_json "
            "FROM character_info "
            "WHERE character_id = ?;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
            Log::ErrorFmt("[HttpMockBackend] sqlite prepare failed for character_info query: %s", sqlite3_errmsg(db));
            if (stmt) sqlite3_finalize(stmt);
            return std::nullopt;
        }

        if (!BindText(stmt, 1, characterId)) {
            Log::ErrorFmt("[HttpMockBackend] sqlite bind failed for character_info query: %s", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return std::nullopt;
        }

        const auto* responseJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        MockStoredResponse response;
        response.body = responseJson ? responseJson : "{}";
        sqlite3_finalize(stmt);
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GetDeckListResponseLocked() {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        nlohmann::json deckList = nlohmann::json::array();

        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT d_deck_datas_id, deck_name, deck_no, generations_id, ace_card, deck_cards_json "
            "FROM deck ORDER BY deck_no;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
            Log::ErrorFmt("[HttpMockBackend] sqlite prepare failed for deck list: %s", sqlite3_errmsg(db));
            if (stmt) sqlite3_finalize(stmt);
            return std::nullopt;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const int deckNo = sqlite3_column_int(stmt, 2);
            const int genId = sqlite3_column_int(stmt, 3);
            const auto* aceCard = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            const auto* cardsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

            nlohmann::json deck;
            deck["d_deck_datas_id"] = id ? id : "";
            deck["deck_name"] = name ? name : "";
            deck["deck_no"] = deckNo;
            deck["generations_id"] = genId;

            const std::string aceStr = aceCard ? aceCard : "";
            if (!aceStr.empty()) {
                deck["ace_card"] = aceStr;
            }

            auto cardsParsed = nlohmann::json::parse(cardsJson ? cardsJson : "[]", nullptr, false);
            deck["deck_cards_list"] = cardsParsed.is_array() ? cardsParsed : nlohmann::json::array();

            deckList.push_back(std::move(deck));
        }
        sqlite3_finalize(stmt);

        nlohmann::json result;
        result["deck_list"] = std::move(deckList);

        const auto& cardListObj = OfflineApiMockBuiltIn::UserCardGetListJsonObj();
        if (cardListObj.contains("user_card_data_list")) {
            result["user_card_data_list"] = cardListObj["user_card_data_list"];
        } else {
            result["user_card_data_list"] = nlohmann::json::array();
        }
        result["rental_deck_list"] = nlohmann::json::array();
        result["rental_card_data_list"] = nlohmann::json::array();

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::ModifyDeckListLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db || payloadJson.empty()) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (!payload.is_object() || !payload.contains("modify_deck_list") || !payload["modify_deck_list"].is_array()) {
            return std::nullopt;
        }

        nlohmann::json responseDeckList = nlohmann::json::array();

        constexpr const char* upsertSql =
            "INSERT OR REPLACE INTO deck (d_deck_datas_id, deck_name, deck_no, generations_id, ace_card, deck_cards_json) "
            "VALUES (?, ?, ?, ?, ?, ?);";

        for (auto& deckEntry : payload["modify_deck_list"]) {
            std::string deckId = deckEntry.value("d_deck_datas_id", std::string{});
            if (deckId.empty()) {
                deckId = GenerateUuid4();
            }

            nlohmann::json cardsList = nlohmann::json::array();
            if (deckEntry.contains("deck_cards_list") && deckEntry["deck_cards_list"].is_array()) {
                cardsList = deckEntry["deck_cards_list"];
                for (auto& card : cardsList) {
                    std::string cardId = card.value("d_deck_cards_id", std::string{});
                    if (cardId.empty()) {
                        card["d_deck_cards_id"] = GenerateUuid4();
                    }
                }
            }

            const std::string deckName = deckEntry.value("deck_name", std::string{});
            const int deckNo = deckEntry.value("deck_no", 0);
            const int genId = deckEntry.value("generations_id", 0);
            const std::string aceCard = deckEntry.value("ace_card", std::string{});
            const std::string cardsJsonStr = cardsList.dump();

            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, upsertSql, -1, &stmt, nullptr) != SQLITE_OK || !stmt) {
                Log::ErrorFmt("[HttpMockBackend] sqlite prepare failed for deck upsert: %s", sqlite3_errmsg(db));
                if (stmt) sqlite3_finalize(stmt);
                continue;
            }

            BindText(stmt, 1, deckId);
            BindText(stmt, 2, deckName);
            sqlite3_bind_int(stmt, 3, deckNo);
            sqlite3_bind_int(stmt, 4, genId);
            BindText(stmt, 5, aceCard);
            BindText(stmt, 6, cardsJsonStr);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                Log::ErrorFmt("[HttpMockBackend] sqlite step failed for deck upsert: %s", sqlite3_errmsg(db));
            }
            sqlite3_finalize(stmt);

            nlohmann::json responseDeck;
            responseDeck["d_deck_datas_id"] = deckId;
            responseDeck["deck_name"] = deckName;
            responseDeck["deck_no"] = deckNo;
            responseDeck["generations_id"] = genId;
            if (!aceCard.empty()) {
                responseDeck["ace_card"] = aceCard;
            }
            responseDeck["deck_cards_list"] = cardsList;

            responseDeckList.push_back(std::move(responseDeck));
        }

        nlohmann::json result;
        result["deck_list"] = std::move(responseDeckList);

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GetRhythmGameHomeLocked() {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        const auto& musicListObj = OfflineApiMockBuiltIn::CollectionGetMusicListJsonObj();
        nlohmann::json musicList = nlohmann::json::array();

        if (musicListObj.contains("music_info_list") && musicListObj["music_info_list"].is_array()) {
            for (const auto& mi : musicListObj["music_info_list"]) {
                const int musicId = mi.value("musics_id", 0);

                nlohmann::json entry;
                entry["music_id"] = musicId;
                entry["high_score"] = 0;
                entry["high_score_achievement_status"] = 0;
                entry["music_mastery_level"] = 1;
                entry["music_scores"] = nlohmann::json::array();

                sqlite3_stmt* stmt = nullptr;
                constexpr const char* sql =
                    "SELECT high_score, high_score_achievement_status, "
                    "music_mastery_level, difficulty_scores_json "
                    "FROM rhythm_music_score WHERE music_id = ?;";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                    sqlite3_bind_int(stmt, 1, musicId);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        entry["high_score"] = sqlite3_column_int(stmt, 0);
                        entry["high_score_achievement_status"] = sqlite3_column_int(stmt, 1);
                        entry["music_mastery_level"] = sqlite3_column_int(stmt, 2);

                        const auto* dsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                        auto parsed = nlohmann::json::parse(dsJson ? dsJson : "[]", nullptr, false);
                        entry["music_scores"] = parsed.is_array() ? parsed : nlohmann::json::array();
                    }
                    sqlite3_finalize(stmt);
                }

                musicList.push_back(std::move(entry));
            }
        }

        nlohmann::json deckList = nlohmann::json::array();
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT rhythm_game_deck_id, name, deck_no, deck_card_list_json "
                "FROM rhythm_game_deck ORDER BY deck_no;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const auto* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    const int deckNo = sqlite3_column_int(stmt, 2);
                    const auto* cardsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

                    nlohmann::json deck;
                    deck["rhythm_game_deck_id"] = id ? id : "";
                    deck["name"] = name ? name : "";
                    deck["deck_no"] = deckNo;
                    auto parsed = nlohmann::json::parse(cardsJson ? cardsJson : "[]", nullptr, false);
                    deck["deck_card_list"] = parsed.is_array() ? parsed : nlohmann::json::array();
                    deckList.push_back(std::move(deck));
                }
                sqlite3_finalize(stmt);
            }
        }

        const auto& cardListObj = OfflineApiMockBuiltIn::UserCardGetListJsonObj();
        nlohmann::json cardDataList = nlohmann::json::array();
        if (cardListObj.contains("user_card_data_list")) {
            cardDataList = cardListObj["user_card_data_list"];
        }
        for (auto& card : cardDataList) {
            if (!card.contains("character_bonus") || card["character_bonus"].empty()) {
                card["character_bonus"] = kDefaultCharacterBonus;
            }
            if (!card.contains("rhythm_game_skill_list")) {
                card["rhythm_game_skill_list"] = nlohmann::json::array();
            }
        }

        const auto& profileObj = OfflineApiMockBuiltIn::HomeGetHomeJsonObj();
        nlohmann::json memberFanlevelList = nlohmann::json::array();
        if (profileObj.contains("profile_info") && profileObj["profile_info"].contains("fan_level_list")) {
            for (const auto& fl : profileObj["profile_info"]["fan_level_list"]) {
                nlohmann::json mfl;
                mfl["characters_id"] = fl.value("character_id", 0);
                mfl["member_fanlevel"] = fl.value("member_fan_level", 0);
                memberFanlevelList.push_back(std::move(mfl));
            }
        }

        int64_t totalClearCount = 0;
        int64_t totalScoreAccum = 0;
        int64_t totalHighScore = 0;
        int starTotal = 0;
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT COALESCE(SUM(clear_count),0), COALESCE(SUM(total_score_accumulated),0), "
                "COALESCE(SUM(high_score),0), high_score_achievement_status, difficulty_scores_json "
                "FROM rhythm_music_score;";
            constexpr const char* allSql =
                "SELECT high_score_achievement_status, difficulty_scores_json "
                "FROM rhythm_music_score;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    totalClearCount = sqlite3_column_int64(stmt, 0);
                    totalScoreAccum = sqlite3_column_int64(stmt, 1);
                    totalHighScore = sqlite3_column_int64(stmt, 2);
                }
                sqlite3_finalize(stmt);
            }
            stmt = nullptr;
            if (sqlite3_prepare_v2(db, allSql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    starTotal += sqlite3_column_int(stmt, 0);
                    const auto* dsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    auto ds = nlohmann::json::parse(dsJson ? dsJson : "[]", nullptr, false);
                    if (ds.is_array()) {
                        for (const auto& d : ds) {
                            starTotal += d.value("combo_achievement_status", 0);
                            starTotal += d.value("clear_lamp", 0);
                        }
                    }
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["music_list"] = std::move(musicList);
        result["rhythm_game_deck_list"] = std::move(deckList);
        result["card_data_list"] = std::move(cardDataList);
        result["class_mission_list"] = nlohmann::json::array({
            {{"condition_type", 1}, {"progress_num", totalClearCount}, {"received_order", 99}},
            {{"condition_type", 2}, {"progress_num", totalScoreAccum}, {"received_order", 99}},
            {{"condition_type", 3}, {"progress_num", totalHighScore}, {"received_order", 99}},
        });
        result["received_total_mission_order"] = 99;
        result["rhythm_game_star_total_count"] = starTotal;
        result["member_fanlevel_list"] = std::move(memberFanlevelList);
        result["friend_card_list"] = nlohmann::json::array();
        result["auto_play_ticket_info"] = {
            {"num", 10}, {"max", 10}, {"next_reset_time", "2023-04-15T00:00:00Z"}
        };

        const auto debugPath = GetBackendDatabasePath().parent_path() / "rhythm_game_home.json";
        {
            std::ofstream ofs(debugPath, std::ios::trunc);
            if (ofs.is_open()) {
                ofs << result.dump(2);
            }
        }

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::RhythmGameSetStartLocked(std::string_view payloadJson) {
        if (!payloadJson.empty()) {
            auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
            if (payload.is_object()) {
                currentRhythmMusicId = payload.value("music_id", 0);
                currentRhythmDifficulty = payload.value("music_score_difficulty", 1);
            }
        }
        MockStoredResponse response;
        response.body = "null";
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::RhythmGameSetFinishLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db || payloadJson.empty()) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (!payload.is_object()) {
            return std::nullopt;
        }

        const int score = payload.value("score", 0);
        const int musicId = currentRhythmMusicId;

        int highScoreBefore = 0;
        int hsStatusBefore = 0;
        int masteryLevelBefore = 1;
        int clearCountBefore = 0;
        nlohmann::json diffScores = nlohmann::json::array();

        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT high_score, high_score_achievement_status, "
                "music_mastery_level, clear_count, difficulty_scores_json "
                "FROM rhythm_music_score WHERE music_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, musicId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    highScoreBefore = sqlite3_column_int(stmt, 0);
                    hsStatusBefore = sqlite3_column_int(stmt, 1);
                    masteryLevelBefore = sqlite3_column_int(stmt, 2);
                    clearCountBefore = sqlite3_column_int(stmt, 3);
                    const auto* dsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                    auto parsed = nlohmann::json::parse(dsJson ? dsJson : "[]", nullptr, false);
                    if (parsed.is_array()) diffScores = std::move(parsed);
                }
                sqlite3_finalize(stmt);
            }
        }

        const int difficulty = currentRhythmDifficulty;
        int maxComboBefore = 0;
        int comboStatusBefore = 0;
        nlohmann::json* currentDiffEntry = nullptr;
        for (auto& ds : diffScores) {
            if (ds.value("difficulty", 0) == difficulty) {
                currentDiffEntry = &ds;
                maxComboBefore = ds.value("best_combo", 0);
                comboStatusBefore = ds.value("combo_achievement_status", 0);
                break;
            }
        }
        if (!currentDiffEntry) {
            diffScores.push_back({{"difficulty", difficulty}, {"best_combo", 0}, {"combo_achievement_status", 0}, {"clear_lamp", 0}});
            currentDiffEntry = &diffScores.back();
        }

        int64_t totalClearBefore = 0, totalScoreAccumBefore = 0, totalHighScoreBefore = 0;
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT COALESCE(SUM(clear_count),0), COALESCE(SUM(total_score_accumulated),0), "
                "COALESCE(SUM(high_score),0) FROM rhythm_music_score;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    totalClearBefore = sqlite3_column_int64(stmt, 0);
                    totalScoreAccumBefore = sqlite3_column_int64(stmt, 1);
                    totalHighScoreBefore = sqlite3_column_int64(stmt, 2);
                }
                sqlite3_finalize(stmt);
            }
        }

        int maxComboThisPlay = 0;
        if (payload.contains("notes_result_list") && payload["notes_result_list"].is_array()) {
            int currentCombo = 0;
            for (const auto& note : payload["notes_result_list"]) {
                const int judgement = note.value("judgement_result", 0);
                if (judgement >= 2) {
                    ++currentCombo;
                    if (currentCombo > maxComboThisPlay) maxComboThisPlay = currentCombo;
                } else {
                    currentCombo = 0;
                }
            }
        }

        const int highScoreAfter = std::max(highScoreBefore, score);
        const int maxComboAfter = std::max(maxComboBefore, maxComboThisPlay);

        int hsStatusAfter = 0;
        if (highScoreAfter >= 10000000) hsStatusAfter = 4;
        else if (highScoreAfter >= 3000000) hsStatusAfter = 3;
        else if (highScoreAfter >= 1500000) hsStatusAfter = 2;
        else if (highScoreAfter >= 500000) hsStatusAfter = 1;

        int comboStatusAfter = 0;
        if (maxComboAfter >= 131) comboStatusAfter = 4;
        else if (maxComboAfter >= 78) comboStatusAfter = 3;
        else if (maxComboAfter >= 52) comboStatusAfter = 2;
        else if (maxComboAfter >= 26) comboStatusAfter = 1;

        (*currentDiffEntry)["best_combo"] = maxComboAfter;
        (*currentDiffEntry)["combo_achievement_status"] = comboStatusAfter;
        (*currentDiffEntry)["clear_lamp"] = 1;

        const int masteryLevelAfter = std::min(masteryLevelBefore + 1, 50);

        const std::string diffScoresStr = diffScores.dump();
        {
            constexpr const char* upsertSql =
                "INSERT OR REPLACE INTO rhythm_music_score "
                "(music_id, high_score, high_score_achievement_status, "
                "clear_count, total_score_accumulated, music_mastery_level, difficulty_scores_json) "
                "VALUES (?, ?, ?, "
                "COALESCE((SELECT clear_count FROM rhythm_music_score WHERE music_id = ?), 0) + 1, "
                "COALESCE((SELECT total_score_accumulated FROM rhythm_music_score WHERE music_id = ?), 0) + ?, ?, ?);";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, upsertSql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, musicId);
                sqlite3_bind_int(stmt, 2, highScoreAfter);
                sqlite3_bind_int(stmt, 3, hsStatusAfter);
                sqlite3_bind_int(stmt, 4, musicId);
                sqlite3_bind_int(stmt, 5, musicId);
                sqlite3_bind_int(stmt, 6, score);
                sqlite3_bind_int(stmt, 7, masteryLevelAfter);
                BindText(stmt, 8, diffScoresStr);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    Log::ErrorFmt("[HttpMockBackend] rhythm_music_score upsert failed: %s", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
            }
        }

        const int64_t totalClearAfter = totalClearBefore + 1;
        const int64_t totalScoreAccumAfter = totalScoreAccumBefore + score;
        const int64_t totalHighScoreAfter = totalHighScoreBefore - highScoreBefore + highScoreAfter;

        int starTotalAfter = 0;
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT high_score_achievement_status, difficulty_scores_json "
                "FROM rhythm_music_score;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    starTotalAfter += sqlite3_column_int(stmt, 0);
                    const auto* dsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    auto ds = nlohmann::json::parse(dsJson ? dsJson : "[]", nullptr, false);
                    if (ds.is_array()) {
                        for (const auto& d : ds) {
                            starTotalAfter += d.value("combo_achievement_status", 0);
                            starTotalAfter += d.value("clear_lamp", 0);
                        }
                    }
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["music_mastery_level_result"] = {
            {"music_id", musicId},
            {"music_mastery_exp_before", 0},
            {"music_mastery_exp_after", 0},
            {"own_status", 0},
        };
        result["drop_reward_list"] = nlohmann::json::array();
        result["music_score_mission_result"] = {
            {"high_score_before", highScoreBefore},
            {"high_score_after", highScoreAfter},
            {"high_score_achievement_status_before", hsStatusBefore},
            {"high_score_achievement_status_after", hsStatusAfter},
            {"max_combo_before", maxComboBefore},
            {"max_combo_after", maxComboAfter},
            {"combo_achievement_status_before", comboStatusBefore},
            {"combo_achievement_status_after", comboStatusAfter},
            {"reward_list", nlohmann::json::array()},
        };
        result["clear_status"] = clearCountBefore == 0 ? 1 : 0;
        result["class_mission_progress_list"] = nlohmann::json::array({
            {{"condition_type", 1}, {"progress_before", totalClearBefore}, {"progress_after", totalClearAfter}},
            {{"condition_type", 2}, {"progress_before", totalScoreAccumBefore}, {"progress_after", totalScoreAccumAfter}},
            {{"condition_type", 3}, {"progress_before", totalHighScoreBefore}, {"progress_after", totalHighScoreAfter}},
        });
        result["auto_play_ticket_info"] = {
            {"num", 10}, {"max", 10}, {"next_reset_time", "2023-04-15T00:00:00Z"}
        };
        result["rhythm_game_star_total_count"] = starTotalAfter;
        result["applied_campaign_types"] = nlohmann::json::array();

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::ModifyRhythmGameDeckListLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db || payloadJson.empty()) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (!payload.is_object() || !payload.contains("modify_deck_list") || !payload["modify_deck_list"].is_array()) {
            return std::nullopt;
        }

        constexpr const char* upsertSql =
            "INSERT OR REPLACE INTO rhythm_game_deck "
            "(rhythm_game_deck_id, name, deck_no, deck_card_list_json) "
            "VALUES (?, ?, ?, ?);";

        for (auto& deckEntry : payload["modify_deck_list"]) {
            const int deckNo = deckEntry.value("deck_no", 0);

            std::string deckId;
            {
                sqlite3_stmt* stmt = nullptr;
                constexpr const char* lookupSql =
                    "SELECT rhythm_game_deck_id FROM rhythm_game_deck WHERE deck_no = ?;";
                if (sqlite3_prepare_v2(db, lookupSql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                    sqlite3_bind_int(stmt, 1, deckNo);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        const auto* existingId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                        if (existingId) deckId = existingId;
                    }
                    sqlite3_finalize(stmt);
                }
            }
            if (deckId.empty()) {
                deckId = GenerateUuid4();
            }

            nlohmann::json cardList = nlohmann::json::array();
            if (deckEntry.contains("deck_card_list") && deckEntry["deck_card_list"].is_array()) {
                cardList = deckEntry["deck_card_list"];
                for (auto& card : cardList) {
                    std::string cardId = card.value("rhythm_game_deck_cards_id", std::string{});
                    if (cardId.empty()) {
                        card["rhythm_game_deck_cards_id"] = GenerateUuid4();
                    }
                }
            }

            const std::string deckName = deckEntry.value("name", std::string{});
            const std::string cardsJsonStr = cardList.dump();

            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, upsertSql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                BindText(stmt, 1, deckId);
                BindText(stmt, 2, deckName);
                sqlite3_bind_int(stmt, 3, deckNo);
                BindText(stmt, 4, cardsJsonStr);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    Log::ErrorFmt("[HttpMockBackend] rhythm_game_deck upsert failed: %s", sqlite3_errmsg(db));
                }
                sqlite3_finalize(stmt);
            }
        }

        MockStoredResponse response;
        response.body = "null";
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestStageSelectLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int areaId = payload.is_object() ? payload.value("area_id", 0) : 0;
        if (areaId == 0) {
            return std::nullopt;
        }

        nlohmann::json stageList = nlohmann::json::array();
        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT stage_id FROM quest_stage WHERE area_id = ? ORDER BY stage_id;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
            sqlite3_bind_int(stmt, 1, areaId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                nlohmann::json stage;
                stage["stage_id"] = sqlite3_column_int(stmt, 0);
                stage["clear_status"] = 3;
                stage["is_lock"] = false;
                stageList.push_back(std::move(stage));
            }
            sqlite3_finalize(stmt);
        }

        nlohmann::json result;
        result["stage_list"] = std::move(stageList);
        result["user_stamina"] = {
            {"stamina_now", 200},
            {"stamina_max", 200},
            {"stamina_recovery_time", "2099-01-01T00:00:00Z"},
        };
        result["is_update_grade_live"] = false;

        MockStoredResponse response;
        response.body = result.dump();

        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestStageDataLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int areaId = payload.is_object() ? payload.value("area_id", 0) : 0;
        const int viewStageId = payload.is_object() ? payload.value("stage_id", 0) : 0;
        if (areaId == 0) {
            return std::nullopt;
        }

        nlohmann::json stageDetailList = nlohmann::json::array();
        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT stage_id, music_id, quest_musics_type, score1, score2, score3, gain_style_point, use_num "
            "FROM quest_stage WHERE area_id = ? ORDER BY stage_id;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
            sqlite3_bind_int(stmt, 1, areaId);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const int stageId = sqlite3_column_int(stmt, 0);
                const int musicId = sqlite3_column_int(stmt, 1);
                const int score3Val = sqlite3_column_int(stmt, 5);

                nlohmann::json detail;
                detail["stage_id"] = stageId;
                detail["is_challenge"] = false;
                detail["is_skip"] = true;
                detail["clear_status"] = 3;
                detail["best_love_music_id"] = musicId;
                detail["stage_reward_list"] = nlohmann::json::array();
                detail["best_love"] = score3Val;

                stageDetailList.push_back(std::move(detail));
            }
            sqlite3_finalize(stmt);
        }

        // Build music_list: for type=0 stages in this area, expand GenerationsId via music table
        nlohmann::json musicList = nlohmann::json::array();
        {
            sqlite3_stmt* genStmt = nullptr;
            constexpr const char* genSql =
                "SELECT music_id FROM quest_stage WHERE area_id = ? AND quest_musics_type = 0 AND music_id > 0 LIMIT 1;";
            int generationsId = 0;
            if (sqlite3_prepare_v2(db, genSql, -1, &genStmt, nullptr) == SQLITE_OK && genStmt) {
                sqlite3_bind_int(genStmt, 1, areaId);
                if (sqlite3_step(genStmt) == SQLITE_ROW) {
                    generationsId = sqlite3_column_int(genStmt, 0);
                }
                sqlite3_finalize(genStmt);
            }
            if (generationsId > 0) {
                sqlite3_stmt* mStmt = nullptr;
                constexpr const char* mSql =
                    "SELECT music_id FROM music WHERE generations_id = ? ORDER BY music_id;";
                if (sqlite3_prepare_v2(db, mSql, -1, &mStmt, nullptr) == SQLITE_OK && mStmt) {
                    sqlite3_bind_int(mStmt, 1, generationsId);
                    while (sqlite3_step(mStmt) == SQLITE_ROW) {
                        musicList.push_back({
                            {"m_musics_id", sqlite3_column_int(mStmt, 0)},
                            {"is_enable", true},
                        });
                    }
                    sqlite3_finalize(mStmt);
                }
            }
        }

        nlohmann::json result;
        result["view_stage_id"] = viewStageId;
        result["stage_detail_list"] = std::move(stageDetailList);
        result["user_stamina"] = {
            {"stamina_now", 200},
            {"stamina_max", 200},
            {"stamina_recovery_time", "2099-01-01T00:00:00Z"},
        };
        result["music_list"] = std::move(musicList);

        MockStoredResponse response;
        response.body = result.dump();

        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestGetLiveSettingLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int stageId = payload.is_object() ? payload.value("stage_id", 0) : 0;
        const int questLiveType = payload.is_object() ? payload.value("quest_live_type", 1) : 1;

        int musicId = 0;
        int questMusicsType = 0;
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT music_id, quest_musics_type FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    musicId = sqlite3_column_int(stmt, 0);
                    questMusicsType = sqlite3_column_int(stmt, 1);
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json deckList = nlohmann::json::array();
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT d_deck_datas_id, deck_name, deck_no, generations_id, deck_cards_json "
                "FROM deck ORDER BY deck_no;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const auto* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    const int deckNo = sqlite3_column_int(stmt, 2);
                    const int genId = sqlite3_column_int(stmt, 3);
                    const auto* cardsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

                    nlohmann::json deck;
                    deck["d_deck_datas_id"] = id ? id : "";
                    deck["deck_name"] = name ? name : "";
                    deck["deck_no"] = deckNo;
                    deck["generations_id"] = genId;
                    auto cardsParsed = nlohmann::json::parse(cardsJson ? cardsJson : "[]", nullptr, false);
                    deck["deck_cards_list"] = cardsParsed.is_array() ? cardsParsed : nlohmann::json::array();
                    deckList.push_back(std::move(deck));
                }
                sqlite3_finalize(stmt);
            }
        }

        const auto& cardListObj = OfflineApiMockBuiltIn::UserCardGetListJsonObj();
        nlohmann::json userCardDataList = nlohmann::json::array();
        if (cardListObj.contains("user_card_data_list")) {
            userCardDataList = cardListObj["user_card_data_list"];
        }

        nlohmann::json musicList = nlohmann::json::array();
        if (questMusicsType == 2 && musicId > 0) {
            musicList.push_back({{"m_musics_id", musicId}, {"is_enable", true}});
        } else if (questMusicsType == 0 && musicId > 0) {
            sqlite3_stmt* mStmt = nullptr;
            constexpr const char* mSql =
                "SELECT music_id FROM music WHERE generations_id = ? ORDER BY music_id;";
            if (sqlite3_prepare_v2(db, mSql, -1, &mStmt, nullptr) == SQLITE_OK && mStmt) {
                sqlite3_bind_int(mStmt, 1, musicId);
                while (sqlite3_step(mStmt) == SQLITE_ROW) {
                    musicList.push_back({
                        {"m_musics_id", sqlite3_column_int(mStmt, 0)},
                        {"is_enable", true},
                    });
                }
                sqlite3_finalize(mStmt);
            }
        }

        nlohmann::json result;
        result["quest_live_type"] = questLiveType;
        result["stage_id"] = stageId;
        result["deck_data"] = std::move(deckList);
        result["best_love_deck"] = nlohmann::json::object();
        result["best_love_musics_id"] = musicId;
        result["user_stamina"] = {
            {"stamina_now", 200},
            {"stamina_max", 200},
            {"stamina_recovery_time", "2099-01-01T00:00:00Z"},
        };
        result["music_list"] = std::move(musicList);
        result["user_card_data_list"] = std::move(userCardDataList);
        result["rental_deck_data"] = nlohmann::json::array();
        result["rental_card_data_list"] = nlohmann::json::array();
        result["friend_card_list"] = nlohmann::json::array();

        MockStoredResponse response;
        response.body = result.dump();

        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestSetLiveSettingLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (!payload.is_object()) {
            return std::nullopt;
        }

        const int stageId = payload.value("stage_id", 0);
        const std::string deckId = payload.value("deck_id", std::string{});
        const int musicId = payload.value("music_id", 0);
        const bool isChallengeMode = payload.value("is_challenge_mode", false);
        const int questLiveType = payload.value("quest_live_type", 1);

        const std::string questLiveId = std::to_string(questLiveType) + "_" + std::to_string(stageId);

        nlohmann::json deckData = nlohmann::json::object();
        if (!deckId.empty()) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT d_deck_datas_id, deck_name, deck_no, generations_id, deck_cards_json "
                "FROM deck WHERE d_deck_datas_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                BindText(stmt, 1, deckId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const auto* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    const int deckNo = sqlite3_column_int(stmt, 2);
                    const int genId = sqlite3_column_int(stmt, 3);
                    const auto* cardsJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

                    deckData["d_deck_datas_id"] = id ? id : "";
                    deckData["deck_name"] = name ? name : "";
                    deckData["deck_no"] = deckNo;
                    deckData["generations_id"] = genId;
                    auto cardsParsed = nlohmann::json::parse(cardsJson ? cardsJson : "[]", nullptr, false);
                    deckData["deck_cards_list"] = cardsParsed.is_array() ? cardsParsed : nlohmann::json::array();
                }
                sqlite3_finalize(stmt);
            }
        }

        // Look up character_bonus from the first card in deck
        nlohmann::json charBonus = kDefaultCharacterBonus;
        if (deckData.contains("deck_cards_list") && deckData["deck_cards_list"].is_array()
            && !deckData["deck_cards_list"].empty()) {
            const auto& firstCard = deckData["deck_cards_list"][0];
            const std::string cardId = firstCard.value("d_card_datas_id", std::string{});
            if (!cardId.empty()) {
                sqlite3_stmt* cbStmt = nullptr;
                constexpr const char* cbSql = "SELECT character_bonus FROM card_detail WHERE d_card_datas_id = ?;";
                if (sqlite3_prepare_v2(db, cbSql, -1, &cbStmt, nullptr) == SQLITE_OK && cbStmt) {
                    BindText(cbStmt, 1, cardId);
                    if (sqlite3_step(cbStmt) == SQLITE_ROW) {
                        const auto* cbJson = reinterpret_cast<const char*>(sqlite3_column_text(cbStmt, 0));
                        auto parsed = nlohmann::json::parse(cbJson ? cbJson : "{}", nullptr, false);
                        if (parsed.is_object() && !parsed.empty()) {
                            charBonus = std::move(parsed);
                        }
                    }
                    sqlite3_finalize(cbStmt);
                }
            }
        }

        currentQuestLive = {};
        currentQuestLive.questLiveId = questLiveId;
        currentQuestLive.questLiveType = questLiveType;
        currentQuestLive.stageId = stageId;
        currentQuestLive.musicId = musicId;
        currentQuestLive.isChallengeMode = isChallengeMode;
        currentQuestLive.deckData = deckData;
        currentQuestLive.characterBonus = charBonus;

        nlohmann::json sectionSkillList = nlohmann::json::array();
        if (stageId > 0) {
            sqlite3_stmt* skStmt = nullptr;
            constexpr const char* skSql = "SELECT section_skills_json FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, skSql, -1, &skStmt, nullptr) == SQLITE_OK && skStmt) {
                sqlite3_bind_int(skStmt, 1, stageId);
                if (sqlite3_step(skStmt) == SQLITE_ROW) {
                    const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(skStmt, 0));
                    auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                    if (parsed.is_array()) {
                        sectionSkillList = std::move(parsed);
                    }
                }
                sqlite3_finalize(skStmt);
            }
        }

        nlohmann::json result;
        result["result"] = true;
        result["quest_live_id"] = questLiveId;
        result["quest_live_type"] = questLiveType;
        result["quest_id"] = stageId;
        result["is_challenge_mode"] = isChallengeMode;
        result["music_id"] = musicId;
        result["deck_data"] = deckData;
        result["rental_deck_data"] = kDefaultRentalDeckData;
        result["character_bonus"] = charBonus;
        result["section_skill_list"] = std::move(sectionSkillList);
        result["init_hand_data"] = "";
        result["grand_prix_retry_count"] = 0;
        result["grand_prix_is_rehearsal"] = false;
        result["grand_prix_id"] = 0;
        result["grade_retry_count"] = 0;
        result["grade_add_skill_list"] = nlohmann::json::array();
        result["playable_count"] = 0;
        result["play_count"] = 0;
        result["fan_level_info_list"] = nlohmann::json::array();

        MockStoredResponse response;
        response.body = result.dump();

        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestSetStartLocked(std::string_view payloadJson) {
        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (payload.is_object()) {
            const std::string liveId = payload.value("quest_live_id", std::string{});
            if (!liveId.empty()) {
                currentQuestLive.questLiveId = liveId;
            }
        }

        std::time_t now = std::time(nullptr);
        std::tm tmLocal{};
#if defined(_WIN32)
        localtime_s(&tmLocal, &now);
#else
        localtime_r(&now, &tmLocal);
#endif
        char buf[64]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+09:00", &tmLocal);
        currentQuestLive.startTime = buf;
        currentQuestLive.finished = false;

        nlohmann::json result;
        result["quest_start_time"] = currentQuestLive.startTime;

        MockStoredResponse response;
        response.body = result.dump();

        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestGetLiveInfoLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }

        nlohmann::json sectionSkillList = nlohmann::json::array();
        if (db && currentQuestLive.stageId > 0) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT section_skills_json FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, currentQuestLive.stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                    if (parsed.is_array()) {
                        sectionSkillList = std::move(parsed);
                    }
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["result"] = true;
        result["quest_live_id"] = currentQuestLive.questLiveId;
        result["quest_live_type"] = currentQuestLive.questLiveType;
        result["quest_id"] = currentQuestLive.stageId;
        result["is_challenge_mode"] = currentQuestLive.isChallengeMode;
        result["music_id"] = currentQuestLive.musicId;
        result["deck_data"] = currentQuestLive.deckData;
        result["rental_deck_data"] = kDefaultRentalDeckData;
        result["character_bonus"] = currentQuestLive.characterBonus;
        result["section_skill_list"] = std::move(sectionSkillList);
        result["init_hand_data"] = "";
        result["grand_prix_retry_count"] = 0;
        result["grand_prix_is_rehearsal"] = false;
        result["grand_prix_id"] = 0;
        result["grade_retry_count"] = 0;
        result["grade_add_skill_list"] = nlohmann::json::array();
        result["playable_count"] = 0;
        result["play_count"] = 0;
        result["fan_level_info_list"] = nlohmann::json::array();

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestSetFinishLocked(std::string_view payloadJson) {
        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (payload.is_object()) {
            currentQuestLive.score = payload.value("score", 0);
            currentQuestLive.playReport = payload.value("play_report", std::string{});
            currentQuestLive.finished = true;
        }

        nlohmann::json result;
        result["quest_live_type"] = currentQuestLive.questLiveType;
        result["quest_result"] = true;
        result["return_resource"] = 0;
        result["applied_campaign_types"] = nlohmann::json::array();

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestGetResultLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }

        int gainStylePoint = 0;
        if (db && currentQuestLive.stageId > 0) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT gain_style_point FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, currentQuestLive.stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    gainStylePoint = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["quest_live_type"] = currentQuestLive.questLiveType;
        result["stage_id"] = currentQuestLive.stageId;
        result["quest_result"] = true;
        result["result_love"] = currentQuestLive.score;
        result["best_love"] = currentQuestLive.score;
        result["before_best_love"] = 0;
        result["add_style_point"] = gainStylePoint;
        result["is_challenge_mode"] = currentQuestLive.isChallengeMode;
        result["music_id"] = currentQuestLive.musicId;
        result["reward_list"] = nlohmann::json::array();
        result["play_report"] = currentQuestLive.playReport;
        result["mastary_level_before"] = 1;
        result["mastary_level_after"] = 1;
        result["mastary_level_experience"] = 0;
        result["mastary_level_total_experience_before"] = 0;
        result["first_clear_flag"] = false;
        result["first_complete_clear_flag"] = false;
        result["is_limit_over_style_point"] = false;

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestSkipLocked(std::string_view payloadJson) {
        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int questLiveType = payload.is_object() ? payload.value("quest_live_type", 1) : 1;
        const int stageId = payload.is_object() ? payload.value("stage_id", 0) : 0;

        int gainStylePoint = 0;
        if (db && stageId > 0) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT gain_style_point FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    gainStylePoint = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["quest_live_type"] = questLiveType;
        result["quest_live_id"] = "";
        result["user_stamina"] = {
            {"stamina_now", 200},
            {"stamina_max", 200},
            {"stamina_recovery_time", "2099-01-01T00:00:00Z"},
        };
        result["add_style_point"] = gainStylePoint;
        result["skip_reward_list"] = nlohmann::json::array();
        result["total_skip_reward_list"] = nlohmann::json::array();
        result["is_limit_over_style_point"] = false;
        result["mastery_level_before"] = 1;
        result["mastery_level_after"] = 1;
        result["mastery_level_experience"] = 0;
        result["mastery_level_total_experience_before"] = 0;
        result["before_earned_music_exp"] = 0;
        result["earned_music_exp"] = 0;
        result["raid_stamina"] = {
            {"stamina_now", 0},
            {"stamina_recovered_time", "0001-01-01T00:00:00Z"},
        };
        result["add_once_event_point"] = 0;
        result["event_point_reward_list"] = nlohmann::json::array();
        result["event_personal_total_point"] = 0;
        result["applied_campaign_types"] = nlohmann::json::array();

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    HttpMockBackend& HttpMockBackend::Get() {
        static HttpMockBackend instance;
        return instance;
    }

    HttpMockBackend::HttpMockBackend()
        : impl_(std::make_unique<Impl>()) {}

    HttpMockBackend::~HttpMockBackend() = default;

    bool HttpMockBackend::Reset() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->ResetLocked();
    }

    bool HttpMockBackend::Rebuild() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->RebuildLocked();
    }

    bool HttpMockBackend::ResetAtFilesDir(const std::string& filesDir) {
        const std::string dbPath = filesDir + "/HasuKikaisann.sqlite3";

        sqlite3* db = nullptr;
        if (sqlite3_open_v2(dbPath.c_str(), &db,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK || !db) {
            if (db) sqlite3_close(db);
            Log::ErrorFmt("[HttpMockBackend] ResetAtFilesDir: open failed: %s", dbPath.c_str());
            return false;
        }

        bool ok = ExecSql(db, "DELETE FROM archive_detail;") &&
                  ExecSql(db, "DELETE FROM card_detail;") &&
                  ExecSql(db, "DELETE FROM character_info;") &&
                  ExecSql(db, "DELETE FROM item;") &&
                  ExecSql(db, "DELETE FROM deck;") &&
                  ExecSql(db, "DELETE FROM rhythm_music_score;") &&
                  ExecSql(db, "DELETE FROM rhythm_game_deck;") &&
                  ExecSql(db, "DELETE FROM quest_stage;") &&
                  ExecSql(db, "DELETE FROM music;") &&
                  ExecBuiltInSqlScripts(db, HttpMockBackendBuiltInSql::SeedScripts, "seed");

        sqlite3_close(db);
        return ok;
    }

    bool HttpMockBackend::RebuildAtFilesDir(const std::string& filesDir) {
        const std::string dbPath = filesDir + "/HasuKikaisann.sqlite3";

        std::error_code ec;
        std::filesystem::remove(dbPath, ec);
        if (ec) {
            Log::WarnFmt("[HttpMockBackend] RebuildAtFilesDir: remove failed: %s", ec.message().c_str());
        }

        sqlite3* db = nullptr;
        if (sqlite3_open_v2(dbPath.c_str(), &db,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK || !db) {
            if (db) sqlite3_close(db);
            Log::ErrorFmt("[HttpMockBackend] RebuildAtFilesDir: open failed: %s", dbPath.c_str());
            return false;
        }

        bool ok = ExecBuiltInSqlScripts(db, HttpMockBackendBuiltInSql::SchemaScripts, "schema") &&
                  ExecBuiltInSqlScripts(db, HttpMockBackendBuiltInSql::SeedScripts, "seed");

        sqlite3_close(db);
        return ok;
    }

    std::optional<MockStoredResponse> HttpMockBackend::GetArchiveDetailById(std::string_view archivesId) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GetArchiveDetailByIdLocked(archivesId);
    }

    std::optional<MockStoredResponse> HttpMockBackend::LookupArchiveDetailFromPayload(std::string_view payloadJson,
                                                                                      std::string_view fallbackArchiveId) {
        auto archivesId = ExtractPayloadStringField(payloadJson, "archives_id");
        if (archivesId.empty()) {
            archivesId = std::string(fallbackArchiveId);
        }
        if (archivesId.empty()) {
            return std::nullopt;
        }
        return GetArchiveDetailById(archivesId);
    }

    bool HttpMockBackend::IsPersistentStorageAvailable() const {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->initialized) {
            impl_->EnsureReadyLocked();
        }
        return impl_->persistentStorageAvailable;
    }

    std::string HttpMockBackend::GetStatusSummary() const {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->initialized) {
            impl_->EnsureReadyLocked();
        }

        if (impl_->persistentStorageAvailable) {
            return "sqlite";
        }

        return "sqlite-unavailable";
    }

    std::string HttpMockBackend::ExtractPayloadStringField(std::string_view payloadJson, std::string_view fieldName) {
        return ExtractJsonStringField(payloadJson, fieldName);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GetCardDetailByDCardId(std::string_view dCardDatasId) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GetCardDetailByDCardIdLocked(dCardDatasId);
    }

    std::optional<MockStoredResponse> HttpMockBackend::LookupCardDetailFromPayload(std::string_view payloadJson) {
        const auto dCardDatasId = ExtractPayloadStringField(payloadJson, "d_card_datas_id");
        if (dCardDatasId.empty()) {
            return std::nullopt;
        }
        return GetCardDetailByDCardId(dCardDatasId);
    }

    std::optional<MockStoredResponse> HttpMockBackend::CheckStyleLevelUp(std::string_view payloadJson) {
        const auto dCardDatasId = ExtractPayloadStringField(payloadJson, "d_card_datas_id");
        if (dCardDatasId.empty()) {
            return std::nullopt;
        }
        auto record = GetCardDetailByDCardId(dCardDatasId);
        if (!record.has_value()) {
            return std::nullopt;
        }
        auto cardObj = nlohmann::json::parse(record->body, nullptr, false);
        if (!cardObj.is_object()) {
            return std::nullopt;
        }
        nlohmann::json result;
        result["is_possible"] = false;
        result["current_style_level"] = cardObj.value("style_level", 0);
        result["selectable_max_level"] = cardObj.value("max_style_level", 0);
        result["style_point"] = 0;
        return MockStoredResponse{result.dump(), {}, 200, "OK (offline mock)"};
    }

    std::optional<MockStoredResponse> HttpMockBackend::GetItemDetailByDItemId(std::string_view dItemDatasId) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GetItemDetailByDItemIdLocked(dItemDatasId);
    }

    std::optional<MockStoredResponse> HttpMockBackend::LookupItemDetailFromPayload(std::string_view payloadJson) {
        const auto dItemDatasId = ExtractPayloadStringField(payloadJson, "d_item_datas_id");
        if (dItemDatasId.empty()) {
            return std::nullopt;
        }
        return GetItemDetailByDItemId(dItemDatasId);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GetCharacterInfoById(std::string_view characterId) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GetCharacterInfoByIdLocked(characterId);
    }

    std::optional<MockStoredResponse> HttpMockBackend::LookupCharacterInfoFromPayload(std::string_view payloadJson) {
        const auto characterId = ExtractPayloadIntegerFieldAsString(payloadJson, "character_id");
        if (characterId.empty()) {
            return std::nullopt;
        }
        return GetCharacterInfoById(characterId);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GetDeckListResponse() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GetDeckListResponseLocked();
    }

    std::optional<MockStoredResponse> HttpMockBackend::ModifyDeckList(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->ModifyDeckListLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GetRhythmGameHome() {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GetRhythmGameHomeLocked();
    }

    std::optional<MockStoredResponse> HttpMockBackend::RhythmGameSetStart(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->RhythmGameSetStartLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::RhythmGameSetFinish(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->RhythmGameSetFinishLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::ModifyRhythmGameDeckList(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->ModifyRhythmGameDeckListLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestStageSelect(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestStageSelectLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestStageData(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestStageDataLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestGetLiveSetting(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestGetLiveSettingLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestSetLiveSetting(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestSetLiveSettingLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestSetStart(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestSetStartLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestGetLiveInfo(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestGetLiveInfoLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestSetFinish(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestSetFinishLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestGetResult(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestGetResultLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::QuestSkip(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->QuestSkipLocked(payloadJson);
    }

    std::string HttpMockBackend::ExtractPayloadIntegerFieldAsString(std::string_view payloadJson, std::string_view fieldName) {
        return ExtractJsonIntegerFieldAsString(payloadJson, fieldName);
    }
}
