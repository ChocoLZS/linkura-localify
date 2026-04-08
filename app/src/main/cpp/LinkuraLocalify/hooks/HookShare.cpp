#include "../HookMain.h"
#include "../Misc.hpp"
#include "../Local.h"
#include <re2/re2.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace LinkuraLocal::HookShare {
    namespace Shareable {
        // memory leak ?
        std::unordered_map<std::string, ArchiveData> archiveData{};
        void* realtimeRenderingArchiveControllerCache = nullptr;
        float realtimeRenderingArchivePositionSeconds = 0;
        std::string currentArchiveId = "";
        float currentArchiveDuration = 0;
        RenderScene renderScene = RenderScene::None;
        SetPlayPosition_State setPlayPositionState = SetPlayPosition_State::Nothing;

        // Function implementations
        void resetRenderScene() {
            renderScene = RenderScene::None;
        }

        bool renderSceneIsNone() {
            return renderScene == RenderScene::None;
        }

        bool renderSceneIsFesLive() {
            return renderScene == RenderScene::FesLive;
        }

        bool renderSceneIsWithLive() {
            return renderScene == RenderScene::WithLive;
        }

        bool renderSceneIsStory() {
            return renderScene == RenderScene::Story;
        }
    }

    // URL替换函数：将external_link中的URL替换为assets_url
    std::string replaceUriHost(const std::string& uri, const std::string& assets_url) {
        // 使用RE2正则表达式匹配URL模式
        // 匹配 https://foo.example.org 这样的URL，并考虑路径部分
        std::string pattern = R"(^https://[^/]+(/.*)?$)";
        RE2 re(pattern);
        
        if (!re.ok()) {
            Log::WarnFmt("RE2 compile failed for pattern: %s, error: %s", pattern.c_str(), re.error().c_str());
            return uri;
        }
        
        std::string path_part;
        if (RE2::FullMatch(uri, re, &path_part)) {
            std::string result = assets_url;
            if (!path_part.empty()) {
                if (!assets_url.empty() && assets_url.back() == '/' && path_part.front() == '/') {
                    result += path_part.substr(1);
                } else if (!assets_url.empty() && assets_url.back() != '/' && path_part.front() != '/') {
                    result += "/" + path_part;
                } else {
                    result += path_part;
                }
            }
            Log::VerboseFmt("URL replaced: %s -> %s", uri.c_str(), result.c_str());
            return result;
        } else {
            std::string result = assets_url;
            if (!uri.empty()) {
                if (!assets_url.empty() && assets_url.back() == '/' && uri.front() == '/') {
                    result += uri.substr(1);
                } else if (!assets_url.empty() && assets_url.back() != '/' && uri.front() != '/') {
                    result += "/" + uri;
                } else {
                    result += uri;
                }
            }
            Log::VerboseFmt("Path combined with assets_url: %s -> %s", uri.c_str(), result.c_str());
            return result;
        }
    }

    bool isMotionCaptureCompatible(const std::string & url, const nlohmann::json& archive_config) {
        bool isCompatible = true;
        auto isAlsArchive = url.ends_with("md");
        isCompatible &= Config::isLegacyMrsVersion() ^ isAlsArchive;
        if (archive_config.contains("version_compatibility") && 
            !archive_config["version_compatibility"].is_null() && 
            archive_config["version_compatibility"].is_object()) {
            auto version_compatibility = archive_config["version_compatibility"];
//            Log::DebugFmt("version_compatibility is %s", version_compatibility.dump().c_str());
            if (version_compatibility.contains("rule") && !version_compatibility["rule"].is_null()) {
                std::string rule = version_compatibility["rule"].get<std::string>();
                isCompatible &= VersionCompatibility::VersionChecker(rule).checkCompatibility(Config::currentClientVersion);
            }
        }
        return isCompatible;
    }
