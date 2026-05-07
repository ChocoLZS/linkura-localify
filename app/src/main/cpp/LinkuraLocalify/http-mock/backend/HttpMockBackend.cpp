#include "HttpMockBackend.hpp"

#include "../../HookMain.h"
#include "../../Local.h"
#include "http_mock_backend_builtin_sql.hpp"
#include "offline_api_mock_builtin.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

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
            int64_t score = 0;
            std::string playReport;
            bool finished = false;
        };
        QuestLiveState currentQuestLive;

        struct GradeSquareState {
            int squareId = 0;
            int squareType = 0;
            int targetId = 0;
            int actionPointCost = 0;
            int status = 0;
            int64_t livePoint = 0;
            std::vector<int> openSquareIds;
        };
        struct GradeQuestState {
            bool active = false;
            int seriesId = 0;
            int characterId = 0;
            int generation = 0;
            int season = 0;
            int actionPoint = 0;
            int currentSquareId = 0;
            int resourceValue = 0;
            std::vector<GradeSquareState> squares;
            std::vector<int> activeAddSkillIds;
            nlohmann::json rewardsJson;
        };
        GradeQuestState gradeQuest;

        std::optional<MockStoredResponse> QuestStageSelectLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestStageDataLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestGetLiveSettingLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSetLiveSettingLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSetStartLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestGetLiveInfoLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSetFinishLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestGetResultLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> QuestSkipLocked(std::string_view payloadJson);

        std::optional<MockStoredResponse> DailyQuestStageListLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> DailyQuestStageDataLocked(std::string_view payloadJson);

        std::optional<MockStoredResponse> MusicLearningGetMusicSelectLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> MusicLearningGetResultLocked(std::string_view payloadJson);

        std::optional<MockStoredResponse> DreamNotifyMemberReleaseConfirmLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> DreamGetResultLocked(std::string_view payloadJson);

        std::optional<MockStoredResponse> GradeGetQuestListLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> GradeSetQuestStartLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> GradeSetQuestActionLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> GradeSetQuestAddSkillLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> GradeGetStageDataLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> GradeGetResultLocked(std::string_view payloadJson);
        std::optional<MockStoredResponse> GradeSetQuestRetireLocked(std::string_view payloadJson);

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
            ExecSql(db, "DELETE FROM daily_quest_stage;");
            ExecSql(db, "DELETE FROM dream_quest_stage;");
            ExecSql(db, "DELETE FROM grade_quest_season;");
            ExecSql(db, "DELETE FROM grade_quest_series;");
            ExecSql(db, "DELETE FROM grade_quest_stage;");
            ExecSql(db, "DELETE FROM grade_add_skill;");
            ExecSql(db, "DELETE FROM grade_quest_progress;");
            ExecSql(db, "DELETE FROM learning_stage;");
            ExecSql(db, "DELETE FROM music_mastery;");
            ExecSql(db, "DELETE FROM music;");
            std::filesystem::remove(resetCmdPath, ec);
        }

        if ((hasPendingReset || !TableHasAnyRows(db, "archive_detail") || !TableHasAnyRows(db, "card_detail") || !TableHasAnyRows(db, "character_info") || !TableHasAnyRows(db, "item") || !TableHasAnyRows(db, "quest_stage") || !TableHasAnyRows(db, "daily_quest_stage") || !TableHasAnyRows(db, "dream_quest_stage") || !TableHasAnyRows(db, "grade_quest_stage") || !TableHasAnyRows(db, "grade_add_skill") || !TableHasAnyRows(db, "learning_stage") || !TableHasAnyRows(db, "music_mastery") || !TableHasAnyRows(db, "music"))
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

        if (!ExecSql(db, "DELETE FROM archive_detail;") || !ExecSql(db, "DELETE FROM card_detail;") || !ExecSql(db, "DELETE FROM character_info;") || !ExecSql(db, "DELETE FROM item;") || !ExecSql(db, "DELETE FROM deck;") || !ExecSql(db, "DELETE FROM rhythm_music_score;") || !ExecSql(db, "DELETE FROM rhythm_game_deck;") || !ExecSql(db, "DELETE FROM quest_stage;") || !ExecSql(db, "DELETE FROM daily_quest_stage;") || !ExecSql(db, "DELETE FROM dream_quest_stage;") || !ExecSql(db, "DELETE FROM grade_quest_season;") || !ExecSql(db, "DELETE FROM grade_quest_series;") || !ExecSql(db, "DELETE FROM grade_quest_stage;") || !ExecSql(db, "DELETE FROM grade_add_skill;") || !ExecSql(db, "DELETE FROM grade_quest_progress;") || !ExecSql(db, "DELETE FROM learning_stage;") || !ExecSql(db, "DELETE FROM music_mastery;") || !ExecSql(db, "DELETE FROM music;")) {
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
            std::string aceCard;
            if (deckEntry.contains("ace_card")) {
                aceCard = deckEntry.value("ace_card", std::string{});
            } else {
                sqlite3_stmt* aceStmt = nullptr;
                constexpr const char* aceSql = "SELECT ace_card FROM deck WHERE d_deck_datas_id = ?;";
                if (sqlite3_prepare_v2(db, aceSql, -1, &aceStmt, nullptr) == SQLITE_OK && aceStmt) {
                    BindText(aceStmt, 1, deckId);
                    if (sqlite3_step(aceStmt) == SQLITE_ROW) {
                        const auto* val = reinterpret_cast<const char*>(sqlite3_column_text(aceStmt, 0));
                        if (val) aceCard = val;
                    }
                    sqlite3_finalize(aceStmt);
                }
            }
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
                    "SELECT music_id FROM music WHERE generations_id = ? AND has_score = 1 ORDER BY music_id;";
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
        bool foundStage = false;
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT music_id, quest_musics_type FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    musicId = sqlite3_column_int(stmt, 0);
                    questMusicsType = sqlite3_column_int(stmt, 1);
                    foundStage = true;
                }
                sqlite3_finalize(stmt);
            }
        }
        if (!foundStage) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT quest_musics_detail, quest_musics_type FROM daily_quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    musicId = sqlite3_column_int(stmt, 0);
                    questMusicsType = sqlite3_column_int(stmt, 1);
                    foundStage = true;
                }
                sqlite3_finalize(stmt);
            }
        }
        if (!foundStage) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT quest_musics_detail, quest_musics_type FROM dream_quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    musicId = sqlite3_column_int(stmt, 0);
                    questMusicsType = sqlite3_column_int(stmt, 1);
                    foundStage = true;
                }
                sqlite3_finalize(stmt);
            }
        }
        if (!foundStage) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT quest_musics_detail, quest_musics_type FROM grade_quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    musicId = sqlite3_column_int(stmt, 0);
                    questMusicsType = sqlite3_column_int(stmt, 1);
                    foundStage = true;
                }
                sqlite3_finalize(stmt);
            }
        }
        if (!foundStage) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT music_id FROM learning_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    musicId = sqlite3_column_int(stmt, 0);
                    questMusicsType = 2;
                    foundStage = true;
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json deckList = nlohmann::json::array();
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT d_deck_datas_id, deck_name, deck_no, generations_id, ace_card, deck_cards_json "
                "FROM deck ORDER BY deck_no;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
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
        } else if (questMusicsType == 3 && musicId > 0) {
            sqlite3_stmt* mStmt = nullptr;
            constexpr const char* mSql =
                "SELECT music_id FROM music WHERE center_character_id = ? ORDER BY music_id;";
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
        } else if (questMusicsType == 0 && musicId > 0) {
            sqlite3_stmt* mStmt = nullptr;
            constexpr const char* mSql =
                "SELECT music_id FROM music WHERE generations_id = ? AND has_score = 1 ORDER BY music_id;";
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
        } else if (questMusicsType == 0 && musicId == 0) {
            sqlite3_stmt* mStmt = nullptr;
            constexpr const char* mSql = "SELECT music_id FROM music WHERE has_score = 1 ORDER BY music_id;";
            if (sqlite3_prepare_v2(db, mSql, -1, &mStmt, nullptr) == SQLITE_OK && mStmt) {
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
        const int resourceValue = payload.value("resource_value", 0);

        if (questLiveType == 4 && gradeQuest.active) {
            gradeQuest.resourceValue = resourceValue;
        }

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
            bool foundSections = false;
            sqlite3_stmt* skStmt = nullptr;
            constexpr const char* skSql = "SELECT section_skills_json FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, skSql, -1, &skStmt, nullptr) == SQLITE_OK && skStmt) {
                sqlite3_bind_int(skStmt, 1, stageId);
                if (sqlite3_step(skStmt) == SQLITE_ROW) {
                    const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(skStmt, 0));
                    auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                    if (parsed.is_array()) {
                        sectionSkillList = std::move(parsed);
                        foundSections = true;
                    }
                }
                sqlite3_finalize(skStmt);
            }
            if (!foundSections) {
                sqlite3_stmt* dStmt = nullptr;
                constexpr const char* dSql = "SELECT section_skills_json FROM daily_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, dSql, -1, &dStmt, nullptr) == SQLITE_OK && dStmt) {
                    sqlite3_bind_int(dStmt, 1, stageId);
                    if (sqlite3_step(dStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(dStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                            foundSections = true;
                        }
                    }
                    sqlite3_finalize(dStmt);
                }
            }
            if (!foundSections) {
                sqlite3_stmt* drStmt = nullptr;
                constexpr const char* drSql = "SELECT section_skills_json FROM dream_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, drSql, -1, &drStmt, nullptr) == SQLITE_OK && drStmt) {
                    sqlite3_bind_int(drStmt, 1, stageId);
                    if (sqlite3_step(drStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(drStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                            foundSections = true;
                        }
                    }
                    sqlite3_finalize(drStmt);
                }
            }
            if (!foundSections) {
                sqlite3_stmt* grStmt = nullptr;
                constexpr const char* grSql = "SELECT section_skills_json FROM grade_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, grSql, -1, &grStmt, nullptr) == SQLITE_OK && grStmt) {
                    sqlite3_bind_int(grStmt, 1, stageId);
                    if (sqlite3_step(grStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(grStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                            foundSections = true;
                        }
                    }
                    sqlite3_finalize(grStmt);
                }
            }
            if (!foundSections) {
                sqlite3_stmt* lStmt = nullptr;
                constexpr const char* lSql = "SELECT section_skills_json FROM learning_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, lSql, -1, &lStmt, nullptr) == SQLITE_OK && lStmt) {
                    sqlite3_bind_int(lStmt, 1, stageId);
                    if (sqlite3_step(lStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(lStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                        }
                    }
                    sqlite3_finalize(lStmt);
                }
            }
        }

        nlohmann::json fanLevelInfoList = nlohmann::json::array();
        {
            const auto& profileObj = OfflineApiMockBuiltIn::HomeGetHomeJsonObj();
            if (profileObj.contains("profile_info") && profileObj["profile_info"].contains("fan_level_list")) {
                for (const auto& fl : profileObj["profile_info"]["fan_level_list"]) {
                    fanLevelInfoList.push_back({
                        {"character_id", fl.value("character_id", 0)},
                        {"member_fan_level", fl.value("member_fan_level", 0)},
                    });
                }
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
        result["grade_retry_count"] = (questLiveType == 4) ? 1 : 0;
        {
            nlohmann::json gradeSkills = nlohmann::json::array();
            if (questLiveType == 4 && gradeQuest.active) {
                for (int id : gradeQuest.activeAddSkillIds) {
                    gradeSkills.push_back(id);
                }
            }
            result["grade_add_skill_list"] = std::move(gradeSkills);
        }
        result["playable_count"] = 0;
        result["play_count"] = 0;
        result["fan_level_info_list"] = std::move(fanLevelInfoList);

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
            bool foundSections = false;
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT section_skills_json FROM quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, currentQuestLive.stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                    if (parsed.is_array()) {
                        sectionSkillList = std::move(parsed);
                        foundSections = true;
                    }
                }
                sqlite3_finalize(stmt);
            }
            if (!foundSections) {
                sqlite3_stmt* dStmt = nullptr;
                constexpr const char* dSql = "SELECT section_skills_json FROM daily_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, dSql, -1, &dStmt, nullptr) == SQLITE_OK && dStmt) {
                    sqlite3_bind_int(dStmt, 1, currentQuestLive.stageId);
                    if (sqlite3_step(dStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(dStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                            foundSections = true;
                        }
                    }
                    sqlite3_finalize(dStmt);
                }
            }
            if (!foundSections) {
                sqlite3_stmt* drStmt = nullptr;
                constexpr const char* drSql = "SELECT section_skills_json FROM dream_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, drSql, -1, &drStmt, nullptr) == SQLITE_OK && drStmt) {
                    sqlite3_bind_int(drStmt, 1, currentQuestLive.stageId);
                    if (sqlite3_step(drStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(drStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                            foundSections = true;
                        }
                    }
                    sqlite3_finalize(drStmt);
                }
            }
            if (!foundSections) {
                sqlite3_stmt* grStmt = nullptr;
                constexpr const char* grSql = "SELECT section_skills_json FROM grade_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, grSql, -1, &grStmt, nullptr) == SQLITE_OK && grStmt) {
                    sqlite3_bind_int(grStmt, 1, currentQuestLive.stageId);
                    if (sqlite3_step(grStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(grStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                            foundSections = true;
                        }
                    }
                    sqlite3_finalize(grStmt);
                }
            }
            if (!foundSections) {
                sqlite3_stmt* lStmt = nullptr;
                constexpr const char* lSql = "SELECT section_skills_json FROM learning_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, lSql, -1, &lStmt, nullptr) == SQLITE_OK && lStmt) {
                    sqlite3_bind_int(lStmt, 1, currentQuestLive.stageId);
                    if (sqlite3_step(lStmt) == SQLITE_ROW) {
                        const auto* json = reinterpret_cast<const char*>(sqlite3_column_text(lStmt, 0));
                        auto parsed = nlohmann::json::parse(json ? json : "[]", nullptr, false);
                        if (parsed.is_array()) {
                            sectionSkillList = std::move(parsed);
                        }
                    }
                    sqlite3_finalize(lStmt);
                }
            }
        }

        nlohmann::json fanLevelInfoList = nlohmann::json::array();
        {
            const auto& profileObj = OfflineApiMockBuiltIn::HomeGetHomeJsonObj();
            if (profileObj.contains("profile_info") && profileObj["profile_info"].contains("fan_level_list")) {
                for (const auto& fl : profileObj["profile_info"]["fan_level_list"]) {
                    fanLevelInfoList.push_back({
                        {"character_id", fl.value("character_id", 0)},
                        {"member_fan_level", fl.value("member_fan_level", 0)},
                    });
                }
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
        result["fan_level_info_list"] = std::move(fanLevelInfoList);

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::QuestSetFinishLocked(std::string_view payloadJson) {
        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (payload.is_object()) {
            currentQuestLive.score = payload.value("score", int64_t(0));
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
            if (gainStylePoint == 0) {
                sqlite3_stmt* dStmt = nullptr;
                constexpr const char* dSql = "SELECT gain_style_point FROM daily_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, dSql, -1, &dStmt, nullptr) == SQLITE_OK && dStmt) {
                    sqlite3_bind_int(dStmt, 1, currentQuestLive.stageId);
                    if (sqlite3_step(dStmt) == SQLITE_ROW) {
                        gainStylePoint = sqlite3_column_int(dStmt, 0);
                    }
                    sqlite3_finalize(dStmt);
                }
            }
            if (gainStylePoint == 0) {
                sqlite3_stmt* lStmt = nullptr;
                constexpr const char* lSql = "SELECT gain_style_point FROM learning_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, lSql, -1, &lStmt, nullptr) == SQLITE_OK && lStmt) {
                    sqlite3_bind_int(lStmt, 1, currentQuestLive.stageId);
                    if (sqlite3_step(lStmt) == SQLITE_ROW) {
                        gainStylePoint = sqlite3_column_int(lStmt, 0);
                    }
                    sqlite3_finalize(lStmt);
                }
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
            if (gainStylePoint == 0) {
                sqlite3_stmt* dStmt = nullptr;
                constexpr const char* dSql = "SELECT gain_style_point FROM daily_quest_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, dSql, -1, &dStmt, nullptr) == SQLITE_OK && dStmt) {
                    sqlite3_bind_int(dStmt, 1, stageId);
                    if (sqlite3_step(dStmt) == SQLITE_ROW) {
                        gainStylePoint = sqlite3_column_int(dStmt, 0);
                    }
                    sqlite3_finalize(dStmt);
                }
            }
            if (gainStylePoint == 0) {
                sqlite3_stmt* lStmt = nullptr;
                constexpr const char* lSql = "SELECT gain_style_point FROM learning_stage WHERE stage_id = ?;";
                if (sqlite3_prepare_v2(db, lSql, -1, &lStmt, nullptr) == SQLITE_OK && lStmt) {
                    sqlite3_bind_int(lStmt, 1, stageId);
                    if (sqlite3_step(lStmt) == SQLITE_ROW) {
                        gainStylePoint = sqlite3_column_int(lStmt, 0);
                    }
                    sqlite3_finalize(lStmt);
                }
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
        if (questLiveType == 3) {
            int masteryLevel = 50;
            int earnedExp = 250300;
            if (db && stageId > 0) {
                sqlite3_stmt* mStmt = nullptr;
                constexpr const char* mSql =
                    "SELECT mm.music_exp_level, mm.earned_music_exp "
                    "FROM learning_stage ls JOIN music_mastery mm ON ls.music_id = mm.music_id "
                    "WHERE ls.stage_id = ?;";
                if (sqlite3_prepare_v2(db, mSql, -1, &mStmt, nullptr) == SQLITE_OK && mStmt) {
                    sqlite3_bind_int(mStmt, 1, stageId);
                    if (sqlite3_step(mStmt) == SQLITE_ROW) {
                        masteryLevel = sqlite3_column_int(mStmt, 0);
                        earnedExp = sqlite3_column_int(mStmt, 1);
                    }
                    sqlite3_finalize(mStmt);
                }
            }
            result["mastery_level_before"] = masteryLevel;
            result["mastery_level_after"] = masteryLevel;
            result["mastery_level_experience"] = 0;
            result["mastery_level_total_experience_before"] = earnedExp;
        } else {
            result["mastery_level_before"] = 1;
            result["mastery_level_after"] = 1;
            result["mastery_level_experience"] = 0;
            result["mastery_level_total_experience_before"] = 0;
        }
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

    std::optional<MockStoredResponse> HttpMockBackend::Impl::DailyQuestStageListLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int questId = payload.is_object() ? payload.value("quest_id", 0) : 0;
        if (questId == 0) {
            return std::nullopt;
        }

        nlohmann::json stageList = nlohmann::json::array();
        sqlite3_stmt* stmt = nullptr;
        constexpr const char* sql =
            "SELECT stage_id FROM daily_quest_stage WHERE series_id = ? ORDER BY stage_id;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
            sqlite3_bind_int(stmt, 1, questId);
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
        result["user_daily_ticket_info"] = {
            {"num", 3},
            {"max", 3},
            {"next_reset_time", "2099-01-01T04:00:00+09:00"},
        };
        result["stage_list"] = std::move(stageList);

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::DailyQuestStageDataLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int stageId = payload.is_object() ? payload.value("stage_id", 0) : 0;
        if (stageId == 0) {
            return std::nullopt;
        }

        int score3Val = 0;
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT score3 FROM daily_quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    score3Val = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json musicList = nlohmann::json::array();
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT music_id FROM music WHERE has_score = 1 ORDER BY music_id;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    musicList.push_back({
                        {"m_musics_id", sqlite3_column_int(stmt, 0)},
                        {"is_enable", true},
                    });
                }
                sqlite3_finalize(stmt);
            }
        }

        int bestLoveMusicId = 0;
        if (!musicList.empty()) {
            bestLoveMusicId = musicList[0].value("m_musics_id", 0);
        }

        nlohmann::json result;
        result["stage_id"] = stageId;
        result["clear_status"] = 3;
        result["best_love_music_id"] = bestLoveMusicId;
        result["user_daily_ticket_info"] = {
            {"num", 3},
            {"max", 3},
            {"next_reset_time", "2099-01-01T04:00:00+09:00"},
        };
        result["stage_reward_list"] = nlohmann::json::array();
        result["music_list"] = std::move(musicList);

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::MusicLearningGetMusicSelectLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        nlohmann::json musicList = nlohmann::json::array();
        {
            sqlite3_stmt* seriesStmt = nullptr;
            constexpr const char* seriesSql =
                "SELECT ls.series_id, ls.music_id, "
                "COALESCE(mm.music_exp_level, 50), COALESCE(mm.earned_music_exp, 250300), COALESCE(mm.is_mastery, 1) "
                "FROM learning_stage ls "
                "LEFT JOIN music_mastery mm ON ls.music_id = mm.music_id "
                "GROUP BY ls.series_id ORDER BY ls.series_id;";
            if (sqlite3_prepare_v2(db, seriesSql, -1, &seriesStmt, nullptr) == SQLITE_OK && seriesStmt) {
                while (sqlite3_step(seriesStmt) == SQLITE_ROW) {
                    const int seriesId = sqlite3_column_int(seriesStmt, 0);
                    const int musicId = sqlite3_column_int(seriesStmt, 1);
                    const int musicExpLevel = sqlite3_column_int(seriesStmt, 2);
                    const int earnedMusicExp = sqlite3_column_int(seriesStmt, 3);
                    const bool isMastery = sqlite3_column_int(seriesStmt, 4) != 0;

                    nlohmann::json stageList = nlohmann::json::array();
                    sqlite3_stmt* stStmt = nullptr;
                    constexpr const char* stSql =
                        "SELECT stage_id, quest_rank, quest_level, gain_music_exp FROM learning_stage "
                        "WHERE series_id = ? ORDER BY quest_rank;";
                    if (sqlite3_prepare_v2(db, stSql, -1, &stStmt, nullptr) == SQLITE_OK && stStmt) {
                        sqlite3_bind_int(stStmt, 1, seriesId);
                        while (sqlite3_step(stStmt) == SQLITE_ROW) {
                            const int stageId = sqlite3_column_int(stStmt, 0);
                            const int questRank = sqlite3_column_int(stStmt, 1);
                            const int questLevel = sqlite3_column_int(stStmt, 2);
                            const int gainMusicExp = sqlite3_column_int(stStmt, 3);
                            stageList.push_back({
                                {"learning_live_stages_id", stageId},
                                {"quest_rank", questRank},
                                {"quest_level", questLevel},
                                {"is_lock", false},
                                {"is_clear", true},
                                {"gain_music_exp", gainMusicExp},
                                {"page", questRank <= 4 ? 1 : 2},
                            });
                        }
                        sqlite3_finalize(stStmt);
                    }

                    musicList.push_back({
                        {"learning_live_series_id", seriesId},
                        {"music_id", musicId},
                        {"earned_music_exp", earnedMusicExp},
                        {"music_exp_level", musicExpLevel},
                        {"is_mastery", isMastery},
                        {"stage_list", std::move(stageList)},
                    });
                }
                sqlite3_finalize(seriesStmt);
            }
        }

        nlohmann::json characterBonusList = nlohmann::json::array();
        {
            constexpr int characterIds[] = {1011, 1020, 1021, 1022, 1023, 1030, 1031, 1032, 1033, 1041, 1042, 1043, 1044, 1051, 1052};
            for (int charId : characterIds) {
                characterBonusList.push_back({
                    {"character_id", charId},
                    {"music_mastery_bonus", 100},
                    {"love_correction_value", 100},
                    {"music_mastery_bonus_list", nlohmann::json::array()},
                    {"season_fan_level", 0},
                });
            }
        }

        const int musicCount = static_cast<int>(musicList.size());
        nlohmann::json result;
        result["music_list"] = std::move(musicList);
        result["music_point"] = musicCount;
        result["latest_music_id"] = 0;
        result["character_bonus_list"] = std::move(characterBonusList);

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::MusicLearningGetResultLocked(std::string_view payloadJson) {
        int masteryLevel = 50;
        int earnedMusicExp = 250300;
        bool isMastery = true;
        if (db && currentQuestLive.musicId > 0) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT music_exp_level, earned_music_exp, is_mastery FROM music_mastery WHERE music_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, currentQuestLive.musicId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    masteryLevel = sqlite3_column_int(stmt, 0);
                    earnedMusicExp = sqlite3_column_int(stmt, 1);
                    isMastery = sqlite3_column_int(stmt, 2) != 0;
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["stage_id"] = currentQuestLive.stageId;
        result["quest_live_type"] = 3;
        result["is_clear"] = true;
        result["is_mastery"] = isMastery;
        result["result_love"] = currentQuestLive.score;
        result["best_love"] = currentQuestLive.score;
        result["before_best_love"] = 0;
        result["music_id"] = currentQuestLive.musicId;
        result["play_report"] = currentQuestLive.playReport;
        result["mastary_level_before"] = masteryLevel;
        result["mastary_level_after"] = masteryLevel;
        result["mastary_level_experience"] = 0;
        result["mastary_level_total_experience_before"] = earnedMusicExp;
        result["style_point"] = 200;
        result["before_earned_music_exp"] = 0;
        result["earned_music_exp"] = 0;
        result["first_clear_flag"] = false;
        result["is_limit_over_style_point"] = false;
        result["is_ex_stage_open"] = true;

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::DreamNotifyMemberReleaseConfirmLocked(std::string_view payloadJson) {
        MockStoredResponse response;
        response.body = "null";
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::DreamGetResultLocked(std::string_view payloadJson) {
        nlohmann::json result;
        result["stage_id"] = currentQuestLive.stageId;
        result["quest_live_type"] = 5;
        result["is_clear"] = true;
        result["result_love"] = currentQuestLive.score;
        result["get_card_id"] = 0;
        result["music_id"] = currentQuestLive.musicId;
        result["play_report"] = currentQuestLive.playReport;
        result["first_clear_flag"] = true;

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GradeGetQuestListLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        std::unordered_map<int, int> progressMap;
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT series_id, clear_status FROM grade_quest_progress;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    progressMap[sqlite3_column_int(stmt, 0)] = sqlite3_column_int(stmt, 1);
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json questSeasonList = nlohmann::json::array();
        {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT season_id, generation, season, order_id FROM grade_quest_season ORDER BY order_id;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const int seasonId = sqlite3_column_int(stmt, 0);

                    nlohmann::json seriesList = nlohmann::json::array();
                    int clearNum = 0;
                    bool prevCleared = false;
                    bool isFirstSeries = true;
                    sqlite3_stmt* sStmt = nullptr;
                    constexpr const char* sSql =
                        "SELECT series_id, order_id, rewards_json FROM grade_quest_series "
                        "WHERE season_id = ? ORDER BY order_id;";
                    if (sqlite3_prepare_v2(db, sSql, -1, &sStmt, nullptr) == SQLITE_OK && sStmt) {
                        sqlite3_bind_int(sStmt, 1, seasonId);
                        while (sqlite3_step(sStmt) == SQLITE_ROW) {
                            const int seriesId = sqlite3_column_int(sStmt, 0);
                            const int orderId = sqlite3_column_int(sStmt, 1);
                            const auto* rwJson = reinterpret_cast<const char*>(sqlite3_column_text(sStmt, 2));

                            auto pit = progressMap.find(seriesId);
                            const int clearStatus = (pit != progressMap.end()) ? pit->second : 0;
                            const bool cleared = (clearStatus > 0);

                            if (isFirstSeries) {
                                prevCleared = (orderId == 1);
                                isFirstSeries = false;
                            }

                            const bool isLock = !prevCleared;

                            if (cleared) clearNum++;
                            prevCleared = cleared;

                            nlohmann::json rewardList = nlohmann::json::array();
                            auto rp = nlohmann::json::parse(rwJson ? rwJson : "[]", nullptr, false);
                            if (rp.is_array()) {
                                for (const auto& r : rp) {
                                    rewardList.push_back({
                                        {"grade_quest_rewards_id", r.value("grade_quest_rewards_id", 0)},
                                        {"is_received", cleared},
                                    });
                                }
                            }

                            seriesList.push_back({
                                {"grade_quest_series_id", seriesId},
                                {"play_status", 0},
                                {"is_lock", isLock},
                                {"clear_status", clearStatus},
                                {"reward_list", std::move(rewardList)},
                            });
                        }
                        sqlite3_finalize(sStmt);
                    }

                    const bool seasonLock = (clearNum == 0 && !seriesList.empty() &&
                                             seriesList[0].value("is_lock", false));
                    questSeasonList.push_back({
                        {"grade_quest_season_id", seasonId},
                        {"play_status", 0},
                        {"is_lock", seasonLock},
                        {"clear_num", clearNum},
                        {"quest_series_list", std::move(seriesList)},
                    });
                }
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["quest_season_list"] = std::move(questSeasonList);
        result["point_bonus_list"] = nlohmann::json::array();
        result["is_update_grade_live"] = false;

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GradeSetQuestStartLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int seriesId = payload.is_object() ? payload.value("grade_quest_series_id", 0) : 0;

        if (!gradeQuest.active || gradeQuest.seriesId != seriesId) {
            gradeQuest = {};
            gradeQuest.active = true;
            gradeQuest.seriesId = seriesId;

            nlohmann::json squaresParsed = nlohmann::json::array();
            {
                sqlite3_stmt* stmt = nullptr;
                constexpr const char* sql =
                    "SELECT default_action_point, squares_json, rewards_json FROM grade_quest_series WHERE series_id = ?;";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                    sqlite3_bind_int(stmt, 1, seriesId);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        gradeQuest.actionPoint = sqlite3_column_int(stmt, 0);
                        const auto* sqJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                        auto sp = nlohmann::json::parse(sqJson ? sqJson : "[]", nullptr, false);
                        if (sp.is_array()) squaresParsed = std::move(sp);
                        const auto* rwJson = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                        auto rp = nlohmann::json::parse(rwJson ? rwJson : "[]", nullptr, false);
                        if (rp.is_array()) gradeQuest.rewardsJson = std::move(rp);
                    }
                    sqlite3_finalize(stmt);
                }
            }
            {
                const int seasonId = seriesId / 10;
                sqlite3_stmt* stmt = nullptr;
                constexpr const char* sql = "SELECT generation, season FROM grade_quest_season WHERE season_id = ?;";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                    sqlite3_bind_int(stmt, 1, seasonId);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        gradeQuest.generation = sqlite3_column_int(stmt, 0);
                        gradeQuest.season = sqlite3_column_int(stmt, 1);
                        gradeQuest.characterId = gradeQuest.generation * 10 + 1;
                    }
                    sqlite3_finalize(stmt);
                }
            }

            for (const auto& sq : squaresParsed) {
                GradeSquareState ss;
                ss.squareId = sq.value("grade_quest_square_id", 0);
                ss.squareType = sq.value("square_type", 0);
                ss.targetId = sq.value("target_id", 0);
                ss.actionPointCost = sq.value("min_action_point", 0);
                if (sq.contains("open_square_ids") && sq["open_square_ids"].is_array()) {
                    for (const auto& dep : sq["open_square_ids"]) {
                        if (dep.is_number_integer()) {
                            ss.openSquareIds.push_back(dep.get<int>());
                        }
                    }
                }
                if (ss.squareType == 1) {
                    ss.status = 2;
                    gradeQuest.currentSquareId = ss.squareId;
                } else {
                    ss.status = 0;
                }
                gradeQuest.squares.push_back(ss);
            }

            for (auto& sq : gradeQuest.squares) {
                if (sq.status != 0 || sq.openSquareIds.empty()) continue;
                bool allDepsMet = true;
                for (int depId : sq.openSquareIds) {
                    bool found = false;
                    for (const auto& other : gradeQuest.squares) {
                        if (other.squareId == depId && other.status == 2) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) { allDepsMet = false; break; }
                }
                if (allDepsMet) {
                    sq.status = 1;
                }
            }
        }

        nlohmann::json questSquareList = nlohmann::json::array();
        for (const auto& sq : gradeQuest.squares) {
            questSquareList.push_back({
                {"grade_quest_square_id", sq.squareId},
                {"status", sq.status},
                {"live_point", sq.livePoint},
            });
        }

        nlohmann::json rewardList = nlohmann::json::array();
        if (gradeQuest.rewardsJson.is_array()) {
            for (const auto& r : gradeQuest.rewardsJson) {
                rewardList.push_back({
                    {"grade_quest_rewards_id", r.value("grade_quest_rewards_id", 0)},
                    {"is_received", false},
                });
            }
        }

        nlohmann::json activeSkills = nlohmann::json::array();
        for (int id : gradeQuest.activeAddSkillIds) {
            activeSkills.push_back(id);
        }

        nlohmann::json result;
        result["character_id"] = gradeQuest.characterId;
        result["current_square_id"] = gradeQuest.currentSquareId;
        result["action_point"] = gradeQuest.actionPoint;
        result["quest_square_list"] = std::move(questSquareList);
        result["active_add_skill_id_list"] = std::move(activeSkills);
        result["lot_add_skill_id_list"] = nlohmann::json::array();
        result["reward_list"] = std::move(rewardList);
        {
            int bonusCount = 0;
            if (db) {
                sqlite3_stmt* bStmt = nullptr;
                constexpr const char* bSql =
                    "SELECT COUNT(*) FROM grade_quest_progress p "
                    "JOIN grade_quest_series sr ON sr.series_id = p.series_id "
                    "JOIN grade_quest_season s ON s.season_id = sr.season_id "
                    "WHERE p.bonus_cleared = 1 AND s.generation = ?;";
                if (sqlite3_prepare_v2(db, bSql, -1, &bStmt, nullptr) == SQLITE_OK && bStmt) {
                    sqlite3_bind_int(bStmt, 1, gradeQuest.generation);
                    if (sqlite3_step(bStmt) == SQLITE_ROW) {
                        bonusCount = sqlite3_column_int(bStmt, 0);
                    }
                    sqlite3_finalize(bStmt);
                }
            }
            int globalClearedCount = 0;
            {
                sqlite3_stmt* gcStmt = nullptr;
                constexpr const char* gcSql = "SELECT COUNT(*) FROM grade_quest_progress WHERE clear_status > 0;";
                if (sqlite3_prepare_v2(db, gcSql, -1, &gcStmt, nullptr) == SQLITE_OK && gcStmt) {
                    if (sqlite3_step(gcStmt) == SQLITE_ROW) {
                        globalClearedCount = sqlite3_column_int(gcStmt, 0);
                    }
                    sqlite3_finalize(gcStmt);
                }
            }
            const int gradeNum = 300 + globalClearedCount;
            nlohmann::json bonusList = nlohmann::json::array();
            if (bonusCount > 0) {
                bonusList.push_back({
                    {"bonus_type", 2}, {"target_detail", 2},
                    {"target_num", gradeQuest.generation}, {"bonus_value", bonusCount * 500},
                });
            }
            bonusList.push_back({
                {"bonus_type", 1}, {"target_detail", 0},
                {"target_num", gradeNum}, {"bonus_value", 10000},
            });
            result["point_bonus_list"] = std::move(bonusList);
        }

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GradeSetQuestActionLocked(std::string_view payloadJson) {
        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int squareId = payload.is_object() ? payload.value("grade_quest_square_id", 0) : 0;

        int squareType = 2;
        int targetId = 0;
        int actionCost = 0;
        for (auto& sq : gradeQuest.squares) {
            if (sq.squareId == squareId) {
                squareType = sq.squareType;
                targetId = sq.targetId;
                actionCost = sq.actionPointCost;
                sq.status = 2;
                gradeQuest.currentSquareId = squareId;
                break;
            }
        }

        gradeQuest.actionPoint = std::max(0, gradeQuest.actionPoint - actionCost);

        for (auto& sq : gradeQuest.squares) {
            if (sq.status != 0) continue;
            bool allDepsMet = true;
            for (int depId : sq.openSquareIds) {
                bool found = false;
                for (const auto& other : gradeQuest.squares) {
                    if (other.squareId == depId && other.status == 2) {
                        found = true;
                        break;
                    }
                }
                if (!found) { allDepsMet = false; break; }
            }
            if (allDepsMet && !sq.openSquareIds.empty()) {
                sq.status = 1;
            }
        }

        nlohmann::json result;
        result["action_point"] = gradeQuest.actionPoint;
        result["square_type"] = squareType;
        if (squareType == 4 && db) {
            int tier = (targetId >= 1 && targetId <= 3) ? targetId : 1;
            nlohmann::json skillList = nlohmann::json::array();
            sqlite3_stmt* stmt = nullptr;
            const char* sql = "SELECT skill_id FROM grade_add_skill WHERE tier = ? ORDER BY RANDOM() LIMIT 3";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, tier);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    skillList.push_back(sqlite3_column_int(stmt, 0));
                }
                sqlite3_finalize(stmt);
            }
            if (skillList.empty()) {
                skillList = nlohmann::json::array({41004050, 41009005, 41004100});
            }
            result["add_skill_id_list"] = skillList;
        } else {
            result["add_skill_id_list"] = nullptr;
        }

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GradeSetQuestAddSkillLocked(std::string_view payloadJson) {
        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        if (payload.is_object()) {
            const int skillId = payload.value("grade_add_skills_id", 0);
            if (skillId > 0) {
                gradeQuest.activeAddSkillIds.push_back(skillId);
            }
            const int squareId = payload.value("grade_quest_square_id", 0);
            for (auto& sq : gradeQuest.squares) {
                if (sq.squareId == squareId) {
                    sq.status = 2;
                    break;
                }
            }
        }

        nlohmann::json result;
        result["action_point"] = gradeQuest.actionPoint;

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GradeGetStageDataLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }
        if (!db) {
            return std::nullopt;
        }

        auto payload = nlohmann::json::parse(payloadJson.begin(), payloadJson.end(), nullptr, false);
        const int squareId = payload.is_object() ? payload.value("grade_quest_square_id", 0) : 0;

        int targetId = 0;
        int existingLivePoint = 0;
        for (const auto& sq : gradeQuest.squares) {
            if (sq.squareId == squareId) {
                targetId = sq.targetId;
                existingLivePoint = sq.livePoint;
                break;
            }
        }

        nlohmann::json musicList = nlohmann::json::array();
        if (targetId > 0) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql =
                "SELECT quest_musics_type, quest_musics_detail FROM grade_quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, targetId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    const int mType = sqlite3_column_int(stmt, 0);
                    const int mDetail = sqlite3_column_int(stmt, 1);
                    if (mType == 2 && mDetail > 0) {
                        musicList.push_back({{"m_musics_id", mDetail}, {"is_enable", true}});
                    } else if (mType == 0 && mDetail > 0) {
                        sqlite3_stmt* mStmt = nullptr;
                        constexpr const char* mSql =
                            "SELECT music_id FROM music WHERE generations_id = ? AND has_score = 1 ORDER BY music_id;";
                        if (sqlite3_prepare_v2(db, mSql, -1, &mStmt, nullptr) == SQLITE_OK && mStmt) {
                            sqlite3_bind_int(mStmt, 1, mDetail);
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
                sqlite3_finalize(stmt);
            }
        }

        nlohmann::json result;
        result["live_point"] = existingLivePoint;
        result["music_list"] = std::move(musicList);

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GradeGetResultLocked(std::string_view payloadJson) {
        if (!initialized && !EnsureReadyLocked()) {
            return std::nullopt;
        }

        const int64_t score = currentQuestLive.score;
        const int64_t livePoint = score / 1000;

        int64_t requiredLivePoint = 0;
        if (db) {
            sqlite3_stmt* stmt = nullptr;
            constexpr const char* sql = "SELECT live_point FROM grade_quest_stage WHERE stage_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK && stmt) {
                sqlite3_bind_int(stmt, 1, currentQuestLive.stageId);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    requiredLivePoint = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
        }

        gradeQuest.actionPoint = std::max(0, gradeQuest.actionPoint - gradeQuest.resourceValue);
        gradeQuest.resourceValue = 0;

        int64_t beforeLivePoint = 0;
        for (const auto& sq : gradeQuest.squares) {
            if (sq.targetId == currentQuestLive.stageId) {
                beforeLivePoint = sq.livePoint;
                break;
            }
        }
        const int64_t totalLivePoint = beforeLivePoint + livePoint;
        const bool questResult = (totalLivePoint >= requiredLivePoint);

        for (auto& sq : gradeQuest.squares) {
            if (sq.targetId == currentQuestLive.stageId) {
                sq.livePoint = totalLivePoint;
                if (questResult) {
                    sq.status = 2;
                    gradeQuest.currentSquareId = sq.squareId;
                }
                break;
            }
        }

        if (questResult) {
            for (auto& sq : gradeQuest.squares) {
                if (sq.status != 0) continue;
                bool allDepsMet = true;
                for (int depId : sq.openSquareIds) {
                    bool found = false;
                    for (const auto& other : gradeQuest.squares) {
                        if (other.squareId == depId && other.status == 2) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) { allDepsMet = false; break; }
                }
                if (allDepsMet && !sq.openSquareIds.empty()) {
                    sq.status = 1;
                }
            }
        }

        bool allCleared = true;
        for (const auto& sq : gradeQuest.squares) {
            if (sq.squareType == 7 && sq.status != 2) {
                allCleared = false;
                break;
            }
        }

        nlohmann::json result;
        result["quest_live_type"] = 4;
        result["quest_stage_id"] = currentQuestLive.stageId;
        result["quest_result"] = questResult;
        result["result_love"] = score;
        result["play_report"] = currentQuestLive.playReport;
        result["add_live_point"] = livePoint;
        result["before_live_point"] = beforeLivePoint;
        result["point_bonus_list"] = nlohmann::json::array();
        result["action_point"] = gradeQuest.actionPoint;

        if (allCleared) {
            bool hasBonusSquare = false;
            for (const auto& sq : gradeQuest.squares) {
                if (sq.squareType == 6 && sq.status == 2) {
                    hasBonusSquare = true;
                    break;
                }
            }
            if (db) {
                sqlite3_stmt* pStmt = nullptr;
                constexpr const char* pSql =
                    "INSERT OR REPLACE INTO grade_quest_progress (series_id, clear_status, bonus_cleared) "
                    "VALUES (?, 2, ?);";
                if (sqlite3_prepare_v2(db, pSql, -1, &pStmt, nullptr) == SQLITE_OK && pStmt) {
                    sqlite3_bind_int(pStmt, 1, gradeQuest.seriesId);
                    sqlite3_bind_int(pStmt, 2, hasBonusSquare ? 1 : 0);
                    sqlite3_step(pStmt);
                    sqlite3_finalize(pStmt);
                }
            }

            nlohmann::json goalSquares = nlohmann::json::array();
            for (const auto& sq : gradeQuest.squares) {
                goalSquares.push_back({
                    {"grade_quest_square_id", sq.squareId},
                    {"status", sq.status},
                    {"live_point", sq.livePoint},
                });
            }
            nlohmann::json rewardList = nlohmann::json::array();
            nlohmann::json clearRewardIds = nlohmann::json::array();
            if (gradeQuest.rewardsJson.is_array()) {
                for (const auto& r : gradeQuest.rewardsJson) {
                    const int rwId = r.value("grade_quest_rewards_id", 0);
                    clearRewardIds.push_back(rwId);
                    rewardList.push_back({
                        {"grade_quest_rewards_id", rwId},
                        {"is_received", true},
                    });
                }
            }
            int globalClearedCount = 0;
            if (db) {
                sqlite3_stmt* cStmt = nullptr;
                constexpr const char* cSql = "SELECT COUNT(*) FROM grade_quest_progress WHERE clear_status > 0;";
                if (sqlite3_prepare_v2(db, cSql, -1, &cStmt, nullptr) == SQLITE_OK && cStmt) {
                    if (sqlite3_step(cStmt) == SQLITE_ROW) {
                        globalClearedCount = sqlite3_column_int(cStmt, 0);
                    }
                    sqlite3_finalize(cStmt);
                }
            }
            const int afterGradeNum = 300 + globalClearedCount;
            result["goal_clear_data"] = {
                {"character_id", gradeQuest.characterId},
                {"current_square_id", gradeQuest.currentSquareId},
                {"quest_square_list", std::move(goalSquares)},
                {"clear_grade_quest_rewards_id_list", std::move(clearRewardIds)},
                {"reward_list", std::move(rewardList)},
                {"grade_up_data", {
                    {"before_grade_num", afterGradeNum - 1},
                    {"after_grade_num", afterGradeNum},
                }},
                {"is_not_received_grade_reward", false},
            };
            gradeQuest.active = false;
        } else {
            result["goal_clear_data"] = nullptr;
        }

        MockStoredResponse response;
        response.body = result.dump();
        return response;
    }

    std::optional<MockStoredResponse> HttpMockBackend::Impl::GradeSetQuestRetireLocked(std::string_view payloadJson) {
        nlohmann::json rewardList = nlohmann::json::array();
        if (gradeQuest.rewardsJson.is_array()) {
            for (const auto& r : gradeQuest.rewardsJson) {
                rewardList.push_back({
                    {"grade_quest_rewards_id", r.value("grade_quest_rewards_id", 0)},
                    {"is_received", false},
                });
            }
        }

        nlohmann::json result;
        result["clear_grade_quest_rewards_id_list"] = nlohmann::json::array();
        result["live_point"] = 0;
        result["reward_list"] = std::move(rewardList);
        result["grade_up_data"] = nullptr;

        gradeQuest = {};

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
                  ExecSql(db, "DELETE FROM daily_quest_stage;") &&
                  ExecSql(db, "DELETE FROM dream_quest_stage;") &&
                  ExecSql(db, "DELETE FROM grade_quest_season;") &&
                  ExecSql(db, "DELETE FROM grade_quest_series;") &&
                  ExecSql(db, "DELETE FROM grade_quest_stage;") &&
                  ExecSql(db, "DELETE FROM grade_add_skill;") &&
                  ExecSql(db, "DELETE FROM grade_quest_progress;") &&
                  ExecSql(db, "DELETE FROM learning_stage;") &&
                  ExecSql(db, "DELETE FROM music_mastery;") &&
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

    std::optional<MockStoredResponse> HttpMockBackend::DailyQuestStageList(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->DailyQuestStageListLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::DailyQuestStageData(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->DailyQuestStageDataLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::MusicLearningGetMusicSelect(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->MusicLearningGetMusicSelectLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::MusicLearningGetResult(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->MusicLearningGetResultLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::DreamNotifyMemberReleaseConfirm(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->DreamNotifyMemberReleaseConfirmLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::DreamGetResult(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->DreamGetResultLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GradeGetQuestList(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GradeGetQuestListLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GradeSetQuestStart(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GradeSetQuestStartLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GradeSetQuestAction(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GradeSetQuestActionLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GradeSetQuestAddSkill(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GradeSetQuestAddSkillLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GradeGetStageData(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GradeGetStageDataLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GradeGetResult(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GradeGetResultLocked(payloadJson);
    }

    std::optional<MockStoredResponse> HttpMockBackend::GradeSetQuestRetire(std::string_view payloadJson) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->GradeSetQuestRetireLocked(payloadJson);
    }

    std::string HttpMockBackend::ExtractPayloadIntegerFieldAsString(std::string_view payloadJson, std::string_view fieldName) {
        return ExtractJsonIntegerFieldAsString(payloadJson, fieldName);
    }
}
