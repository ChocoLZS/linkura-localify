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
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
            }
            if (Config::fesArchiveUnlockTicket) {
                json["selectable_camera_types"] = {1,2,3,4};
                json["ticket_rank"] = 6;
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
            if (Config::enableMotionCaptureReplay) {
                auto archive_id = Shareable::currentArchiveId;
                auto it = Config::archiveConfigMap.find(archive_id);
                if (it == Config::archiveConfigMap.end()) return result;
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
                        auto new_external_link = replaceExternalLinkUrl(external_link, assets_url);
                        json["archive_url"] = new_external_link;
                        if (new_external_link.ends_with(".iarc")) Log::ShowToast("The motion replay before 2025.05.29 can't be replayed for now!");
                    }
                }
                if (replay_type == 2) {
                    json.erase("video_url");
                    if (!external_fix_link.empty()) {
                        auto new_external_fix_link = replaceExternalLinkUrl(external_fix_link, assets_url);
                        json["archive_url"] = new_external_fix_link;
                    }
                }
            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetWithArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_with_archive_data response
            if (Config::withliveOrientation == (int)HookLiveRender::LiveScreenOrientation::Landscape) {
                json["is_horizontal"] = "true";
            }
            if (Config::withliveOrientation == (int)HookLiveRender::LiveScreenOrientation::Portrait) {
                json["is_horizontal"] = "false";
            }
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
            }
            if (Config::enableMotionCaptureReplay) {
                auto archive_id = Shareable::currentArchiveId;
                auto it = Config::archiveConfigMap.find(archive_id);
                if (it == Config::archiveConfigMap.end()) return result;
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
                        auto new_external_link = replaceExternalLinkUrl(external_link, assets_url);
                        json["archive_url"] = new_external_link;
                        if (new_external_link.ends_with(".iarc")) Log::ShowToast("The motion replay before 2025.05.29 can't be replayed for now!");
                    }
                }
                if (replay_type == 2) {
                    json.erase("video_url");
                    if (!external_fix_link.empty()) {
                        auto new_external_fix_link = replaceExternalLinkUrl(external_fix_link, assets_url);
                        json["archive_url"] = new_external_fix_link;
                    }
                }
            }
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
        IF_CALLER_WITHIN(FesliveApi_FesliveEnterWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr, caller, 3000) { // hook /v1/archive/get_archive_list response
            for (auto& archive : json["archive_list"]) {
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
                    if (it != Config::archiveConfigMap.end()) {
                        if (replay_type == 1) { // motion capture replay
                            archive_title = "✅ " + archive_title;
                        }
                        if (replay_type == 2) {
                            archive_title = "☑️ " + archive_title;
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
            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(FesliveApi_FesliveLiveInfoWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
            }
            result = Il2cppUtils::FromJsonStr(json.dump(), type);
        }
        IF_CALLER_WITHIN(WebviewLiveApi_WebviewLiveLiveInfoWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::unlockAfter) {
                json["has_extra_admission"] = "true";
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

    DEFINE_HOOK(Il2cppUtils::Il2CppString* , AlstArchiveDirectory_GetLocalFullPathFromFileName, (Il2cppUtils::Il2CppObject* self, Il2cppUtils::Il2CppString* fileName)) {
        auto result = AlstArchiveDirectory_GetLocalFullPathFromFileName_Orig(self, fileName);
        auto result_str = result->ToString();
        if (Config::enableMotionCaptureReplay) {
            auto archive_id = Shareable::currentArchiveId;
            auto it = Config::archiveConfigMap.find(archive_id);
            if (it == Config::archiveConfigMap.end()) return result;
            auto archive_config = it->second;
            auto replay_type = archive_config["replay_type"].get<uint>();
            if (replay_type == 1) { // replay
                auto new_result_str = replaceExternalLinkUrl(result_str, Config::motionCaptureResourceUrl);
                result = Il2cppUtils::Il2CppString::New(new_result_str);
            }
        }
        return result;
    }

#pragma endregion

#pragma region oldVersion
    DEFINE_HOOK(void, Configuration_AddDefaultHeader, (void* self, Il2cppUtils::Il2CppString* key, Il2cppUtils::Il2CppString* value, void* mtd)) {
        Log::DebugFmt("Configuration_AddDefaultHeader HOOKED, %s=%s", key->ToString().c_str(), value->ToString().c_str());
        auto key_str = key->ToString();
        auto value_str = value->ToString();
        if (key_str == "x-client-version") {
            value = Il2cppUtils::Il2CppString::New("4.5.0");
        }
        if (key_str == "x-res-version") {
            value = Il2cppUtils::Il2CppString::New("R2508150");
        }
        Configuration_AddDefaultHeader_Orig(self, key, value ,mtd);
    }

    DEFINE_HOOK(void, Configuration_set_UserAgent, (void* self, Il2cppUtils::Il2CppString* value, void* mtd)) {
        Log::DebugFmt("Configuration_set_UserAgent HOOKED, %s", value->ToString().c_str());
        auto value_str = value->ToString();
        if (value_str.starts_with("inspix-android")) {
            value = Il2cppUtils::Il2CppString::New("inspix-android/4.5.0");
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
    DEFINE_HOOK(void*, AssetManager_SynchronizeResourceVersion, (void* self, Il2cppUtils::Il2CppString* requestedVersion, Il2cppUtils::Il2CppString* savedVersion, void* mtd)) {
        Log::DebugFmt("Hailstorm_AssetManager__SynchronizeResourceVersion HOOKED, requestedVersion is %s, savedVersion is %s", requestedVersion->ToString().c_str(), savedVersion->ToString().c_str());
//        auto hooked_requestedVersion = Il2cppUtils::Il2CppString::New("R2504300@hbZZOCoWTueF+rikQLgPapC2Qw==");
        return AssetManager_SynchronizeResourceVersion_Orig(self, requestedVersion, savedVersion, mtd);
    }

    // Core_SynchronizeResourceVersion -> AssetManager_SynchronizeResourceVersion
    DEFINE_HOOK(void* ,Core_SynchronizeResourceVersion, (void* self, Il2cppUtils::Il2CppString* requestedVersion,  void* mtd)) {
        Log::DebugFmt("Core_SynchronizeResourceVersion HOOKED, requestedVersion is %s", requestedVersion->ToString().c_str());
        auto hooked_requestedVersion = Il2cppUtils::Il2CppString::New("R2504300@hbZZOCoWTueF+rikQLgPapC2Qw==");
        return Core_SynchronizeResourceVersion_Orig(self, hooked_requestedVersion, mtd);
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
            // hook /v1/archive/get_archive_list response
            auto ArchiveGetArchiveListWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetArchiveListWithHttpInfoAsync>d__18");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetArchiveListWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr = method->methodPointer;
            }
            // hook /v1/archive/withlive_info
            auto ArchiveWithliveInfoWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveWithliveInfoWithHttpInfoAsync>d__70");
            method = Il2cppUtils::GetMethodIl2cpp(ArchiveWithliveInfoWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveWithliveInfoWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }
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

#pragma endregion
#pragma region oldVersion
        ADD_HOOK(Configuration_AddDefaultHeader, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client", "Configuration", "AddDefaultHeader"));
        ADD_HOOK(Configuration_set_UserAgent, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client", "Configuration", "set_UserAgent"));
//        ADD_HOOK(AssetManager_SynchronizeResourceVersion, Il2cppUtils::GetMethodPointer("Core.dll", "Hailstorm", "AssetManager", "SynchronizeResourceVersion"));
        ADD_HOOK(Core_SynchronizeResourceVersion, Il2cppUtils::GetMethodPointer("Core.dll", "", "Core", "SynchronizeResourceVersion"));
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