#pragma region HttpRequests
    nlohmann::json handle_legacy_archive_data(nlohmann::json json) {
        json["live_timeline_ids"] = json["timeline_ids"];
        nlohmann::json character_ids = nlohmann::json::array();
        for (const auto& character : json["characters"]) {
            character_ids.push_back(character["character_id"]);
        }
        json["character_ids"] = character_ids;
//        json["_id"] = 23;
//        json["costume_ids"] = {3016};
        Log::VerboseFmt("%s", json.dump().c_str());
        if (Config::isLegacyMrsVersion()) {
            for (auto& chapter : json["chapters"]) {
                chapter["is_available"] = "true";
            }
        }
        return json;
    }

    void clear_json_arr(nlohmann::json& json, const std::string& key) {
        if (json.contains(key) && json[key].is_array()) {
            json[key].clear();
        }
    }
    nlohmann::json handle_get_with_archive_data(nlohmann::json json, bool is_legacy = false) {
        if (Config::withliveOrientation == (int)HookLiveRender::LiveScreenOrientation::Landscape) {
            json["is_horizontal"] = "true";
        }
        if (Config::withliveOrientation == (int)HookLiveRender::LiveScreenOrientation::Portrait) {
            json["is_horizontal"] = "false";
        }
        if (Config::unlockAfter) {
            json["has_extra_admission"] = "true";
        }
        if (Config::enableSetArchiveStartTime) {
            json["chapters"][0]["play_time_second"] = Config::archiveStartTime;
        }
        if (Config::enableMotionCaptureReplay) {
            auto archive_id = Shareable::currentArchiveId;
            auto it = Config::archiveConfigMap.find(archive_id);
            if (it == Config::archiveConfigMap.end()) return json;
            auto archive_config = it->second;
            auto replay_type = archive_config["replay_type"].get<uint>();
            auto external_link = archive_config.contains("external_link") ? archive_config["external_link"].get<std::string>() : "";
            auto external_fix_link = archive_config.contains("external_fix_link") ? archive_config["external_fix_link"].get<std::string>() : "";

            auto assets_url = Config::motionCaptureResourceUrl;
            if (replay_type == 0) {
                json.erase("archive_url");
            }
            if (replay_type == 1) {
                json.erase("video_url");
                if (!external_link.empty()) {
                    auto new_external_link = replaceUriHost(external_link, assets_url);
                    json["archive_url"] = new_external_link;
                }
            }
            if (replay_type == 2) {
                json.erase("video_url");
                if (!external_fix_link.empty()) {
                    auto new_external_fix_link = replaceUriHost(external_fix_link, assets_url);
                    json["archive_url"] = new_external_fix_link;
                }
            }
            clear_json_arr(json, "timelines");
            clear_json_arr(json, "gift_pt_rankings");
        }
        if (is_legacy) {
            json = handle_legacy_archive_data(json);
        }
        return json;
    }
    nlohmann::json handle_get_fes_archive_data(nlohmann::json json, bool is_legacy = false) {
        if (Config::unlockAfter) {
            json["has_extra_admission"] = "true";
        }
        if (Config::fesArchiveUnlockTicket) {
            json["selectable_camera_types"] = {1,2,3,4};
            json["ticket_rank"] = 6;
        }
        if (Config::enableSetArchiveStartTime) {
            json["chapters"][0]["play_time_second"] = Config::archiveStartTime;
        }
        if (Config::enableMotionCaptureReplay) {
            auto archive_id = Shareable::currentArchiveId;
            auto it = Config::archiveConfigMap.find(archive_id);
            if (it == Config::archiveConfigMap.end()) return json;
            auto archive_config = it->second;
            auto replay_type = archive_config["replay_type"].get<uint>();
            auto external_link = archive_config.contains("external_link") ? archive_config["external_link"].get<std::string>() : "";
            auto external_fix_link = archive_config.contains("external_fix_link") ? archive_config["external_fix_link"].get<std::string>() : "";
            auto assets_url = Config::motionCaptureResourceUrl;
            if (replay_type == 0) {
                json.erase("archive_url");
            }
            if (replay_type == 1) {
                json.erase("video_url");
                if (!external_link.empty()) {
                    auto new_external_link = replaceUriHost(external_link, assets_url);
                    json["archive_url"] = new_external_link;
                }
            }
            if (replay_type == 2) {
                json.erase("video_url");
                if (!external_fix_link.empty()) {
                    auto new_external_fix_link = replaceUriHost(external_fix_link, assets_url);
                    json["archive_url"] = new_external_fix_link;
                }
            }
            clear_json_arr(json, "timelines");
            clear_json_arr(json, "gift_pt_rankings");
        }
        if (is_legacy) {
            json = handle_legacy_archive_data(json);
        }
        return json;
    }
    bool filter_archive_by_rule(nlohmann::json archive) {
        if (!Config::enableMotionCaptureReplay || !Config::filterMotionCaptureReplay) return false;
        auto archive_id = archive["archives_id"].get<std::string>();
        auto it = Config::archiveConfigMap.find(archive_id);
        if (it == Config::archiveConfigMap.end()) {
            return true; // not found should be filtered
        }
        auto archive_config = it->second;

        /**
         * filter by simple replay type
         */
        auto replay_type = archive_config["replay_type"].get<uint>();
        if (replay_type == 0) return true;

        // apply rule

        // judge motion capture version is compatible with current client
        if (Config::filterPlayableMotionCapture) {
            if (replay_type == 1) {
                if (!isMotionCaptureCompatible(archive_config["external_link"].get<std::string>(), archive_config)) {
                    return true;
                }
            }
            if (replay_type == 2) {
                if (!isMotionCaptureCompatible(archive_config["external_fix_link"].get<std::string>(), archive_config)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool try_handle_get_archive_data(const std::string& archive_id) {
        std::string message = "The motion replay is not compatible for current client!";
        bool avoid_next = false;
        if (Config::enableMotionCaptureReplay && Config::avoidAccidentalTouch) {
            auto it = Config::archiveConfigMap.find(archive_id);
            if (it == Config::archiveConfigMap.end()) return false;
            auto archive_config = it->second;

            if (archive_config.contains("version_compatibility") &&
                  !archive_config["version_compatibility"].is_null() &&
                  archive_config["version_compatibility"].is_object()) {
                auto version_compatibility = archive_config["version_compatibility"];
                if (version_compatibility.contains("message") && !version_compatibility["message"].is_null()) {
                    message = version_compatibility["message"].get<std::string>();
                }
            }
            // client version is valid
            auto replay_type = archive_config["replay_type"].get<uint>();
            if (replay_type == 0) {
                return false;
            }
            if (replay_type == 1) {
                if (!isMotionCaptureCompatible(archive_config["external_link"].get<std::string>(), archive_config)) {
                    avoid_next = true;
                }
            }
            if (replay_type == 2) {
                if (!isMotionCaptureCompatible(archive_config["external_fix_link"].get<std::string>(), archive_config)) {
                    avoid_next = true;
                }
            }
        }
        if (avoid_next) {
            Log::ShowToast(message.c_str());
        }
        return avoid_next;
    }

    nlohmann::json handle_get_archive_list(nlohmann::json json, bool is_legacy = false) {
        auto& archive_list = json["archive_list"];
        archive_list.erase(
                std::remove_if(archive_list.begin(), archive_list.end(),
                               [](const nlohmann::json& archive) {
                                   return filter_archive_by_rule(archive);
                }),
                archive_list.end());
        for (auto& archive : archive_list) {
            auto archive_id = archive["archives_id"].get<std::string>();
            if (Config::unlockAfter) {
                archive["has_extra_admission"] = "true";
            }
            if (Shareable::archiveData.find(archive_id) == Shareable::archiveData.end()) {
                auto live_start_time = archive["live_start_time"].get<std::string>();
                auto live_end_time = archive["live_end_time"].get<std::string>();
                auto duration = LinkuraLocal::Misc::Time::parseISOTime(live_end_time) - LinkuraLocal::Misc::Time::parseISOTime(live_start_time);
                Shareable::archiveData[archive_id] = {
                        .id = archive_id,
                        .duration = duration,
                };
                Log::VerboseFmt("archives id is %s, duration is %lld", archive_id.c_str(), duration);
            }
            if (Config::enableMotionCaptureReplay && Config::enableInGameReplayDisplay) {
                auto it = Config::archiveConfigMap.find(archive_id);
                if (it == Config::archiveConfigMap.end()) continue;
                auto archive_config = it->second;
                auto replay_type = archive_config["replay_type"].get<uint>();
                auto archive_title = archive["name"].get<std::string>();
                std::string recommendVersion;
                if (archive_config.contains("version_compatibility") &&
                    !archive_config["version_compatibility"].is_null() &&
                    archive_config["version_compatibility"].is_object()) {
                    auto version_compatibility = archive_config["version_compatibility"];
                    if (version_compatibility.contains("rule") &&
                        !version_compatibility["rule"].is_null()) {
                        std::string rule = version_compatibility["rule"].get<std::string>();
                        recommendVersion = Config::getRecommendVersion(rule);
                    }
                }
                recommendVersion = recommendVersion.empty() ? recommendVersion : "[" + recommendVersion + "]";

                /**
                 * isMrsVersion isAlsArchive Playable
                 * 0            0            0
                 * 0            1            1
                 * 1            0            1
                 * 1            1            0
                 *
                 * Exclusive or: isMrsVersion ^ isAls
                 */
                if (replay_type == 1) { // motion capture replay
                    std::string mark = isMotionCaptureCompatible(archive_config["external_link"].get<std::string>(), archive_config) ? "✅" : "❌" + recommendVersion;
                    archive_title = mark + archive_title;
                }
                if (replay_type == 2) {
                    std::string mark = isMotionCaptureCompatible(archive_config["external_fix_link"].get<std::string>(), archive_config) ? "☑️" : "❌" + recommendVersion;
                    archive_title = mark + archive_title;
                }
                if (replay_type == 0) { // video replay
                    archive_title = "📺" + archive_title;
                }
                archive["name"] = archive_title;
            }
            if (is_legacy) {
                if (archive["live_type"].get<int>() == 2) { // with meets
                    archive["ticket_rank"] = 1;
                }
            }
        }
        if (is_legacy) {
            json.erase("filterable_characters");
            json.erase("sortable_fields");
        }
        return json;
    }

    void* advSeriesMasterInstance = nullptr;
    std::unordered_set<int64_t> advSeriesIdExistsCache;
    std::unordered_set<int64_t> advSeriesIdNotExistsCache;

    void* advDatasMasterInstance = nullptr;
    std::unordered_set<int64_t> advDataIdExistsCache;
    std::unordered_set<int64_t> advDataIdNotExistsCache;

    DEFINE_HOOK(void, Silverflame_SFL_AdvSeriesMaster_ctor, (void* self, void* conn)) {
        Log::DebugFmt("Silverflame_SFL_AdvSeriesMaster_ctor HOOKED, instance=%p", self);
        advSeriesMasterInstance = self;
        // 清空缓存
        advSeriesIdExistsCache.clear();
        advSeriesIdNotExistsCache.clear();
        return Silverflame_SFL_AdvSeriesMaster_ctor_Orig(self, conn);
    }

    DEFINE_HOOK(void*, Silverflame_SFL_AdvSeriesMaster_Fetch, (void* self, int64_t id)) {
        void* result = Silverflame_SFL_AdvSeriesMaster_Fetch_Orig(self, id);
        return result;
    }

    DEFINE_HOOK(void, Silverflame_SFL_AdvDatasMaster_ctor, (void* self, void* conn)) {
        Log::DebugFmt("Silverflame_SFL_AdvDatasMaster_ctor HOOKED, instance=%p", self);
        advDatasMasterInstance = self;
        // 清空缓存
        advDataIdExistsCache.clear();
        advDataIdNotExistsCache.clear();
        return Silverflame_SFL_AdvDatasMaster_ctor_Orig(self, conn);
    }

    DEFINE_HOOK(void*, Silverflame_SFL_AdvDatasMaster_Fetch, (void* self, int64_t id)) {
        void* result = Silverflame_SFL_AdvDatasMaster_Fetch_Orig(self, id);
        return result;
    }

    // 辅助函数：检查 adv_series_id 是否存在于本地数据库（带缓存）
    bool isAdvSeriesIdExists(int64_t id) {
        if (advSeriesIdExistsCache.count(id)) {
            return true;
        }
        if (advSeriesIdNotExistsCache.count(id)) {
            return false;
        }

        if (!advSeriesMasterInstance) {
            Log::WarnFmt("AdvSeriesMaster instance is null, cannot check id=%lld", id);
            return true; // 如果实例不存在，默认返回 true 不过滤
        }
        void* result = Silverflame_SFL_AdvSeriesMaster_Fetch_Orig(advSeriesMasterInstance, id);
        bool exists = result != nullptr;

        if (exists) {
            advSeriesIdExistsCache.insert(id);
        } else {
            advSeriesIdNotExistsCache.insert(id);
            Log::DebugFmt("adv_series_id=%lld not found in local database", id);
        }

        return exists;
    }

    bool isAdvDataIdExists(int64_t id) {
        if (advDataIdExistsCache.count(id)) {
            return true;
        }
        if (advDataIdNotExistsCache.count(id)) {
            return false;
        }

        if (!advDatasMasterInstance) {
            Log::WarnFmt("AdvDatasMaster instance is null, cannot check id=%lld", id);
            return true; // 如果实例不存在，默认返回 true 不过滤
        }
        void* result = Silverflame_SFL_AdvDatasMaster_Fetch_Orig(advDatasMasterInstance, id);
        bool exists = result != nullptr;

        if (exists) {
            advDataIdExistsCache.insert(id);
        } else {
            advDataIdNotExistsCache.insert(id);
            Log::DebugFmt("adv_data_id=%lld not found in local database", id);
        }

        return exists;
    }

    template<typename Predicate>
    void json_erase_if(nlohmann::json& arr, Predicate pred) {
        arr.erase(std::remove_if(arr.begin(), arr.end(), pred), arr.end());
    }

    void filterActivityRecordMonthlyInfoList(nlohmann::json& json) {
        if (!json.contains("activity_record_monthly_info_list") || !json["activity_record_monthly_info_list"].is_array()) {
            return;
        }
        auto& info_list = json["activity_record_monthly_info_list"];

        json_erase_if(info_list, [](const nlohmann::json& info_item) {
            auto adv_series_id = info_item.value("adv_series_id", 0LL);
            if (adv_series_id && !isAdvSeriesIdExists(adv_series_id)) {
                Log::DebugFmt("Filtering out adv_series_id=%lld", adv_series_id);
                return true;
            }
            return false;
        });

        for (auto& info_item : info_list) {
            if (!info_item.contains("adv_info_list") || !info_item["adv_info_list"].is_array()) continue;
            json_erase_if(info_item["adv_info_list"], [](const nlohmann::json& adv_info) {
                auto adv_data_id = adv_info.value("adv_data_id", 0LL);
                if (adv_data_id && !isAdvDataIdExists(adv_data_id)) {
                    Log::DebugFmt("Filtering out adv_data_id=%lld", adv_data_id);
                    return true;
                }
                return false;
            });
        }
    }

    uintptr_t ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    /**
     * Legacy version 1.x.x
     */
    uintptr_t ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_Old_MoveNext_Addr = 0;
    uintptr_t ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_Old_MoveNext_Addr = 0;
    uintptr_t ArchiveApi_ArchiveGetArchiveList_Old_MoveNext_Addr = 0;

    uintptr_t ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr = 0;
    uintptr_t WithliveApi_WithliveEnterWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t FesliveApi_FesliveEnterWithHttpInfoAsync_MoveNext_Addr = 0;

    uintptr_t WebviewLiveApi_WebviewLiveLiveInfoWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t WithliveApi_WithliveLiveInfoWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t FesliveApi_FesliveLiveInfoWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t ArchiveApi_ArchiveWithliveInfoWithHttpInfoAsync_MoveNext_Addr = 0;

    uintptr_t ActivityRecordGetTopWithHttpInfoAsync_MoveNext_Addr = 0;

    namespace OfflineApiMock {
        static std::string ReadFileToString(const std::filesystem::path& path) {
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs) return {};
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            return buffer.str();
        }

        static std::string SanitizeApiPathToRelative(std::string apiPath) {
            // If it's a full URL, keep only the path part.
            if (apiPath.rfind("http://", 0) == 0 || apiPath.rfind("https://", 0) == 0) {
                const auto schemePos = apiPath.find("://");
                const auto pathPos = schemePos == std::string::npos ? std::string::npos : apiPath.find('/', schemePos + 3);
                apiPath = pathPos == std::string::npos ? std::string("root") : apiPath.substr(pathPos);
            }

            // Strip query string if any.
            if (auto qpos = apiPath.find('?'); qpos != std::string::npos) {
                apiPath.resize(qpos);
            }
            // Strip fragment if any.
            if (auto fpos = apiPath.find('#'); fpos != std::string::npos) {
                apiPath.resize(fpos);
            }
            while (!apiPath.empty() && apiPath.front() == '/') apiPath.erase(apiPath.begin());
            if (apiPath.empty()) apiPath = "root";

            // Make it safe as a file path across platforms.
            for (auto& ch : apiPath) {
                const bool ok = (ch >= 'a' && ch <= 'z')
                             || (ch >= 'A' && ch <= 'Z')
                             || (ch >= '0' && ch <= '9')
                             || ch == '/' || ch == '_' || ch == '-' || ch == '.';
                if (!ok) ch = '_';
            }

            // Prevent path traversal.
            std::filesystem::path rel(apiPath);
            if (rel.is_absolute()) return {};
            for (const auto& part : rel) {
                if (part == "..") return {};
            }

            // Always treat it as a json mock.
            if (rel.extension() != ".json") {
                rel += ".json";
            }
            return rel.generic_string();
        }

        static std::filesystem::path GetMockRootDir() {
            const auto base = LinkuraLocal::Local::GetBasePath();
            const auto dir = Config::offlineApiMockDir.empty() ? std::filesystem::path("mock_api") : std::filesystem::path(Config::offlineApiMockDir);
            return (base / dir).lexically_normal();
        }

        static std::filesystem::path GetMockFilePath(const std::string& apiPath) {
            const auto rel = SanitizeApiPathToRelative(apiPath);
            if (rel.empty()) return {};
            return (GetMockRootDir() / std::filesystem::path(rel)).lexically_normal();
        }

        static std::string GetBuiltInFallbackJson(const std::string& apiPath) {
            // When users run offline integration tests without any mock files prepared,
            // provide a minimal set of built-in defaults for critical endpoints.
            if (apiPath == "/v1/user/login") {
                return R"({
  "type": 2,
  "session_token": "foo",
  "is_tutorial": false,
  "is_term_update": false,
  "is_login_bonus_receive": true,
  "push_device_token": "bar",
  "sisca_product_id_list": [
    "com.oddno.lovelive.sisca_01"
  ],
  "membership_product_id_list": [
    "com.oddno.lovelive.membership_0001"
  ],
  "item_store_product_id_list": [
    "com.oddno.lovelive.item_store_1001"
  ],
  "tutorials_status_list": [
    { "tutorial_id": 20,   "is_complete": true },
    { "tutorial_id": 30,   "is_complete": true },
    { "tutorial_id": 40,   "is_complete": true },
    { "tutorial_id": 50,   "is_complete": true },
    { "tutorial_id": 5010, "is_complete": true },
    { "tutorial_id": 5020, "is_complete": true },
    { "tutorial_id": 5030, "is_complete": true },
    { "tutorial_id": 5040, "is_complete": true },
    { "tutorial_id": 5050, "is_complete": true },
    { "tutorial_id": 5060, "is_complete": true },
    { "tutorial_id": 5070, "is_complete": true }
  ]
})";
            }
            return "{}";
        }

	        struct Methods {
	            void* restResponseKlass = nullptr;
	            Il2cppUtils::MethodInfo* restResponseCtor = nullptr;
	            Il2cppUtils::MethodInfo* setContent = nullptr;
	            Il2cppUtils::MethodInfo* setContentType = nullptr;
	            Il2cppUtils::MethodInfo* setStatusCode = nullptr;
	            Il2cppUtils::MethodInfo* setStatusDescription = nullptr;
	            Il2cppUtils::MethodInfo* setResponseStatus = nullptr;
	            Il2cppUtils::MethodInfo* setContentLength = nullptr;
	            Il2cppUtils::MethodInfo* setRawBytes = nullptr;
	            Il2cppUtils::MethodInfo* setHeaders = nullptr;

	            // Build a completed Task<object> via TaskCompletionSource<object> (class, safe to invoke from native).
	            // ApiClient.CallApiAsync returns Task<object> (confirmed in libil2cpp decompile).
	            void* taskCompletionSourceObjectKlass = nullptr; // Il2CppClass* for TaskCompletionSource<object>
	            Il2cppUtils::MethodInfo* tcsCtor = nullptr;      // .ctor()
	            Il2cppUtils::MethodInfo* tcsSetResult = nullptr; // SetResult(object)
	            Il2cppUtils::MethodInfo* tcsGetTask = nullptr;   // get_Task()
	            UnityResolve::Method* restResponseGetContent = nullptr;
	            UnityResolve::Method* restResponseGetStatusCode = nullptr;
	            UnityResolve::Method* restResponseGetResponseStatus = nullptr;
	            UnityResolve::Method* restResponseGetHeaders = nullptr;

	            // For setting RawBytes in a safe way (avoid manual Il2CppArray layout).
	            UnityResolve::Method* encodingGetUtf8 = nullptr;   // System.Text.Encoding.get_UTF8
	            UnityResolve::Method* encodingGetBytes = nullptr;  // Encoding.GetBytes(string)
	        };

        static void* TryGetClassIl2cpp(std::initializer_list<const char*> assemblies, const char* ns, const char* name) {
            for (const auto asmName : assemblies) {
                if (!asmName) continue;
                if (auto klass = Il2cppUtils::GetClassIl2cpp(asmName, ns, name)) return klass;
            }
            return nullptr;
        }

        static Il2cppUtils::MethodInfo* TryGetMethodIl2cpp(std::initializer_list<const char*> assemblies,
                                                           const char* ns,
                                                           const char* className,
                                                           const char* methodName,
                                                           int argsCount) {
            for (const auto asmName : assemblies) {
                if (!asmName) continue;
                if (auto mtd = Il2cppUtils::GetMethodIl2cpp(asmName, ns, className, methodName, argsCount)) return mtd;
            }
            return nullptr;
        }

        static Methods& GetMethods() {
            static Methods methods{};
            static bool inited = false;
            if (inited) return methods;
            inited = true;

            const auto restSharpAssemblies = { "RestSharp.dll", "RestSharp" };
            methods.restResponseKlass = TryGetClassIl2cpp(restSharpAssemblies, "RestSharp", "RestResponse");
            methods.restResponseCtor = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponse", ".ctor", 0);
            methods.setContent = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_Content", 1);
            methods.setContentType = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_ContentType", 1);
            methods.setStatusCode = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_StatusCode", 1);
            methods.setStatusDescription = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_StatusDescription", 1);
            methods.setResponseStatus = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_ResponseStatus", 1);
            methods.setContentLength = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_ContentLength", 1);
	            methods.setRawBytes = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_RawBytes", 1);
	            methods.setHeaders = TryGetMethodIl2cpp(restSharpAssemblies, "RestSharp", "RestResponseBase", "set_Headers", 1);

	            // TaskCompletionSource<object> for building a completed Task<object> without Task.FromResult / struct builder pitfalls.
	            methods.taskCompletionSourceObjectKlass = Il2cppUtils::get_system_class_from_reflection_type_str(
	                "System.Threading.Tasks.TaskCompletionSource`1[[System.Object, mscorlib]]",
	                "mscorlib");
	            if (!methods.taskCompletionSourceObjectKlass) {
	                methods.taskCompletionSourceObjectKlass = Il2cppUtils::get_system_class_from_reflection_type_str(
	                    "System.Threading.Tasks.TaskCompletionSource`1[System.Object]",
	                    "mscorlib");
	            }
	            if (methods.taskCompletionSourceObjectKlass) {
	                methods.tcsCtor = Il2cppUtils::GetMethodIl2cpp(methods.taskCompletionSourceObjectKlass, ".ctor", 0);
	                methods.tcsSetResult = Il2cppUtils::GetMethodIl2cpp(methods.taskCompletionSourceObjectKlass, "SetResult", 1);
	                methods.tcsGetTask = Il2cppUtils::GetMethodIl2cpp(methods.taskCompletionSourceObjectKlass, "get_Task", 0);
	            }
            methods.restResponseGetContent = Il2cppUtils::GetMethod("RestSharp.dll", "RestSharp", "RestResponseBase", "get_Content");
            if (!methods.restResponseGetContent) {
                methods.restResponseGetContent = Il2cppUtils::GetMethod("RestSharp", "RestSharp", "RestResponseBase", "get_Content");
            }
            methods.restResponseGetStatusCode = Il2cppUtils::GetMethod("RestSharp.dll", "RestSharp", "RestResponseBase", "get_StatusCode");
            if (!methods.restResponseGetStatusCode) {
                methods.restResponseGetStatusCode = Il2cppUtils::GetMethod("RestSharp", "RestSharp", "RestResponseBase", "get_StatusCode");
            }
            methods.restResponseGetResponseStatus = Il2cppUtils::GetMethod("RestSharp.dll", "RestSharp", "RestResponseBase", "get_ResponseStatus");
            if (!methods.restResponseGetResponseStatus) {
                methods.restResponseGetResponseStatus = Il2cppUtils::GetMethod("RestSharp", "RestSharp", "RestResponseBase", "get_ResponseStatus");
            }
            methods.restResponseGetHeaders = Il2cppUtils::GetMethod("RestSharp.dll", "RestSharp", "RestResponseBase", "get_Headers");
            if (!methods.restResponseGetHeaders) {
                methods.restResponseGetHeaders = Il2cppUtils::GetMethod("RestSharp", "RestSharp", "RestResponseBase", "get_Headers");
            }

	            // UTF8 encoding helpers.
	            methods.encodingGetUtf8 = Il2cppUtils::GetMethod("mscorlib.dll", "System.Text", "Encoding", "get_UTF8");
	            methods.encodingGetBytes = Il2cppUtils::GetMethod("mscorlib.dll", "System.Text", "Encoding", "GetBytes", { "System.String" });

	            if (Config::dbgMode || Config::enableOfflineApiMock) {
	                Log::InfoFmt(
	                    "[OfflineApiMock] resolve OfflineApiMock: RestResponse klass=%p ctor=%p setContent=%p TCS klass=%p ctor=%p SetResult=%p get_Task=%p",
	                    methods.restResponseKlass,
	                    methods.restResponseCtor ? (void*)methods.restResponseCtor->methodPointer : nullptr,
	                    methods.setContent ? (void*)methods.setContent->methodPointer : nullptr,
	                    methods.taskCompletionSourceObjectKlass,
	                    methods.tcsCtor ? (void*)methods.tcsCtor->methodPointer : nullptr,
	                    methods.tcsSetResult ? (void*)methods.tcsSetResult->methodPointer : nullptr,
	                    methods.tcsGetTask ? (void*)methods.tcsGetTask->methodPointer : nullptr);
	            }

            return methods;
        }

        static void* CreateRestResponse(const std::string& jsonBody, int httpStatusCode, const std::string& statusDescription) {
            auto& m = GetMethods();
            if (!m.restResponseKlass || !m.restResponseCtor || !m.setContent || !m.setContentType || !m.setStatusCode || !m.setStatusDescription || !m.setResponseStatus) {
                Log::Error("OfflineApiMock: RestSharp methods not resolved.");
                return nullptr;
            }

            auto resp = UnityResolve::Invoke<void*>("il2cpp_object_new", m.restResponseKlass);
            if (!resp) return nullptr;

            using CtorFn = void(*)(void*, Il2cppUtils::MethodInfo*);
            reinterpret_cast<CtorFn>(m.restResponseCtor->methodPointer)(resp, m.restResponseCtor);

            using SetStringFn = void(*)(void*, Il2cppUtils::Il2CppString*, Il2cppUtils::MethodInfo*);
            using SetIntFn = void(*)(void*, int, Il2cppUtils::MethodInfo*);

            auto contentStr = Il2cppUtils::Il2CppString::New(jsonBody);
            reinterpret_cast<SetStringFn>(m.setContent->methodPointer)(resp, contentStr, m.setContent);
            reinterpret_cast<SetStringFn>(m.setContentType->methodPointer)(resp, Il2cppUtils::Il2CppString::New("application/json"), m.setContentType);
            reinterpret_cast<SetIntFn>(m.setStatusCode->methodPointer)(resp, httpStatusCode, m.setStatusCode);
            reinterpret_cast<SetStringFn>(m.setStatusDescription->methodPointer)(resp, Il2cppUtils::Il2CppString::New(statusDescription), m.setStatusDescription);
            // RestSharp.ResponseStatus.Completed is typically 1.
            reinterpret_cast<SetIntFn>(m.setResponseStatus->methodPointer)(resp, 1, m.setResponseStatus);

            // Best-effort: ensure Headers is non-null (some generated clients enumerate response.Headers unconditionally).
            if (m.setHeaders) {
                static void* listOfParamKlass = nullptr;
                static Il2cppUtils::MethodInfo* listCtor = nullptr;
                if (!listOfParamKlass) {
                    // Assembly-qualified generic arg is required since RestSharp.Parameter lives in RestSharp assembly.
                    listOfParamKlass = Il2cppUtils::get_system_class_from_reflection_type_str(
                        "System.Collections.Generic.List`1[[RestSharp.Parameter, RestSharp]]", "mscorlib");
                    if (listOfParamKlass) {
                        listCtor = Il2cppUtils::GetMethodIl2cpp(listOfParamKlass, ".ctor", 0);
                    }
                }
                if (listOfParamKlass && listCtor) {
                    auto emptyList = UnityResolve::Invoke<void*>("il2cpp_object_new", listOfParamKlass);
                    if (emptyList) {
                        using CtorFn = void(*)(void*, Il2cppUtils::MethodInfo*);
                        reinterpret_cast<CtorFn>(listCtor->methodPointer)(emptyList, listCtor);
                        using SetObjFn = void(*)(void*, void*, Il2cppUtils::MethodInfo*);
                        reinterpret_cast<SetObjFn>(m.setHeaders->methodPointer)(resp, emptyList, m.setHeaders);
                    }
                }
            }

            // Best-effort: also populate RawBytes/ContentLength, since some client code prefers RawBytes.
            if (m.setRawBytes && m.setContentLength && m.encodingGetUtf8 && m.encodingGetUtf8->function && m.encodingGetBytes && m.encodingGetBytes->function) {
                // Encoding.UTF8.GetBytes(content)
                using GetUtf8Fn = void*(*)(void* method_info);
                using GetBytesFn = void*(*)(void* selfEncoding, Il2cppUtils::Il2CppString* s, void* method_info);
                auto enc = reinterpret_cast<GetUtf8Fn>(m.encodingGetUtf8->function)(m.encodingGetUtf8->address);
                if (enc) {
                    auto bytes = reinterpret_cast<GetBytesFn>(m.encodingGetBytes->function)(enc, contentStr, m.encodingGetBytes->address);
                    if (bytes) {
                        using SetObjFn = void(*)(void*, void*, Il2cppUtils::MethodInfo*);
                        reinterpret_cast<SetObjFn>(m.setRawBytes->methodPointer)(resp, bytes, m.setRawBytes);
                        // ContentLength = RawBytes.Length (we don't read length here; set to content string length as fallback).
                        using SetI64Fn = void(*)(void*, int64_t, Il2cppUtils::MethodInfo*);
                        reinterpret_cast<SetI64Fn>(m.setContentLength->methodPointer)(resp, (int64_t)jsonBody.size(), m.setContentLength);
                    }
                }
            }

            if (m.restResponseGetContent && m.restResponseGetContent->function && (Config::dbgMode || Config::enableOfflineApiMock)) {
                // Best-effort sanity check: ensure Content getter sees what we set.
                using GetContentFn = Il2cppUtils::Il2CppString*(*)(void*, void*);
                auto content = reinterpret_cast<GetContentFn>(m.restResponseGetContent->function)(resp, m.restResponseGetContent->address);
                const auto contentLen = content ? (int)content->ToString().size() : -1;
                Log::InfoFmt("[OfflineApiMock] RestResponse content length=%d", contentLen);
            }
            if ((Config::dbgMode || Config::enableOfflineApiMock) && m.restResponseGetStatusCode && m.restResponseGetStatusCode->function) {
                using GetIntFn = int(*)(void*, void*);
                auto sc = reinterpret_cast<GetIntFn>(m.restResponseGetStatusCode->function)(resp, m.restResponseGetStatusCode->address);
                Log::InfoFmt("[OfflineApiMock] RestResponse StatusCode=%d", sc);
            }
            if ((Config::dbgMode || Config::enableOfflineApiMock) && m.restResponseGetResponseStatus && m.restResponseGetResponseStatus->function) {
                using GetIntFn = int(*)(void*, void*);
                auto rs = reinterpret_cast<GetIntFn>(m.restResponseGetResponseStatus->function)(resp, m.restResponseGetResponseStatus->address);
                Log::InfoFmt("[OfflineApiMock] RestResponse ResponseStatus=%d", rs);
            }
            if ((Config::dbgMode || Config::enableOfflineApiMock) && m.restResponseGetHeaders && m.restResponseGetHeaders->function) {
                using GetObjFn = void*(*)(void*, void*);
                auto headers = reinterpret_cast<GetObjFn>(m.restResponseGetHeaders->function)(resp, m.restResponseGetHeaders->address);
                Log::InfoFmt("[OfflineApiMock] RestResponse Headers=%p", headers);
            }

            return resp;
        }

        static void* TaskFromResultObject(void* resultObject) {
            auto& m = GetMethods();
            if (!m.taskCompletionSourceObjectKlass || !m.tcsCtor || !m.tcsSetResult || !m.tcsGetTask) {
                Log::Error("OfflineApiMock: TaskCompletionSource<object> not resolved (.ctor/SetResult/get_Task missing).");
                return nullptr;
            }

            // This hook may run on non-Unity threads; il2cpp_runtime_invoke requires the current thread to be attached.
            UnityResolve::ThreadAttach();

            if (Config::dbgMode || Config::enableOfflineApiMock) {
                Log::InfoFmt("[OfflineApiMock] creating completed Task<object> for result=%p", resultObject);
            }

            auto tcs = UnityResolve::Invoke<void*>("il2cpp_object_new", m.taskCompletionSourceObjectKlass);
            if (!tcs) {
                Log::Error("OfflineApiMock: il2cpp_object_new(TaskCompletionSource<object>) failed.");
                return nullptr;
            }

            using CtorFn = void(*)(void*, Il2cppUtils::MethodInfo*);
            reinterpret_cast<CtorFn>(m.tcsCtor->methodPointer)(tcs, m.tcsCtor);

            using SetObjFn = void(*)(void*, void*, Il2cppUtils::MethodInfo*);
            reinterpret_cast<SetObjFn>(m.tcsSetResult->methodPointer)(tcs, resultObject, m.tcsSetResult);

            using GetObjFn = void*(*)(void*, Il2cppUtils::MethodInfo*);
            auto task = reinterpret_cast<GetObjFn>(m.tcsGetTask->methodPointer)(tcs, m.tcsGetTask);
            if (!task) {
                Log::Error("OfflineApiMock: TaskCompletionSource<object>.Task returned nullptr.");
                return nullptr;
            }

            if (Config::dbgMode || Config::enableOfflineApiMock) {
                auto k = Il2cppUtils::get_class_from_instance(task);
                Log::InfoFmt("[OfflineApiMock] created completed task=%p klass=%s.%s",
                             task,
                             (k && k->namespaze) ? k->namespaze : "",
                             (k && k->name) ? k->name : "");
            }
            return task;
        }
    }

	    // http request log
	    DEFINE_HOOK(void*, ApiClient_CallApiAsync, (void* self,
	            Il2cppUtils::Il2CppString* path, void* method,
	            void* queryParams, void* postBody,
            void* headerParams, void* formParams, void* fileParams, void* pathParams,
            Il2cppUtils::Il2CppString* contentType, void* cancellationToken, void* method_info)) {
        auto strPath = path ? path->ToString() : "(null)";
        std::string strBody = "{}";
        if (postBody) {
            const auto klass = Il2cppUtils::get_class_from_instance(postBody);
            const bool isString = klass
                && klass->name && std::string_view(klass->name) == "String"
                && klass->namespaze && std::string_view(klass->namespaze) == "System";
            if (isString) {
                auto bodyStr = static_cast<Il2cppUtils::Il2CppString*>(postBody);
                strBody = bodyStr->ToString();
            }
        }
        Log::VerboseFmt("[ApiClient_CallApiAsync] path: %s\nrequest: %s", strPath.c_str(), strBody.c_str());

        if (Config::enableOfflineApiMock && path) {
            const auto mockFile = OfflineApiMock::GetMockFilePath(strPath);
            const auto mockFileStr = mockFile.empty() ? std::string("(invalid path)") : mockFile.string();
            std::string mockJson;
            if (!mockFile.empty()) {
                mockJson = OfflineApiMock::ReadFileToString(mockFile);
            }

            if (mockJson.empty()) {
                if (Config::offlineApiMockForceNoNetwork) {
                    Log::WarnFmt("[OfflineApiMock] missing mock file for path=%s (expected: %s), using built-in fallback json",
                                 strPath.c_str(), mockFileStr.c_str());
                    mockJson = OfflineApiMock::GetBuiltInFallbackJson(strPath);
                } else {
                    Log::WarnFmt("[OfflineApiMock] missing mock file for path=%s, falling back to real HTTP", strPath.c_str());
                    return ApiClient_CallApiAsync_Orig(self, path, method, queryParams, postBody,
                                                      headerParams, formParams, fileParams, pathParams,
                                                      contentType, cancellationToken, method_info);
                }
            } else {
                Log::InfoFmt("[OfflineApiMock] hit %s", mockFileStr.c_str());
            }

            auto resp = OfflineApiMock::CreateRestResponse(mockJson, 200, "OK (offline mock)");
            if (!resp) {
                Log::Error("[OfflineApiMock] failed to create RestResponse, returning empty json response");
                resp = OfflineApiMock::CreateRestResponse("{}", 200, "OK (offline mock)");
            }

            if (Config::dbgMode || Config::enableOfflineApiMock) {
                Log::InfoFmt("[OfflineApiMock] creating completed Task<object> for resp=%p path=%s", resp, strPath.c_str());
            }
            auto task = OfflineApiMock::TaskFromResultObject(resp);
            if (!task) {
                // Last-resort: avoid sending network request.
                Log::Error("[OfflineApiMock] failed to create completed Task<object>, returning nullptr (may break caller)");
                return nullptr;
            }
            if (Config::dbgMode || Config::enableOfflineApiMock) {
                auto tk = Il2cppUtils::get_class_from_instance(task);
                Log::InfoFmt("[OfflineApiMock] returning mock task=%p klass=%s.%s for path=%s",
                             task,
                             (tk && tk->namespaze) ? tk->namespaze : "",
                             (tk && tk->name) ? tk->name : "",
                             strPath.c_str());
            }
            return task;
        }

        return ApiClient_CallApiAsync_Orig(self, path, method, queryParams, postBody,
                                          headerParams, formParams, fileParams, pathParams,
                                          contentType, cancellationToken, method_info);
    }
    // http response modify
    DEFINE_HOOK(void* , ApiClient_Deserialize, (void* self, void* response, void* type, void* method_info)) {
        if (Config::dbgMode || Config::enableOfflineApiMock) {
            auto caller = __builtin_return_address(0);
            Log::VerboseFmt("[ApiClient_Deserialize] enter self=%p response=%p type=%p caller=%p", self, response, type, caller);
        }
        auto result = ApiClient_Deserialize_Orig(self, response, type, method_info);
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(result)->ToString());
        // Print API response type name and response body for debugging
        {
            auto klass = UnityResolve::Invoke<void*>("il2cpp_class_from_system_type", type);
            if (klass) {
                auto ns   = UnityResolve::Invoke<const char*>("il2cpp_class_get_namespace", klass);
                auto name = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", klass);
                Log::VerboseFmt("[ApiClient_Deserialize] type: %s.%s\nresponse: %s",
                    ns ? ns : "", name ? name : "", json.dump().c_str());
            }
        }
        auto caller = __builtin_return_address(0);
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_fes_archive_data response
            json = handle_get_fes_archive_data(json);
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_with_archive_data response
            json = handle_get_with_archive_data(json);
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_Old_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_fes_archive_data response 1.x.x
            json = handle_get_fes_archive_data(json, Config::isFirstYearVersion());
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_Old_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_with_archive_data response 1.x.x
            json = handle_get_with_archive_data(json, Config::isFirstYearVersion());
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_archive_list response
            json = handle_get_archive_list(json);
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetArchiveList_Old_MoveNext_Addr, caller, 3000) { // 1.x.x
            json = handle_get_archive_list(json, Config::isFirstYearVersion());
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(WithliveApi_WithliveEnterWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::withliveOrientation == (int)HookLiveRender::LiveScreenOrientation::Landscape) {
                json["is_horizontal"] = "true";
            }
            if (Config::withliveOrientation == (int)HookLiveRender::LiveScreenOrientation::Portrait) {
                json["is_horizontal"] = "false";
            }
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
//        IF_CALLER_WITHIN(FesliveApi_FesliveEnterWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
////            if (Config::unlockAfter) {
////                json["has_extra_admission"] = "true";
////            }
//            // if (Config::fesArchiveUnlockTicket) {
//            //     json["selectable_camera_types"] = {1,2,3,4};
//            //     json["ticket_rank"] = 6;
//            // }
//            result = Il2cppUtils::FromJsonStr(json.dump(), type);
//        }
        // live info
        IF_CALLER_WITHIN(ArchiveApi_ArchiveWithliveInfoWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(WithliveApi_WithliveLiveInfoWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
//                json["has_admission"] = "true";
            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
//        IF_CALLER_WITHIN(FesliveApi_FesliveLiveInfoWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
////            if (Config::unlockAfter) {
////                json["has_admission"] = "true";
////            }
//            result = Il2cppUtils::FromJsonStr(json.dump(), type);
//        }
        IF_CALLER_WITHIN(ActivityRecordGetTopWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::enableLegacyCompatibility && !Config::isLatestVersion()) {
                filterActivityRecordMonthlyInfoList(json);
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }
        return result;
    }

    DEFINE_HOOK(void* , ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Log::DebugFmt("ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync HOOKED");
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(request)->ToString());
        auto archive_id = json["archives_id"].get<std::string>();
        if (try_handle_get_archive_data(archive_id)) {
            return nullptr;
        }
        Shareable::currentArchiveId = archive_id;
        Shareable::renderScene = Shareable::RenderScene::FesLive;
        return ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_Orig(self,
                                                                         request,
                                                                         cancellation_token, method_info);
    }
    DEFINE_HOOK(void* , ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Log::DebugFmt("ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync HOOKED");
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(request)->ToString());
        auto archive_id = json["archives_id"].get<std::string>();
        if (try_handle_get_archive_data(archive_id)) {
            return nullptr;
        }
        Shareable::currentArchiveId = archive_id;
        Shareable::renderScene = Shareable::RenderScene::WithLive;
        return ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_Orig(self,
                                                                          request,
                                                                          cancellation_token, method_info);
    }

    DEFINE_HOOK(void*, ArchiveApi_ArchiveGetArchiveListWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Log::DebugFmt("ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync HOOKED");
        if (Config::enableMotionCaptureReplay && Config::filterMotionCaptureReplay) {
            auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(request)->ToString());
            json.erase("limit");
            json.erase("offset");
            request = static_cast<Il2cppUtils::Il2CppObject*>(
                    Il2cppUtils::FromJsonStr(json.dump(), Il2cppUtils::get_system_type_from_instance(request))
            );
        }
        return ArchiveApi_ArchiveGetArchiveListWithHttpInfoAsync_Orig(self, request, cancellation_token, method_info);
    }

    DEFINE_HOOK(void* , ArchiveApi_ArchiveWithliveInfoWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        if (Config::unlockAfter || (Config::enableMotionCaptureReplay && Config::filterMotionCaptureReplay)) {
            return nullptr;
        }
        return ArchiveApi_ArchiveWithliveInfoWithHttpInfoAsync_Orig(self,
                                                                          request,
                                                                          cancellation_token, method_info);
    }
    // cheat for server api, but we need to decrease the abnormal behaviour here. ( camera_type should change when every request sends )
    DEFINE_HOOK(void* ,ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        if (Config::fesArchiveUnlockTicket) {
            return nullptr;
//            auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(request)->ToString());
//            json["camera_type"] = 1;
//            if (json.contains("focus_character_id")){
//                json.erase("focus_character_id");
//            }
//            request = static_cast<Il2cppUtils::Il2CppObject*>(
//                    Il2cppUtils::FromJsonStr(json.dump(), Il2cppUtils::get_system_type_from_instance(request))
//            );
        }
        return ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync_Orig(self,
                                                                    request,
                                                                    cancellation_token, method_info);
    }

    DEFINE_HOOK(void*, ArchiveApi_ArchiveSetFesCameraAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Log::DebugFmt("ArchiveApi_ArchiveSetFesCameraAsync HOOKED");
        return nullptr;
    }
    DEFINE_HOOK(void*, FesliveApi_FesliveSetCameraWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
         if (Config::fesArchiveUnlockTicket) {
             return nullptr;
         }
        return FesliveApi_FesliveSetCameraWithHttpInfoAsync_Orig(self,
                                                                    request,
                                                                    cancellation_token, method_info);
    }

    DEFINE_HOOK(void*, ArchiveApi_ArchiveGetWithTimelineDataWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        return nullptr;
    }
    DEFINE_HOOK(void*, ArchiveApi_ArchiveGetFesTimelineDataWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        return nullptr;
    }

    DEFINE_HOOK(void* , FesliveApi_FesliveEnterWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Shareable::renderScene = Shareable::RenderScene::FesLive;
        return FesliveApi_FesliveEnterWithHttpInfoAsync_Orig(self,
                                                                          request,
                                                                          cancellation_token, method_info);
    }

    DEFINE_HOOK(void* , WithliveApi_WithliveEnterWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Shareable::renderScene = Shareable::RenderScene::WithLive;
        return WithliveApi_WithliveEnterWithHttpInfoAsync_Orig(self,
                                                             request,
                                                             cancellation_token, method_info);
    }

    DEFINE_HOOK(Il2cppUtils::Il2CppString* , AlstArchiveDirectory_GetLocalFullPathFromFileName, (Il2cppUtils::Il2CppObject* self, Il2cppUtils::Il2CppString* fileName)) {
        auto result = AlstArchiveDirectory_GetLocalFullPathFromFileName_Orig(self, fileName);
        auto result_str = result->ToString();
        if (Config::enableMotionCaptureReplay) {
            auto archive_id = Shareable::currentArchiveId;
            auto it = Config::archiveConfigMap.find(archive_id);
            if (it == Config::archiveConfigMap.end()) return result;
            auto archive_config = it->second;
            auto replay_type = archive_config["replay_type"].get<uint>();
            if (replay_type == 1 || replay_type == 2) { // replay
                auto new_result_str = replaceUriHost(result_str, Config::motionCaptureResourceUrl);
                result = Il2cppUtils::Il2CppString::New(new_result_str);
            }
        }
        return result;
    }

