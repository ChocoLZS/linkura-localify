#include "HttpMockBackend.hpp"

#include "../../HookMain.h"
#include "../../Local.h"
#include "http_mock_backend_builtin_sql.hpp"

#include <filesystem>
#include <mutex>
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
        std::optional<MockStoredResponse> GetCharacterInfoByIdLocked(std::string_view characterId);
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
            std::filesystem::remove(resetCmdPath, ec);
        }

        if ((hasPendingReset || !TableHasAnyRows(db, "archive_detail") || !TableHasAnyRows(db, "card_detail") || !TableHasAnyRows(db, "character_info"))
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

        if (!ExecSql(db, "DELETE FROM archive_detail;") || !ExecSql(db, "DELETE FROM card_detail;") || !ExecSql(db, "DELETE FROM character_info;")) {
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

    std::string HttpMockBackend::ExtractPayloadIntegerFieldAsString(std::string_view payloadJson, std::string_view fieldName) {
        return ExtractJsonIntegerFieldAsString(payloadJson, fieldName);
    }
}
