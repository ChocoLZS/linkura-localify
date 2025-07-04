#include "Hook.h"
#include "Plugin.h"
#include "Log.h"
#include "../deps/UnityResolve/UnityResolve.hpp"
#include "Il2cppUtils.hpp"
#include "Local.h"
#include "MasterLocal.h"
#include <unordered_set>
#include "camera/camera.hpp"
#include "config/Config.hpp"
// #include <jni.h>
#include <thread>
#include <map>
#include <set>
#include "../platformDefine.hpp"
#include <nlohmann/json.hpp>
#include <string_view>
#include <locale>
#include <codecvt>

#ifdef GKMS_WINDOWS
    #include "../windowsPlatform.hpp"
    #include "cpprest/details/http_helpers.h"
    #include "../resourceUpdate/resourceUpdate.hpp"
#endif


std::unordered_set<void*> hookedStubs{};
extern std::filesystem::path gakumasLocalPath;

/*
void UnHookAll() {
    for (const auto i: hookedStubs) {
        int result = shadowhook_unhook(i);
        if(result != 0)
        {
            int error_num = shadowhook_get_errno();
            const char *error_msg = shadowhook_to_errmsg(error_num);
            LinkuraLocal::Log::ErrorFmt("unhook failed: %d - %s", error_num, error_msg);
        }
    }
}*/

namespace LinkuraLocal::HookMain {
    using Il2cppString = UnityResolve::UnityType::String;

