#include "../HookMain.h"


namespace LinkuraLocal::HookLiveRender {
    enum struct SchoolResolution_LiveAreaQuality {
        Low,
        Middle,
        High
    };
    enum struct LiveScreenOrientation {
        Landscape,
        Portrait
    };

    void* realtimeRenderingArchiveControllerCache = nullptr;

    DEFINE_HOOK(void* , RealtimeRenderingArchiveController_SetPlayPositionAsync, (void* self, float seconds)) {
        Log::DebugFmt("RealtimeRenderingArchiveController_SetPlayPositionAsync HOOKED: seconds is %f", seconds);
        realtimeRenderingArchiveControllerCache = self;
        return RealtimeRenderingArchiveController_SetPlayPositionAsync_Orig(self, seconds);
    }

    DEFINE_HOOK(u_int64_t, SchoolResolution_GetResolution, (SchoolResolution_LiveAreaQuality quality, LiveScreenOrientation orientation)) {
        auto result = SchoolResolution_GetResolution_Orig(quality, orientation);
        if (Config::renderHighResolution) {
            u_int64_t width = 1920, height = 1080;
            switch (quality) {
                case SchoolResolution_LiveAreaQuality::Low: // 1080p
                    width = orientation == LiveScreenOrientation::Landscape ? 1920 : 1080;
                    height = orientation == LiveScreenOrientation::Landscape ? 1080 : 1920;
                    if (realtimeRenderingArchiveControllerCache) RealtimeRenderingArchiveController_SetPlayPositionAsync_Orig(realtimeRenderingArchiveControllerCache, 1920);
                    break;
                case SchoolResolution_LiveAreaQuality::Middle: // 2k
                    width = orientation == LiveScreenOrientation::Landscape ? 2560 : 1440;
                    height = orientation == LiveScreenOrientation::Landscape ? 1440 : 2560;
                    if (realtimeRenderingArchiveControllerCache) RealtimeRenderingArchiveController_SetPlayPositionAsync_Orig(realtimeRenderingArchiveControllerCache, 2560);
                    break;
                case SchoolResolution_LiveAreaQuality::High: // 4k
                    width = orientation == LiveScreenOrientation::Landscape ? 3840 : 2160;
                    height = orientation == LiveScreenOrientation::Landscape ? 2160 : 3840;
                    if (realtimeRenderingArchiveControllerCache) RealtimeRenderingArchiveController_SetPlayPositionAsync_Orig(realtimeRenderingArchiveControllerCache, 3840);
                    break;
                default:
                    width = result & 0xffffffff;
                    height = result >> 32;
                    break;
            }
            result = (u_int64_t)(height << 32 | width);
        }
        return result;
    }

    // cheat for server api, but we need to decrease the abnormal behaviour here. ( camera_type should change when every request sends )
    DEFINE_HOOK(void* ,ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, (void* self, Il2cppUtils::Il2CppObject* request, void* cancellation_token, void* method_info)) {
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
        return ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync_Orig(self,
                                                                    request,
                                                                    cancellation_token, method_info);
    }

