#include "../HookMain.h"
#include "../../deps/nlohmann/json.hpp"
#include "../../build/linkura_messages.pb.h"
#include <thread>

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

    enum SetPlayPosition_State {
        Nothing,
        UpdateReceived
    };

    SetPlayPosition_State setPlayPositionState = SetPlayPosition_State::Nothing;

    DEFINE_HOOK(void* , RealtimeRenderingArchiveController_SetPlayPositionAsync, (void* self, float seconds)) {
        Log::DebugFmt("RealtimeRenderingArchiveController_SetPlayPositionAsync HOOKED: seconds is %f", seconds);
        HookShare::Shareable::realtimeRenderingArchiveControllerCache = self;
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
        }
        if (setPlayPositionState == SetPlayPosition_State::UpdateReceived && HookShare::Shareable::realtimeRenderingArchiveControllerCache) {
            if (HookShare::Shareable::renderSceneIsWithLive()) {
                HookShare::Shareable::resetRenderScene();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                HookCamera::unregisterMainFreeCamera(true);
                HookCamera::unregisterCurrentCamera();
            }
            RealtimeRenderingArchiveController_SetPlayPositionAsync_Orig(
                    HookShare::Shareable::realtimeRenderingArchiveControllerCache,
                    HookShare::Shareable::realtimeRenderingArchivePositionSeconds
            );
            setPlayPositionState = SetPlayPosition_State::Nothing;
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

    /**
     * @brief set target frame rate for unity engine
     */
    DEFINE_HOOK(void, Unity_set_targetFrameRate, (int value)) {
        const auto configFps = Config::targetFrameRate;
        return Unity_set_targetFrameRate_Orig(configFps == 0 ? value: configFps);
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

//        static auto uTimeSpan_klass = Il2cppUtils::GetClass("mscorlib.dll", "System", "TimeSpan");
//        static auto uTimeSpan_ctor = Il2cppUtils::GetMethod("mscorlib.dll", "System", "TimeSpan", ".ctor", {"System.Int32","System.Int32","System.Int32","System.Int32","System.Int32"});
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


    std::vector<uint8_t> getCurrentArchiveInfo() {
        try {
            linkura::ipc::ArchiveInfo archiveInfo;
            // Get duration from cached archive data
            auto archiveDataIt = HookShare::Shareable::archiveData.find(HookShare::Shareable::currentArchiveId);
            if (archiveDataIt != HookShare::Shareable::archiveData.end()) {
                Log::DebugFmt("getCurrentArchiveInfo: duration=%lld", archiveDataIt->second.duration);
                archiveInfo.set_duration(archiveDataIt->second.duration);
            } else {
                archiveInfo.set_duration(0);
            }

            // Serialize to protobuf
            std::vector<uint8_t> result(archiveInfo.ByteSize());
            archiveInfo.SerializeToArray(result.data(), result.size());

            Log::DebugFmt("getCurrentArchiveInfo: duration=%lld",
                         archiveInfo.duration());

            return result;
        } catch (const std::exception& e) {
            Log::ErrorFmt("Error in getCurrentArchiveInfo: %s", e.what());
            return std::vector<uint8_t>();
        }
    }

    void setArchivePosition(float seconds) {
        try {
            if (HookShare::Shareable::realtimeRenderingArchiveControllerCache == nullptr) {
                Log::Error("setArchivePosition: No cached archive controller available");
                return;
            }

            Log::DebugFmt("setArchivePosition: Setting position to %f seconds", seconds);
            setPlayPositionState = SetPlayPosition_State::UpdateReceived;
            HookShare::Shareable::realtimeRenderingArchivePositionSeconds = seconds;
        } catch (const std::exception& e) {
            Log::ErrorFmt("Error in setArchivePosition: %s", e.what());
        }
    }

    void Install(HookInstaller* hookInstaller) {
        ADD_HOOK(SchoolResolution_GetResolution, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain",
                                                                      "SchoolResolution", "GetResolution"));
        
        // Fes live camera unlock
        ADD_HOOK(ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveSetFesCameraWithHttpInfoAsync"));
        ADD_HOOK(RealtimeRenderingArchiveController_SetPlayPositionAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "RealtimeRenderingArchiveController", "SetPlayPositionAsync"));
        ADD_HOOK(Unity_set_targetFrameRate, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Application::set_targetFrameRate(System.Int32)"));

        // FesConnectArchivePlayer
//        ADD_HOOK(FesConnectArchivePlayer_get_CurrentTime, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer", "get_CurrentTime"));
//        ADD_HOOK(FesConnectArchivePlayer_get_RunningTime, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer", "get_RunningTime"));
//        auto FesConnectArchivePlayer_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer");
//        if (FesConnectArchivePlayer_klass) {
//            auto JumpTimeAsync_klass = Il2cppUtils::find_nested_class_from_name(FesConnectArchivePlayer_klass, "<JumpTimeAsync>d__11");
//            auto method = Il2cppUtils::GetMethodIl2cpp(JumpTimeAsync_klass, "MoveNext", 0);
//            ADD_HOOK(FesConnectArchivePlayer_JumpTimeAsync_MoveNext, method->methodPointer);
//        }
//        ADD_HOOK(FesConnectArchivePlayer_JumpTimeAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesConnectArchivePlayer", "JumpTimeAsync"));
//        ADD_HOOK(M3U8Live_get_TotalSegmentsDuration, Il2cppUtils::GetMethodPointer("Alstromeria.dll", "Alst.Archive", "M3U8Live", "get_TotalSegmentsDuration"));

    }

}