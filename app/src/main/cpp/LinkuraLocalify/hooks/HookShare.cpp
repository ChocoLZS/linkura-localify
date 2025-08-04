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
    std::string replaceExternalLinkUrl(const std::string& external_link, const std::string& assets_url) {
        // 使用RE2正则表达式匹配URL模式
        // 匹配 https://foo.example.org 这样的URL，并考虑路径部分
        std::string pattern = R"(^https://[^/]+(/.*)?$)";
        RE2 re(pattern);
        
        if (!re.ok()) {
            Log::DebugFmt("RE2 compile failed for pattern: %s, error: %s", pattern.c_str(), re.error().c_str());
            return external_link;
        }
        
        std::string path_part;
        if (RE2::FullMatch(external_link, re, &path_part)) {
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
            Log::DebugFmt("URL replaced: %s -> %s", external_link.c_str(), result.c_str());
            return result;
        } else {
            std::string result = assets_url;
            if (!external_link.empty()) {
                if (!assets_url.empty() && assets_url.back() == '/' && external_link.front() == '/') {
                    result += external_link.substr(1);
                } else if (!assets_url.empty() && assets_url.back() != '/' && external_link.front() != '/') {
                    result += "/" + external_link;
                } else {
                    result += external_link;
                }
            }
            Log::DebugFmt("Path combined with assets_url: %s -> %s", external_link.c_str(), result.c_str());
            return result;
        }
    }

#pragma region HttpRequests
    uintptr_t ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr = 0;
    // http response modify
    DEFINE_HOOK(void* , ApiClient_Deserialize, (void* self, void* response, void* type, void* method_info)) {
        auto result = ApiClient_Deserialize_Orig(self, response, type, method_info);
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(result)->ToString());
        auto caller = __builtin_return_address(0);
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_fes_archive_data response
            if (Config::fesArchiveUnlockTicket) {
                json["selectable_camera_types"] = {1,2,3,4};
                json["ticket_rank"] = 6;
                json["has_extra_admission"] = "true";
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
            if (Config::enableMotionCaptureReplay) {
                auto archive_id = Shareable::currentArchiveId;
                auto it = Config::archiveConfigMap.find(archive_id);
                if (it == Config::archiveConfigMap.end()) return result;
                auto archive_config = it->second;
                auto replay_type = archive_config["replay_type"].get<uint>();
                auto external_link = archive_config["external_link"].get<std::string>();
                auto video_url = archive_config["video_url"].get<std::string>();
                auto assets_url = Config::motionCaptureResourceUrl;
                if (replay_type == 0) {
                    json.erase("archive_url");
                    if (!video_url.empty()) {
                        auto new_video_url = replaceExternalLinkUrl(video_url, assets_url);
                        json["video_url"] = new_video_url;
                    }
                }
                if (replay_type == 1) {
                    json.erase("video_url");
                    if (!external_link.empty()) {
                        auto new_external_link = replaceExternalLinkUrl(external_link, assets_url);
                        json["archive_url"] = new_external_link;
                    }
                }
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_with_archive_data response
            if (Config::enableMotionCaptureReplay) {
                auto archive_id = Shareable::currentArchiveId;
                auto it = Config::archiveConfigMap.find(archive_id);
                if (it == Config::archiveConfigMap.end()) return result;
                auto archive_config = it->second;
                auto replay_type = archive_config["replay_type"].get<uint>();
                auto external_link = archive_config["external_link"].get<std::string>();
                auto video_url = archive_config["video_url"].get<std::string>();
                auto assets_url = Config::motionCaptureResourceUrl;
                if (replay_type == 0) {
                    json.erase("archive_url");
                    if (!video_url.empty()) {
                        auto new_video_url = replaceExternalLinkUrl(video_url, assets_url);
                        json["video_url"] = new_video_url;
                    }
                }
                if (replay_type == 1) {
                    json.erase("video_url");
                    if (!external_link.empty()) {
                        auto new_external_link = replaceExternalLinkUrl(external_link, assets_url);
                        json["archive_url"] = new_external_link;
                    }
                }
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_archive_list response
            for (auto& archive : json["archive_list"]) {
                auto archive_id = archive["archives_id"].get<std::string>();
                if (Shareable::archiveData.find(archive_id) == Shareable::archiveData.end()) {
                    auto live_start_time = archive["live_start_time"].get<std::string>();
                    auto live_end_time = archive["live_end_time"].get<std::string>();
                    auto duration = LinkuraLocal::Misc::Time::parseISOTime(live_end_time) - LinkuraLocal::Misc::Time::parseISOTime(live_start_time);
                    Shareable::archiveData[archive_id] = {
                            .duration = duration
                    };
                    Log::VerboseFmt("archives id is %s, duration is %lld", archive_id.c_str(), duration);
                }
                if (Config::enableMotionCaptureReplay && Config::enableInGameReplayDisplay) {
                    auto it = Config::archiveConfigMap.find(archive_id);
                    if (it == Config::archiveConfigMap.end()) continue;
                    auto archive_config = it->second;
                    auto replay_type = archive_config["replay_type"].get<uint>();
                    auto archive_title = archive["name"].get<std::string>();
                    if (it != Config::archiveConfigMap.end()) {
                        if (replay_type == 1) { // motion capture replay
                            auto external_link = archive_config["external_link"].get<std::string>();
                            if (external_link.contains("fix")) {
                                archive_title = "☑️ " + archive_title;
                            } else {
                                archive_title = "✅ " + archive_title;
                            }
                        }
                        if (replay_type == 0) { // video replay
                            archive_title = "📺 " + archive_title;
                        }
                        archive["name"] = archive_title;
                    }
                }

            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }

        return result;
    }

    DEFINE_HOOK(void* , ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Log::DebugFmt("ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync HOOKED");
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(request)->ToString());
        Shareable::currentArchiveId = json["archives_id"].get<std::string>();
        Shareable::renderScene = Shareable::RenderScene::FesLive;
        return ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_Orig(self,
                                                                         request,
                                                                         cancellation_token, method_info);
    }
    DEFINE_HOOK(void* , ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        Log::DebugFmt("ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync HOOKED");
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(request)->ToString());
        Shareable::currentArchiveId = json["archives_id"].get<std::string>();
        Shareable::renderScene = Shareable::RenderScene::WithLive;
        return ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_Orig(self,
                                                                          request,
                                                                          cancellation_token, method_info);
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

#pragma endregion

    void Install(HookInstaller* hookInstaller) {

        // GetHttpAsyncAddr
        ADD_HOOK(ApiClient_Deserialize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client","ApiClient", "Deserialize"));
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
            // hook /v1/archive/get_archive_list response
            auto ArchiveGetArchiveListWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetArchiveListWithHttpInfoAsync>d__18");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetArchiveListWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr = method->methodPointer;
            }
        }
#pragma region RenderScene

        ADD_HOOK(FesliveApi_FesliveEnterWithHttpInfoAsync , Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "FesliveApi", "FesliveEnterWithHttpInfoAsync"));
        ADD_HOOK(WithliveApi_WithliveEnterWithHttpInfoAsync , Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "WithliveApi", "WithliveEnterWithHttpInfoAsync"));
        ADD_HOOK(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveGetFesArchiveDataWithHttpInfoAsync"));
        ADD_HOOK(ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveGetWithArchiveDataWithHttpInfoAsync"));


#pragma endregion
    }
}