    UnityResolve::UnityType::String* environment_get_stacktrace() {
        /*
        static auto mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System",
                                                 "Environment", "get_StackTrace");
        return mtd->Invoke<UnityResolve::UnityType::String*>();*/
        const auto pClass = Il2cppUtils::GetClass("mscorlib.dll", "System.Diagnostics",
                                                  "StackTrace");

        const auto ctor_mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System.Diagnostics",
                                                     "StackTrace", ".ctor");
        const auto toString_mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System.Diagnostics",
                                                         "StackTrace", "ToString");

        const auto klassInstance = pClass->New<void*>();
        ctor_mtd->Invoke<void>(klassInstance);
        return toString_mtd->Invoke<Il2cppString*>(klassInstance);
    }

    DEFINE_HOOK(void, Internal_LogException, (void* ex, void* obj)) {
        Internal_LogException_Orig(ex, obj);
        static auto Exception_ToString = Il2cppUtils::GetMethod("mscorlib.dll", "System", "Exception", "ToString");
        Log::LogUnityLog(ANDROID_LOG_ERROR, "UnityLog - Internal_LogException:\n%s", Exception_ToString->Invoke<Il2cppString*>(ex)->ToString().c_str());
    }

    DEFINE_HOOK(void, Internal_Log, (int logType, int logOption, UnityResolve::UnityType::String* content, void* context)) {
        Internal_Log_Orig(logType, logOption, content, context);
        // 2022.3.21f1
        Log::LogUnityLog(ANDROID_LOG_VERBOSE, "Internal_Log:\n%s", content->ToString().c_str());
    }

    enum struct SchoolResolution_LiveAreaQuality {
        Low,
        Middle,
        High
    };

    enum struct LiveScreenOrientation {
        Landscape,
        Portrait
    };

    // 0-3 width
    // 4-7 height
    // 8-11 refresh_rate: always 0
    DEFINE_HOOK(u_int64_t, SchoolResolution_GetResolution, (SchoolResolution_LiveAreaQuality quality, LiveScreenOrientation orientation)) {
        auto result = SchoolResolution_GetResolution_Orig(quality, orientation);
        if (Config::renderHighResolution) {
            u_int64_t width = 1920, height = 1080;
            switch (quality) {
                case SchoolResolution_LiveAreaQuality::Low: // 1080p
                    width = orientation == LiveScreenOrientation::Landscape ? 1920 : 1080;
                    height = orientation == LiveScreenOrientation::Landscape ? 1080 : 1920;
                    break;
                case SchoolResolution_LiveAreaQuality::Middle: // 2k
                    width = orientation == LiveScreenOrientation::Landscape ? 2560 : 1440;
                    height = orientation == LiveScreenOrientation::Landscape ? 1440 : 2560;
                    break;
                case SchoolResolution_LiveAreaQuality::High: // 4k
                    width = orientation == LiveScreenOrientation::Landscape ? 3840 : 2160;
                    height = orientation == LiveScreenOrientation::Landscape ? 2160 : 3840;
                    break;
                default:
                    width = result & 0xffffffff;
                    height = result >> 32;
                    break;
            }
            result = (u_int64_t)(height << 32 | width);
            Log::DebugFmt("切换分辨率至: %d x %d, 返回结果: %p", width, height, result);
        }
        return result;
    }

    // cheat for server api, but we need to decrease the abnormal behaviour here. ( camera_type should change when every request sends )
    DEFINE_HOOK(void* ,ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, (void* _this, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
        if (Config::fesArchiveUnlockTicket) {
            auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(request)->ToString());
            json["camera_type"] = 1;
            if (json.contains("focus_character_id")){
                json.erase("focus_character_id");
            }
            request = static_cast<Il2cppUtils::Il2CppObject*>(
                    Il2cppUtils::FromJsonStr(json.dump(), Il2cppUtils::get_system_type_from_instance(request))
            );
        }
        return ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync_Orig(_this,
                                                                    request,
                                                                    cancellation_token, method_info);
    }

    uintptr_t ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    DEFINE_HOOK(void* , ApiClient_Deserialize, (void* _this, void* response, void* type, void* method_info)) {
        auto result = ApiClient_Deserialize_Orig(_this, response, type, method_info);
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(result)->ToString());
        auto caller = __builtin_return_address(0);
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::fesArchiveUnlockTicket) {
                Log::DebugFmt(
                        "ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext HOOKED");
                json["selectable_camera_types"] = {1,2,3,4};
                json["ticket_rank"] = 6;
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }
        
        return result;
    }

     bool IsNativeObjectAlive(void* obj) {
         static UnityResolve::Method* IsNativeObjectAliveMtd = nullptr;
         if (!IsNativeObjectAliveMtd) IsNativeObjectAliveMtd = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                                      "Object", "IsNativeObjectAlive");
         return IsNativeObjectAliveMtd->Invoke<bool>(obj);
     }

     DEFINE_HOOK(void, Unity_set_targetFrameRate, (int value)) {
         const auto configFps = Config::targetFrameRate;
         return Unity_set_targetFrameRate_Orig(configFps == 0 ? value: configFps);
     }

    void StartInjectFunctions() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

        UnityResolve::Init(xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_NOW),
            UnityResolve::Mode::Il2Cpp, Config::lazyInit);

        ADD_HOOK(SchoolResolution_GetResolution, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain",
                                                                      "SchoolResolution", "GetResolution"));
        // Fes live camera unlock
        ADD_HOOK(ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveSetFesCameraWithHttpInfoAsync"));

#pragma region GetHttpAsyncAddr
        ADD_HOOK(ApiClient_Deserialize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client","ApiClient", "Deserialize"));
        auto ArchiveApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi");
        if (ArchiveApi_klass) {
            // hook /v1/archive/get_fes_archive_data response
            auto ArchiveGetFesArchiveDataWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetFesArchiveDataWithHttpInfoAsync>d__30");
            Log::DebugFmt("ArchiveGetWithArchiveDataWithHttpInfoAsync_klass name is: %s", static_cast<Il2cppUtils::Il2CppClassHead*>(ArchiveGetFesArchiveDataWithHttpInfoAsync_klass)->name);
            auto method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetFesArchiveDataWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }
#pragma endregion

        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));

        // ADD_HOOK(Unity_set_position_Injected, Il2cppUtils::il2cpp_resolve_icall(
        //         "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)"));
        // ADD_HOOK(Unity_set_rotation_Injected, Il2cppUtils::il2cpp_resolve_icall(
        //         "UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)"));
        // ADD_HOOK(Unity_get_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
        //                                                               "Camera", "get_fieldOfView"));
        // ADD_HOOK(Unity_set_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
        //                                                               "Camera", "set_fieldOfView"));
        ADD_HOOK(Unity_set_targetFrameRate, Il2cppUtils::il2cpp_resolve_icall(
                 "UnityEngine.Application::set_targetFrameRate(System.Int32)"));
        // ADD_HOOK(EndCameraRendering, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine.Rendering",
        //                                                              "RenderPipeline", "EndCameraRendering"));


    }
    // 77 2640 5000

    DEFINE_HOOK(int, il2cpp_init, (const char* domain_name)) {
#ifndef GKMS_WINDOWS
         const auto ret = il2cpp_init_Orig(domain_name);
#else
//        const auto ret = 0;
#endif
        // InjectFunctions();

        Log::Info("Waiting for config...");

        while (!Config::isConfigInit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!Config::enabled) {
            Log::Info("Plugin not enabled");
            return ret;
        }

        Log::Info("Start init plugin...");

        if (Config::lazyInit) {
            UnityResolveProgress::startInit = true;
            UnityResolveProgress::assembliesProgress.total = 2;
            UnityResolveProgress::assembliesProgress.current = 1;
            UnityResolveProgress::classProgress.total = 36;
            UnityResolveProgress::classProgress.current = 0;
        }

        StartInjectFunctions();

        if (Config::lazyInit) {
            UnityResolveProgress::assembliesProgress.current = 2;
            UnityResolveProgress::classProgress.total = 1;
            UnityResolveProgress::classProgress.current = 0;
        }

        UnityResolveProgress::startInit = false;

        Log::Info("Plugin init finished.");
        return ret;
    }
}


namespace LinkuraLocal::Hook {
    void Install() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

        Log::Info("Installing hook");

#ifndef GKMS_WINDOWS
        ADD_HOOK(HookMain::il2cpp_init,
            Plugin::GetInstance().GetHookInstaller()->LookupSymbol("il2cpp_init"));
#else
        HookMain::il2cpp_init_Hook(nullptr);
#endif


        Log::Info("Hook installed");
    }
}