#pragma endregion

#pragma region oldVersion
    DEFINE_HOOK(void, Configuration_AddDefaultHeader, (void* self, Il2cppUtils::Il2CppString* key, Il2cppUtils::Il2CppString* value, void* mtd)) {
        if (Config::enableLegacyCompatibility) {
            Log::DebugFmt("Configuration_AddDefaultHeader HOOKED, %s=%s", key->ToString().c_str(), value->ToString().c_str());
            auto key_str = key->ToString();
            auto value_str = value->ToString();
            if (key_str == "x-client-version") {
                value = Il2cppUtils::Il2CppString::New(Config::latestClientVersion.toString());
            }
            if (key_str == "x-res-version") {
                value = Il2cppUtils::Il2CppString::New(Misc::StringFormat::split_once(Config::latestResVersion, "@").first);
            }
        }
        Configuration_AddDefaultHeader_Orig(self, key, value ,mtd);
    }

    DEFINE_HOOK(void, Configuration_set_UserAgent, (void* self, Il2cppUtils::Il2CppString* value, void* mtd)) {
        if (Config::enableLegacyCompatibility) {
            Log::DebugFmt("Configuration_set_UserAgent HOOKED, %s", value->ToString().c_str());
            auto value_str = value->ToString();
            if (value_str.starts_with("inspix-android")) {
                value = Il2cppUtils::Il2CppString::New("inspix-android/" + Config::latestClientVersion.toString());
            }
        }
        Configuration_set_UserAgent_Orig(self, value ,mtd);
    }

