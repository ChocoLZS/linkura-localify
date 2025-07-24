#include "../HookMain.h"
#include "../Misc.hpp"

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

#pragma region HttpRequests
    uintptr_t ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    uintptr_t ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr = 0;
    // http response modify
    DEFINE_HOOK(void* , ApiClient_Deserialize, (void* self, void* response, void* type, void* method_info)) {
        auto result = ApiClient_Deserialize_Orig(self, response, type, method_info);
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(result)->ToString());
        auto caller = __builtin_return_address(0);
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::fesArchiveUnlockTicket) {
                json["selectable_camera_types"] = {1,2,3,4};
                json["ticket_rank"] = 6;
                json["has_extra_admission"] = "true";
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetArchiveList_MoveNext_Addr, caller, 3000) {
            for (auto archive : json["archive_list"]) {
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
            }
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