    uintptr_t ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    DEFINE_HOOK(void* , ApiClient_Deserialize, (void* self, void* response, void* type, void* method_info)) {
        auto result = ApiClient_Deserialize_Orig(self, response, type, method_info);
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(result)->ToString());
        auto caller = __builtin_return_address(0);
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::fesArchiveUnlockTicket) {
                Log::DebugFmt(
                        "ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext HOOKED");
                json["selectable_camera_types"] = {1,2,3,4};
                json["ticket_rank"] = 6;
                json["has_extra_admission"] = "true";
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }

        return result;
    }

    /**
     * @brief set target frame rate for unity engine
     */
    DEFINE_HOOK(void, Unity_set_targetFrameRate, (int value)) {
        const auto configFps = Config::targetFrameRate;
        return Unity_set_targetFrameRate_Orig(configFps == 0 ? value: configFps);
    }

    // 👀
    DEFINE_HOOK(void, CoverImageCommandReceiver_Awake, (Il2cppUtils::Il2CppObject* self, void* method)) {
        CoverImageCommandReceiver_Awake_Orig(self, method);
        Log::DebugFmt("CoverImageCommandReceiver_Awake HOOKED");
    }

    DEFINE_HOOK(void*, FesConnectArchivePlayer_get_CurrentTime, (void* self)) {
//        static auto TimeSpan_klass = Il2cppUtils::GetClass("mscorlib.dll", "System", "TimeSpan");
//        static auto _ticks_field = TimeSpan_klass->Get<UnityResolve::Field>("_ticks");
        static auto TimeSpan_get_TotalSeconds = Il2cppUtils::GetMethod("mscorlib.dll", "System", "TimeSpan", "get_TotalSeconds");
        Log::DebugFmt("FesConnectArchivePlayer_get_CurrentTime HOOKED");
        auto timeSpan = FesConnectArchivePlayer_get_CurrentTime_Orig(self);
        auto seconds = TimeSpan_get_TotalSeconds->Invoke<double>(timeSpan);
        Log::DebugFmt("FesConnectArchivePlayer_get_CurrentTime seconds %f", seconds);
        return timeSpan;
    }

    DEFINE_HOOK(void*, FesConnectArchivePlayer_get_RunningTime, (void* self)) {
//        static auto TimeSpan_klass = Il2cppUtils::GetClass("mscorlib.dll", "System", "TimeSpan");
//        static auto _ticks_field = TimeSpan_klass->Get<UnityResolve::Field>("_ticks");
        static auto TimeSpan_get_TotalSeconds = Il2cppUtils::GetMethod("mscorlib.dll", "System", "TimeSpan", "get_TotalSeconds");
        Log::DebugFmt("FesConnectArchivePlayer_get_RunningTime HOOKED");
        auto timeSpan = FesConnectArchivePlayer_get_RunningTime_Orig(self);
        auto seconds = TimeSpan_get_TotalSeconds->Invoke<double>(timeSpan);
        Log::DebugFmt("FesConnectArchivePlayer_get_RunningTime seconds %f", seconds);
        return timeSpan;
    }

    DEFINE_HOOK(void, FesConnectArchivePlayer_JumpTimeAsync_MoveNext, (void* self, void* method)) {
        Log::DebugFmt("FesConnectArchivePlayer_JumpTimeAsync_MoveNext HOOKED");
        FesConnectArchivePlayer_JumpTimeAsync_MoveNext_Orig(self, method);
    }

    DEFINE_HOOK(void*, FesConnectArchivePlayer_JumpTimeAsync, (void* self, void* timespan, void* method)) {
//        Log::DebugFmt("FesConnectArchivePlayer_JumpTimeAsync HOOKED");
//        static auto TimeSpan_klass = Il2cppUtils::get_system_class_from_reflection_type_str("System.TimeSpan");
//        Log::DebugFmt("FesConnectArchivePlayer_JumpTimeAsync TimeSpan_klass: %p", TimeSpan_klass);
//        static auto TimeSpan_ctor_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(TimeSpan_klass, ".ctor", 5);
//        Log::DebugFmt("FesConnectArchivePlayer_JumpTimeAsync TimeSpan_ctor_mtd: %p", TimeSpan_ctor_mtd);
//        static auto TimeSpan_ctor = reinterpret_cast<void (*)(void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(TimeSpan_ctor_mtd->methodPointer);
//        Log::DebugFmt("FesConnectArchivePlayer_JumpTimeAsync TimeSpan_ctor: %p", TimeSpan_ctor);
//        auto newTimeSpan = UnityResolve::Invoke<void*>("il2cpp_object_new", TimeSpan_klass);
//        TimeSpan_ctor(newTimeSpan, 25, 25, 25, 25, 25);
//        Log::DebugFmt("new timespan is created at %p", newTimeSpan);

        static auto uTimeSpan_klass = Il2cppUtils::GetClass("mscorlib.dll", "System", "TimeSpan");
        static auto uTimeSpan_ctor = Il2cppUtils::GetMethod("mscorlib.dll", "System", "TimeSpan", ".ctor", {"System.Int32","System.Int32","System.Int32","System.Int32","System.Int32"});
//        const auto uNewTimeSpan = uTimeSpan_klass->New<void*>();
//        uTimeSpan_ctor->Invoke<void>(uNewTimeSpan, 25, 25, 25, 25, 25);


//        Log::DebugFmt("FesConnectArchivePlayer_JumpTimeAsync _ticks_field: %p", _ticks_field);
//        auto ticks = Il2cppUtils::ClassGetFieldValue<long>(newTimeSpan, _ticks_field);
//        Log::DebugFmt("new timespan's ticks is %lld", ticks);
//        auto u_ticks = Il2cppUtils::ClassGetFieldValue<long>(uNewTimeSpan, _ticks_field);
//        Log::DebugFmt("uNewTimeSpan's ticks is %lld", u_ticks);

//        Il2cppUtils::ClassSetFieldValue<long>(newTimeSpan, _ticks_field, 25* 60 * 1000);
//        Il2cppUtils::ClassSetFieldValue<long>(uNewTimeSpan, _ticks_field, 25 * 60 * 1000);
//        static auto TimeSpan_FromMilliseconds = Il2cppUtils::GetMethod("mscorlib.dll", "System", "TimeSpan", "FromMilliseconds");
//        Log::DebugFmt("FesConnectArchivePlayer_JumpTimeAsync TimeSpan_FromMilliseconds: %p", TimeSpan_FromMilliseconds);
//        auto staticTimeSpan = TimeSpan_FromMilliseconds->Invoke<void*>(1.0);
//        Log::DebugFmt("staticTimeSpan is created at %p", staticTimeSpan);
//        auto staticTimeSpanTicks = Il2cppUtils::ClassGetFieldValue<long>(staticTimeSpan, _ticks_field);
//        Log::DebugFmt("staticTimeSpan's ticks is %lld", staticTimeSpanTicks);
//        if (IsNativeObjectAlive(timespan)) {
//            Il2cppUtils::ClassSetFieldValue<long>(timespan, _ticks_field, 25 * 60LL * 1000 * 10000);
//        }
        return FesConnectArchivePlayer_JumpTimeAsync_Orig(self, timespan, method);
    }

    DEFINE_HOOK(Il2cppUtils::Il2CppObject*, M3U8Live_get_TotalSegmentsDuration, (void* self, void* method)) {
//        Log::DebugFmt("M3U8Live_get_TotalSegmentsDuration HOOKED");
        auto result = M3U8Live_get_TotalSegmentsDuration_Orig(self, method);
//        if (result) {
//            Log::DebugFmt("M3U8Live_get_TotalSegmentsDuration result: %p", result);
//            static auto TimeSpan_klass = Il2cppUtils::GetClass("mscorlib.dll", "System", "TimeSpan");
//            Log::DebugFmt("M3U8Live_get_TotalSegmentsDuration TimeSpan_klass: %p", TimeSpan_klass);
//            static auto _ticks_field = TimeSpan_klass->Get<UnityResolve::Field>("_ticks");
//            Log::DebugFmt("M3U8Live_get_TotalSegmentsDuration _ticks_field: %p", _ticks_field);
//
//        }
        return result;
    }


    void Install(HookInstaller* hookInstaller) {
        ADD_HOOK(SchoolResolution_GetResolution, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain",
                                                                      "SchoolResolution", "GetResolution"));
        
        // Fes live camera unlock
        ADD_HOOK(ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveSetFesCameraWithHttpInfoAsync"));

        // FesConnectArchivePlayer
//        ADD_HOOK(FesConnectArchivePlayer_get_CurrentTime, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer", "get_CurrentTime"));
//        ADD_HOOK(FesConnectArchivePlayer_get_RunningTime, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer", "get_RunningTime"));
        auto FesConnectArchivePlayer_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer");
        if (FesConnectArchivePlayer_klass) {
            auto JumpTimeAsync_klass = Il2cppUtils::find_nested_class_from_name(FesConnectArchivePlayer_klass, "<JumpTimeAsync>d__11");
            auto method = Il2cppUtils::GetMethodIl2cpp(JumpTimeAsync_klass, "MoveNext", 0);
            ADD_HOOK(FesConnectArchivePlayer_JumpTimeAsync_MoveNext, method->methodPointer);
        }
        ADD_HOOK(FesConnectArchivePlayer_JumpTimeAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer", "JumpTimeAsync"));
        ADD_HOOK(M3U8Live_get_TotalSegmentsDuration, Il2cppUtils::GetMethodPointer("Alstromeria.dll", "Alst.Archive", "M3U8Live", "get_TotalSegmentsDuration"));
        ADD_HOOK(RealtimeRenderingArchiveController_SetPlayPositionAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "RealtimeRenderingArchiveController", "SetPlayPositionAsync"));

        // GetHttpAsyncAddr
        ADD_HOOK(ApiClient_Deserialize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client","ApiClient", "Deserialize"));
        auto ArchiveApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi");
        if (ArchiveApi_klass) {
            // hook /v1/archive/get_fes_archive_data response
            auto ArchiveGetFesArchiveDataWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetFesArchiveDataWithHttpInfoAsync>d__30");
            auto method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetFesArchiveDataWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }

        ADD_HOOK(Unity_set_targetFrameRate, Il2cppUtils::il2cpp_resolve_icall(
                 "UnityEngine.Application::set_targetFrameRate(System.Int32)"));
    }
}