//    DEFINE_HOOK(void, AssetManager_SynchronizeResourceVersion_MoveNext, (void* self, void* mtd)) {
////        Log::DebugFmt("AssetManager_SynchronizeResourceVersion HOOKED, requestedVersion is %s", requestedVersion->ToString().c_str());
//        static auto AssetManager_klass = Il2cppUtils::GetClassIl2cpp("Core.dll", "Hailstorm", "AssetManager");
//        static auto SynchronizeResourceVersion_klass = Il2cppUtils::find_nested_class_from_name(AssetManager_klass, "<SynchronizeResourceVersion>d__22");
//        Log::DebugFmt("SynchronizeResourceVersion_klass is at %p", SynchronizeResourceVersion_klass);
//        if (SynchronizeResourceVersion_klass) {
//            static auto requestedVersion_field = UnityResolve::Invoke<Il2cppUtils::FieldInfo*>("il2cpp_class_get_field_from_name", SynchronizeResourceVersion_klass, "requestedVersion");
//            static auto savedVersion_field = UnityResolve::Invoke<Il2cppUtils::FieldInfo*>("il2cpp_class_get_field_from_name", SynchronizeResourceVersion_klass, "savedVersion");
//            Log::DebugFmt("requestedVersion_field is %p, savedVersion_field is %p", requestedVersion_field, savedVersion_field);
//            auto requestedVersion = Il2cppUtils::ClassGetFieldValue<Il2cppUtils::Il2CppString*>(self, requestedVersion_field);
//            auto savedVersion = Il2cppUtils::ClassGetFieldValue<Il2cppUtils::Il2CppString*>(self, savedVersion_field);
//            if (requestedVersion) {
//                auto requestedVersion_str = requestedVersion->ToString();
//                Log::DebugFmt("AssetManager_SynchronizeResourceVersion HOOKED, requestedVersion is %s", requestedVersion_str.c_str());
//            }
//            if (savedVersion) {
//                auto savedVersion_str = savedVersion->ToString();
//                Log::DebugFmt("AssetManager_SynchronizeResourceVersion HOOKED, savedVersion is %s",  savedVersion_str.c_str());
//            }
//        }
//        AssetManager_SynchronizeResourceVersion_MoveNext_Orig(self, mtd);
//    }

    // this will cause stuck
