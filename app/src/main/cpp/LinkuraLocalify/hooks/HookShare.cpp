#include "../HookMain.h"
#include "../Misc.hpp"
#include <re2/re2.h>

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
    // http response modify
    DEFINE_HOOK(void* , ApiClient_Deserialize, (void* self, void* response, void* type, void* method_info)) {
        auto result = ApiClient_Deserialize_Orig(self, response, type, method_info);
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(result)->ToString());
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
//        IF_CALLER_WITHIN(WithliveApi_WithliveLiveInfoWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
//            if (Config::unlockAfter) {
////                json["has_admission"] = "true";
//                json["has_extra_admission"] = "true";
//            }
//            result = Il2cppUtils::FromJsonStr(json.dump(), type);
//        }
//        IF_CALLER_WITHIN(FesliveApi_FesliveLiveInfoWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
////            if (Config::unlockAfter) {
////                json["has_admission"] = "true";
////            }
//            result = Il2cppUtils::FromJsonStr(json.dump(), type);
//        }
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
            requestedVersion = Il2cppUtils::Il2CppString::New(Config::currentResVersion);
        }
        return Core_SynchronizeResourceVersion_Orig(self, requestedVersion, mtd);
    }
#pragma region

    void Install(HookInstaller* hookInstaller) {

        // GetHttpAsyncAddr
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
#pragma endregion
    }
}
