#include "HttpMockBackend.hpp"

#include "../../HookMain.h"
#include "../../Local.h"
#include "http_mock_backend_builtin_sql.hpp"
#include "offline_api_mock_builtin.hpp"

#include <filesystem>
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
            std::filesystem::remove(resetCmdPath, ec);
        }

        if ((hasPendingReset || !TableHasAnyRows(db, "archive_detail") || !TableHasAnyRows(db, "card_detail") || !TableHasAnyRows(db, "character_info") || !TableHasAnyRows(db, "item"))
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

        if (!ExecSql(db, "DELETE FROM archive_detail;") || !ExecSql(db, "DELETE FROM card_detail;") || !ExecSql(db, "DELETE FROM character_info;") || !ExecSql(db, "DELETE FROM item;") || !ExecSql(db, "DELETE FROM deck;")) {
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

    std::string HttpMockBackend::ExtractPayloadIntegerFieldAsString(std::string_view payloadJson, std::string_view fieldName) {
        return ExtractJsonIntegerFieldAsString(payloadJson, fieldName);
    }
}