//    DEFINE_HOOK(void*, AssetManager_SynchronizeResourceVersion, (void* self, Il2cppUtils::Il2CppString* requestedVersion, Il2cppUtils::Il2CppString* savedVersion, void* mtd)) {
//        Log::DebugFmt("Hailstorm_AssetManager__SynchronizeResourceVersion HOOKED, requestedVersion is %s, savedVersion is %s", requestedVersion->ToString().c_str(), savedVersion->ToString().c_str());
////        auto hooked_requestedVersion = Il2cppUtils::Il2CppString::New("R2504300@hbZZOCoWTueF+rikQLgPapC2Qw==");
//        return AssetManager_SynchronizeResourceVersion_Orig(self, requestedVersion, savedVersion, mtd);
//    }

    // Core_SynchronizeResourceVersion -> AssetManager_SynchronizeResourceVersion
    DEFINE_HOOK(void* , Core_SynchronizeResourceVersion, (void* self, Il2cppUtils::Il2CppString* requestedVersion,  void* mtd)) {
        Log::DebugFmt("Core_SynchronizeResourceVersion HOOKED, requestedVersion is %s", requestedVersion->ToString().c_str());
        if (Config::enableLegacyCompatibility) {
            Log::DebugFmt("requestedVersion is changed from %s to %s", requestedVersion->ToString().c_str(), Config::currentResVersion.c_str());
            requestedVersion = Il2cppUtils::Il2CppString::New(Config::currentResVersion);
        }
        return Core_SynchronizeResourceVersion_Orig(self, requestedVersion, mtd);
    }
    DEFINE_HOOK(Il2cppUtils::Il2CppString*, Application_get_version, ()) {
        Il2cppUtils::Il2CppString* result = Application_get_version_Orig();
        if (Config::enableLegacyCompatibility) {
            Log::DebugFmt("Application_get_version HOOKED, version is changed from %s to %s", result->ToString().c_str(), Config::currentClientVersion.toString().c_str());
            result = Il2cppUtils::Il2CppString::New(Config::currentClientVersion.toString());
        }
        return result;
    }
#pragma region

    void Install(HookInstaller* hookInstaller) {
        if (Config::dbgMode || Config::enableOfflineApiMock) {
            Log::InfoFmt("[OfflineApiMock] native build=%s %s", __DATE__, __TIME__);
        }

        // GetHttpAsyncAddr
        ADD_HOOK(ApiClient_CallApiAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client","ApiClient", "CallApiAsync"));
        ADD_HOOK(ApiClient_Deserialize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client","ApiClient", "Deserialize"));
#pragma region ArchiveApi
        auto ArchiveApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi");
        auto method = (Il2cppUtils::MethodInfo*) nullptr;
        if (ArchiveApi_klass) {
            // hook /v1/archive/get_fes_archive_data response
            auto ArchiveGetFesArchiveDataWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetFesArchiveDataWithHttpInfoAsync>d__30");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetFesArchiveDataWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/archive/get_with_archive_data response
            auto ArchiveGetWithArchiveDataWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetWithArchiveDataWithHttpInfoAsync>d__46");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetWithArchiveDataWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/archive/get_fes_archive_data response 1.x.x
            auto ArchiveGetFesArchiveDataWithHttpInfoAsync_old_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetFesArchiveDataWithHttpInfoAsync>d__34");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetFesArchiveDataWithHttpInfoAsync_old_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_Old_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/archive/get_with_archive_data response 1.x.x
            auto ArchiveGetWithArchiveDataWithHttpInfoAsync_old_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetWithArchiveDataWithHttpInfoAsync>d__50");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetWithArchiveDataWithHttpInfoAsync_old_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_Old_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/archive/get_archive_list response
            auto ArchiveGetArchiveListWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetArchiveListWithHttpInfoAsync>d__18");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetArchiveListWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/archive/get_archive_list response 1.x.x
            auto ArchiveGetArchiveListWithHttpInfoAsync_old_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetArchiveListWithHttpInfoAsync>d__22");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetArchiveListWithHttpInfoAsync_old_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetArchiveList_Old_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/archive/withlive_info
            auto ArchiveWithliveInfoWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveWithliveInfoWithHttpInfoAsync>d__70");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveWithliveInfoWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveWithliveInfoWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }
        ADD_HOOK(ArchiveApi_ArchiveWithliveInfoWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveWithliveInfoWithHttpInfoAsync"));
        // Fes live camera unlock
        ADD_HOOK(ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveSetFesCameraWithHttpInfoAsync"));
#pragma endregion

#pragma region WithliveApi
        auto WithliveApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "WithliveApi");
        method = (Il2cppUtils::MethodInfo*) nullptr;
        if (WithliveApi_klass) {
            // hook /v1/withlive/enter response
            auto WithliveEnterWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(
                    WithliveApi_klass, "<WithliveEnterWithHttpInfoAsync>d__30");
            method = Il2cppUtils::GetMethodIl2cpp(WithliveEnterWithHttpInfoAsync_klass, "MoveNext",
                                                  0);
            if (method) {
                WithliveApi_WithliveEnterWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/withlive/live_info response
            auto WithliveLiveInfoWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(
                    WithliveApi_klass, "<WithliveLiveInfoWithHttpInfoAsync>d__50");
            method = Il2cppUtils::GetMethodIl2cpp(WithliveLiveInfoWithHttpInfoAsync_klass, "MoveNext",
                                                  0);
            if (method) {
                WithliveApi_WithliveLiveInfoWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }
#pragma endregion

#pragma region FesliveApi
        auto FesliveApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "FesliveApi");
        if (FesliveApi_klass) {
            // hook /v1/withlive/enter response
            auto FesliveEnterWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(
                    FesliveApi_klass, "<FesliveEnterWithHttpInfoAsync>d__38");
            method = Il2cppUtils::GetMethodIl2cpp(FesliveEnterWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                FesliveApi_FesliveEnterWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/feslive/live_info response
            auto FesliveLiveInfoWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(
                    FesliveApi_klass, "<FesliveLiveInfoWithHttpInfoAsync>d__66");
            method = Il2cppUtils::GetMethodIl2cpp(FesliveLiveInfoWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                FesliveApi_FesliveLiveInfoWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }
        // Fes live camera unlock
        ADD_HOOK(FesliveApi_FesliveSetCameraWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "FesliveApi", "FesliveSetCameraWithHttpInfoAsync"));
#pragma endregion

#pragma region WebviewLiveApi
        auto WebviewLiveApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "WebviewLiveApi");
        if (WebviewLiveApi_klass) {
            // hook /v1/webview/live/live_info
            auto WebviewLiveLiveInfoWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(
                    WebviewLiveApi_klass, "<WebviewLiveLiveInfoWithHttpInfoAsync>d__22");
            method = Il2cppUtils::GetMethodIl2cpp(WebviewLiveLiveInfoWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                WebviewLiveApi_WebviewLiveLiveInfoWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }
#pragma endregion
#pragma region ActivityTopApi
        auto ActivityTopApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ActivityRecordApi");
        method = (Il2cppUtils::MethodInfo*) nullptr;
        if (ActivityTopApi_klass) {
            auto ActivityRecordGetTopWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(
                    ActivityTopApi_klass, "<ActivityRecordGetTopWithHttpInfoAsync>d__18");
            method = Il2cppUtils::GetMethodIl2cpp(ActivityRecordGetTopWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ActivityRecordGetTopWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }

#pragma region RenderScene
        ADD_HOOK(AlstArchiveDirectory_GetLocalFullPathFromFileName, Il2cppUtils::GetMethodPointer("Core.dll", "Alstromeria", "AlstArchiveDirectory", "GetRemoteUriFromFileName"));
        ADD_HOOK(FesliveApi_FesliveEnterWithHttpInfoAsync , Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "FesliveApi", "FesliveEnterWithHttpInfoAsync"));
        ADD_HOOK(WithliveApi_WithliveEnterWithHttpInfoAsync , Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "WithliveApi", "WithliveEnterWithHttpInfoAsync"));
        ADD_HOOK(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveGetFesArchiveDataWithHttpInfoAsync"));
        ADD_HOOK(ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveGetWithArchiveDataWithHttpInfoAsync"));
        ADD_HOOK(ArchiveApi_ArchiveGetArchiveListWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveGetArchiveListWithHttpInfoAsync"));
#pragma endregion
#pragma region oldVersion
        ADD_HOOK(Configuration_AddDefaultHeader, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client", "Configuration", "AddDefaultHeader"));
        ADD_HOOK(Configuration_set_UserAgent, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client", "Configuration", "set_UserAgent"));
//        ADD_HOOK(AssetManager_SynchronizeResourceVersion, Il2cppUtils::GetMethodPointer("Core.dll", "Hailstorm", "AssetManager", "SynchronizeResourceVersion"));
        ADD_HOOK(Core_SynchronizeResourceVersion, Il2cppUtils::GetMethodPointer("Core.dll", "", "Core", "SynchronizeResourceVersion"));
        ADD_HOOK(Application_get_version, Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Application::get_version"));
        ADD_HOOK(ArchiveApi_ArchiveGetWithTimelineDataWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveGetWithTimelineDataWithHttpInfoAsync"));
        ADD_HOOK(ArchiveApi_ArchiveGetFesTimelineDataWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveGetFesTimelineDataWithHttpInfoAsync"));
        //        auto AssetManager_klass = Il2cppUtils::GetClassIl2cpp("Core.dll", "Hailstorm", "AssetManager");
//        if (AssetManager_klass) {
//            auto SynchronizeResourceVersion_klss = Il2cppUtils::find_nested_class_from_name(AssetManager_klass, "<SynchronizeResourceVersion>d__22");
//            method = Il2cppUtils::GetMethodIl2cpp(SynchronizeResourceVersion_klss, "MoveNext", 0);
//            if (method) {
//                ADD_HOOK(AssetManager_SynchronizeResourceVersion_MoveNext, method->methodPointer);
//            }
//        }
        ADD_HOOK(Silverflame_SFL_AdvSeriesMaster_ctor, Il2cppUtils::GetMethodPointer("Core.dll", "Silverflame.SFL", "AdvSeriesMaster", ".ctor"));
        ADD_HOOK(Silverflame_SFL_AdvSeriesMaster_Fetch, Il2cppUtils::GetMethodPointer("Core.dll", "Silverflame.SFL", "AdvSeriesMaster", "Fetch"));
        ADD_HOOK(Silverflame_SFL_AdvDatasMaster_ctor, Il2cppUtils::GetMethodPointer("Core.dll", "Silverflame.SFL", "AdvDatasMaster", ".ctor"));
        ADD_HOOK(Silverflame_SFL_AdvDatasMaster_Fetch, Il2cppUtils::GetMethodPointer("Core.dll", "Silverflame.SFL", "AdvDatasMaster", "Fetch"));
#pragma endregion
    }
}
