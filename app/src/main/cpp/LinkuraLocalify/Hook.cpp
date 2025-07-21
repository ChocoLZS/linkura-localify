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
#include <chrono>
#include "linkura_messages.pb.h"
#include <future>


std::unordered_set<void*> hookedStubs{};
extern std::filesystem::path linkuraLocalPath;

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

#pragma region LiveRender
    enum struct SchoolResolution_LiveAreaQuality {
        Low,
        Middle,
        High
    };
    enum struct LiveScreenOrientation {
        Landscape,
        Portrait
    };

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

#pragma endregion

#pragma region FreeCamera
    bool IsNativeObjectAlive(void* obj) {
        if (!obj) return false;
        static UnityResolve::Method* IsNativeObjectAliveMtd = nullptr;
        if (!IsNativeObjectAliveMtd) IsNativeObjectAliveMtd = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                                     "Object", "IsNativeObjectAlive");
        return IsNativeObjectAliveMtd->Invoke<bool>(obj);
    }
    UnityResolve::UnityType::Camera* mainFreeCameraCache = nullptr;
    UnityResolve::UnityType::Transform* freeCameraTransformCache = nullptr;
    UnityResolve::UnityType::Transform* cacheTrans = nullptr;
    UnityResolve::UnityType::Quaternion cacheRotation{};
    UnityResolve::UnityType::Vector3 cachePosition{};
    UnityResolve::UnityType::Vector3 cacheForward{};
    UnityResolve::UnityType::Vector3 cacheLookAt{};

    void registerMainFreeCamera(UnityResolve::UnityType::Camera* mainCamera, L4Camera::CameraSceneType cameraSceneType) {
        if (!Config::enableFreeCamera) return;
        mainFreeCameraCache = mainCamera;
        if (mainFreeCameraCache) freeCameraTransformCache = mainFreeCameraCache->GetTransform();
        L4Camera::SetCameraSceneType(cameraSceneType);
    }

    void unregisterMainFreeCamera(bool cleanup = false) {
        if (!Config::enableFreeCamera) return;
        L4Camera::SetCameraSceneType(L4Camera::CameraSceneType::NONE);
        if (cleanup) {
            mainFreeCameraCache = nullptr;
            freeCameraTransformCache = nullptr;
        }
        Log::DebugFmt("Unregister main camera");
    }

    UnityResolve::UnityType::Camera* currentCameraCache = nullptr;
    UnityResolve::UnityType::Transform* currentCameraTransformCache = nullptr;
    bool currentCameraRegistered = false;
    
    void registerCurrentCamera(UnityResolve::UnityType::Camera* currentCamera) {
        currentCameraCache = currentCamera;
        if (currentCameraCache) currentCameraTransformCache = currentCameraCache->GetTransform();
        currentCameraRegistered = true;
    }
    void unregisterCurrentCamera() {
        currentCameraRegistered = false;
    }

    std::vector<uint8_t> getCameraInfoProtobuf() {
        linkura::ipc::CameraData cameraData;
        
        if (currentCameraRegistered && currentCameraCache && IsNativeObjectAlive(currentCameraCache)) {
            try {
                auto fov = currentCameraCache->GetFoV();
                auto transform = currentCameraCache->GetTransform();
                auto position = currentCameraTransformCache->GetPosition();
                auto rotation = currentCameraTransformCache->GetRotation();
                
                cameraData.set_is_valid(true);
                
                auto* pos = cameraData.mutable_position();
                pos->set_x(position.x);
                pos->set_y(position.y);
                pos->set_z(position.z);
                
                auto* rot = cameraData.mutable_rotation();
                rot->set_x(rotation.x);
                rot->set_y(rotation.y);
                rot->set_z(rotation.z);
                rot->set_w(rotation.w);
                
                cameraData.set_fov(fov);
                cameraData.set_mode(static_cast<int32_t>(L4Camera::GetCameraMode()));
                cameraData.set_scene_type(static_cast<int32_t>(L4Camera::GetCameraSceneType()));
            } catch (const std::exception& e) {
                Log::ErrorFmt("getCameraInfo exception: %s", e.what());
                cameraData.set_is_valid(false);
            }
        } else {
            cameraData.set_is_valid(false);
        }
        
        std::string serialized_data;
        cameraData.SerializeToString(&serialized_data);
        return {serialized_data.begin(), serialized_data.end()};
    }

    DEFINE_HOOK(void, Unity_set_rotation_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Quaternion* value)) {
        if (Config::enableFreeCamera && (L4Camera::GetCameraSceneType() != L4Camera::CameraSceneType::NONE)) {
            if (IsNativeObjectAlive(freeCameraTransformCache)) {
                static auto lookat_injected = reinterpret_cast<void (*)(void*freeCameraTransformCache,
                                                                        UnityResolve::UnityType::Vector3* worldPosition, UnityResolve::UnityType::Vector3* worldUp)>(
                        Il2cppUtils::il2cpp_resolve_icall(
                                "UnityEngine.Transform::Internal_LookAt_Injected(UnityEngine.Vector3&,UnityEngine.Vector3&)"));
                static auto worldUp = UnityResolve::UnityType::Vector3(0, 1, 0);
                const auto cameraMode = L4Camera::GetCameraMode();
                if (cameraMode == L4Camera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        if (L4Camera::GetFirstPersonRoll() == L4Camera::FirstPersonRoll::ENABLE_ROLL) {
                            *value = cacheRotation;
                        }
                        else {
                            static LinkuraLocal::Misc::FixedSizeQueue<float> recordsY(60);
                            const auto newY = L4Camera::CheckNewY(cacheLookAt, true, recordsY);
                            UnityResolve::UnityType::Vector3 newCacheLookAt{cacheLookAt.x, newY, cacheLookAt.z};
                            lookat_injected(self, &newCacheLookAt, &worldUp);
                        }
                    }
                }
                else if (cameraMode == L4Camera::CameraMode::FOLLOW) {
                    auto newLookAtPos = L4Camera::CalcFollowModeLookAt(cachePosition,
                                                                       L4Camera::followPosOffset, true);
                    lookat_injected(self, &newLookAtPos, &worldUp);
                }
                else {
                    auto& origCameraLookat = L4Camera::baseCamera.lookAt;
                    if (freeCameraTransformCache) lookat_injected(freeCameraTransformCache, &origCameraLookat, &worldUp);
                }
            }
            if (self == freeCameraTransformCache) return;
        }
        return Unity_set_rotation_Injected_Orig(self, value);
    }

    DEFINE_HOOK(void, Unity_set_position_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Vector3* data)) {
        if (Config::enableFreeCamera && (L4Camera::GetCameraSceneType() != L4Camera::CameraSceneType::NONE)) {
            if (IsNativeObjectAlive(freeCameraTransformCache)) {
                const auto cameraMode = L4Camera::GetCameraMode();
                if (cameraMode == L4Camera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        auto pos = L4Camera::CalcFirstPersonPosition(cachePosition, cacheForward, L4Camera::firstPersonPosOffset);
                        Unity_set_position_Injected_Orig(freeCameraTransformCache, &pos);
                    }

                }
                else if (cameraMode == L4Camera::CameraMode::FOLLOW) {
                    auto newLookAtPos = L4Camera::CalcFollowModeLookAt(cachePosition, L4Camera::followPosOffset);
                    auto pos = L4Camera::CalcPositionFromLookAt(newLookAtPos, L4Camera::followPosOffset);
                    Unity_set_position_Injected_Orig(freeCameraTransformCache, &pos);
                }
                else {
                    auto& origCameraPos = L4Camera::baseCamera.pos;
                    UnityResolve::UnityType::Vector3 pos{origCameraPos.x, origCameraPos.y, origCameraPos.z};
                    Unity_set_position_Injected_Orig(freeCameraTransformCache, &pos);
                }
            }

            if (self == freeCameraTransformCache) return;
        }
        Unity_set_position_Injected_Orig(self, data);
    }

    DEFINE_HOOK(void, Unity_set_fieldOfView, (UnityResolve::UnityType::Camera* self, float value)) {
        if (Config::enableFreeCamera && (L4Camera::GetCameraSceneType() != L4Camera::CameraSceneType::NONE)) {
            if (IsNativeObjectAlive(mainFreeCameraCache) && self == mainFreeCameraCache) {
                value = L4Camera::baseCamera.fov;
            }
        }
        Unity_set_fieldOfView_Orig(self, value);
    }
    DEFINE_HOOK(float, Unity_get_fieldOfView, (UnityResolve::UnityType::Camera* self)) {
        if (Config::enableFreeCamera && (L4Camera::GetCameraSceneType() != L4Camera::CameraSceneType::NONE)) {
            if (IsNativeObjectAlive(mainFreeCameraCache)) {
                for (const auto& i : UnityResolve::UnityType::Camera::GetAllCamera()) {
                    Unity_set_fieldOfView_Orig(i, L4Camera::baseCamera.fov);
                }
            }
            if (self == mainFreeCameraCache) {
                Unity_set_fieldOfView_Orig(mainFreeCameraCache, L4Camera::baseCamera.fov);
                return L4Camera::baseCamera.fov;
            }
        }
        return Unity_get_fieldOfView_Orig(self);
    }
    DEFINE_HOOK(void, EndCameraRendering, (void* ctx, void* camera, void* method)) {
        if (Config::enableFreeCamera && (L4Camera::GetCameraSceneType() != L4Camera::CameraSceneType::NONE)) {
            if (IsNativeObjectAlive(mainFreeCameraCache)) {
                // prevent crash for with live and fes live & remain the free fov for story
                if (L4Camera::GetCameraSceneType() == L4Camera::CameraSceneType::STORY) Unity_set_fieldOfView_Orig(mainFreeCameraCache, L4Camera::baseCamera.fov);
                if (L4Camera::GetCameraMode() == L4Camera::CameraMode::FIRST_PERSON) {
                    mainFreeCameraCache->SetNearClipPlane(0.001f);
                }
            }
        }
        EndCameraRendering_Orig(ctx, camera, method);
    }
    enum CameraType {
        Invalid,
        FixedCamera,
        TargetCamera,
        VirtualCamera,
        CameraMan,
        LoopCamera,
        AudienceCamera,
        AnimationCamera,
        SmartPhoneCamera,
        RedSpyCamera,
        TimelineCamera,
        LookAtCamera
    };
    // fes live / with meets, but we treat it as with live
    // when every dynamic camera change, this will be called
    DEFINE_HOOK(UnityResolve::UnityType::Camera*, CameraManager_GetCamera, (Il2cppUtils::Il2CppObject* self, CameraType cameraType , int cameraId , void* method)) {
        Log::DebugFmt("CameraManager_GetCamera HOOKED");
        auto camera =  CameraManager_GetCamera_Orig(self, cameraType, cameraId, method);
        if (L4Camera::GetCameraSceneType() != L4Camera::CameraSceneType::FES_LIVE) {
            registerMainFreeCamera(camera, L4Camera::CameraSceneType::WITH_LIVE);
            registerCurrentCamera(camera);
        }
        
        return camera;
    }

    // chapter switch
    // this will be called when chapter page closed in with meets
    uintptr_t LiveConnectChapterListPresenter_CreateAvailableChapterNodeView_MoveNext_Addr = 0;
    DEFINE_HOOK(void, LiveConnectChapterModel_NewChapterConfirmed, (Il2cppUtils::Il2CppObject* self, void* method)) {
        auto caller = __builtin_return_address(0);
        IF_CALLER_WITHIN(LiveConnectChapterListPresenter_CreateAvailableChapterNodeView_MoveNext_Addr, caller, 2000) {
            // fes live will use the same fixed camera at all time
            if (L4Camera::GetCameraSceneType() == L4Camera::CameraSceneType::WITH_LIVE) {
                unregisterMainFreeCamera();
                unregisterCurrentCamera();
            }
        }
        LiveConnectChapterModel_NewChapterConfirmed_Orig(self, method);
    }
    // exit
    DEFINE_HOOK(void*, LiveSceneController_FinalizeSceneAsync, (Il2cppUtils::Il2CppObject* self, void* token, void* method)) {
        Log::DebugFmt("LiveSceneController_FinalizeSceneAsync HOOKED");
        unregisterMainFreeCamera();
        unregisterCurrentCamera();
        L4Camera::reset_camera();
        return LiveSceneController_FinalizeSceneAsync_Orig(self, token, method);
    }

    DEFINE_HOOK(void, StoryModelSpaceManager_Init, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("StoryModelSpaceManager_Init HOOKED");

        static auto get_ModelSpace = UnityResolve::Get("Assembly-CSharp.dll")->Get(
                "StoryModelSpaceManager")->Get<UnityResolve::Method>("get_modelSpace");
        static auto get_StoryCamera = UnityResolve::Get("Assembly-CSharp.dll")->Get("StoryModelSpace")->Get<UnityResolve::Method>("get_StoryCamera");
        StoryModelSpaceManager_Init_Orig(self, method);
        auto modelSpace = get_ModelSpace->Invoke<Il2cppUtils::Il2CppObject*>(self);
        auto storyCamera = get_StoryCamera->Invoke<UnityResolve::UnityType::Camera*>(modelSpace);
        registerMainFreeCamera(storyCamera, L4Camera::CameraSceneType::STORY);
        registerCurrentCamera(storyCamera);
    }
    DEFINE_HOOK(void, StoryScene_OnFinalize, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("StoryScene_OnFinalize HOOKED");
        unregisterMainFreeCamera(true);
        unregisterCurrentCamera();
        L4Camera::reset_camera();
        StoryScene_OnFinalize_Orig(self, method);
    }

#pragma endregion

#pragma region Camera
    // fes live only
    DEFINE_HOOK(UnityResolve::UnityType::Camera*, FesLiveFixedCamera_GetCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("FesLiveFixedCamera_GetCamera HOOKED");
        auto camera = FesLiveFixedCamera_GetCamera_Orig(self, method);
        registerMainFreeCamera(camera, L4Camera::CameraSceneType::FES_LIVE);
        registerCurrentCamera(camera);
        return camera;
    }

    DEFINE_HOOK(UnityResolve::UnityType::Camera*, DynamicCamera_GetCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("DynamicCamera_GetCamera HOOKED");
        auto camera = DynamicCamera_GetCamera_Orig(self, method);
        registerCurrentCamera(camera);
        return camera;
    }

    DEFINE_HOOK(UnityResolve::UnityType::Camera*, IdolTargetingCamera_GetCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("IdolTargetingCamera_GetCamera HOOKED");
        auto camera = IdolTargetingCamera_GetCamera_Orig(self, method);
        registerCurrentCamera(camera);
        return camera;
    }

#pragma endregion

    void StartInjectFunctions() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();
        // problem here
        auto hmodule = xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_LAZY);
        UnityResolve::Init(hmodule, UnityResolve::Mode::Il2Cpp, Config::lazyInit);
//        UnityResolve::Init(xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_LAZY),
//            UnityResolve::Mode::Il2Cpp, Config::lazyInit);

        ADD_HOOK(SchoolResolution_GetResolution, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain",
                                                                      "SchoolResolution", "GetResolution"));
        // 👀
        ADD_HOOK(CoverImageCommandReceiver_Awake, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CoverImageCommandReceiver", "Awake"));
        // Fes live camera unlock
        ADD_HOOK(ArchiveApi_ArchiveSetFesCameraWithHttpInfoAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi", "ArchiveSetFesCameraWithHttpInfoAsync"));

#pragma region GetHttpAsyncAddr
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
#pragma endregion

#pragma region Camera
//        ADD_HOOK(FesLiveCameraSwitcher_SwitchCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesLiveCameraSwitcher", "SwitchCamera"));

        ADD_HOOK(DynamicCamera_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "DynamicCamera", "GetCamera"));
        ADD_HOOK(IdolTargetingCamera_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "IdolTargetingCamera", "GetCamera"));
//        ADD_HOOK(IdolTargetingCamera_SetTargetIdol, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "IdolTargetingCamera", "SetTargetIdol"));
//        ADD_HOOK(WithLiveCameraReference_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "WithLiveCameraReference", ".ctor"));
//
//        ADD_HOOK(DynamicCamera_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "DynamicCamera", ".ctor"));
//        ADD_HOOK(DynamicCamera_FindCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "DynamicCamera", "FindCamera"));
//        ADD_HOOK(FixedCamera_Initialize, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "FixedCamera", "Initialize"));
//        ADD_HOOK(CameraManager_CreatePresetCameras, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "CreatePresetCameras"));


//  useless ADD_HOOK(SwipeCamera_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.LiveMain", "SwipeCamera", ".ctor"));

#pragma endregion

#pragma region FreeCamera_ADD_HOOK
        ADD_HOOK(FesLiveFixedCamera_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesLiveFixedCamera", "GetCamera"));
        ADD_HOOK(CameraManager_GetCamera, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "GetCamera"));
       auto LiveConnectChapterListPresenter_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "School.LiveMain", "LiveConnectChapterListPresenter");
       auto display_klass = Il2cppUtils::find_nested_class_from_name(LiveConnectChapterListPresenter_klass, "<>c__DisplayClass10_0");
       auto createAvailableChapterNodeView_klass = Il2cppUtils::find_nested_class_from_name(display_klass, "<<CreateAvailableChapterNodeView>b__2>d");
       auto method = Il2cppUtils::GetMethodIl2cpp(createAvailableChapterNodeView_klass, "MoveNext", 0);
       if (method) {
           LiveConnectChapterListPresenter_CreateAvailableChapterNodeView_MoveNext_Addr = method->methodPointer;
       }
//       auto LiveSystemSceneController_PrepareChangeSceneAsync_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "School.LiveMain", "LiveSystemSceneController");
//       auto prepareChangeSceneAsync_klass = Il2cppUtils::find_nested_class_from_name(LiveSystemSceneController_PrepareChangeSceneAsync_klass, "<PrepareChangeSceneAsync>d__1");
//       method = Il2cppUtils::GetMethodIl2cpp(prepareChangeSceneAsync_klass, "MoveNext", 0);
//       if (method) {
//           // not used for chapter
//           ADD_HOOK(LiveSystemSceneController_PrepareChangeSceneAsync_MoveNext, method->methodPointer);
//       }
//       // not used for chapter
//        LiveSceneController_PrepareChangeSceneAsync_Addr = reinterpret_cast<uintptr_t>(Il2cppUtils::GetMethodPointer(
//                "Core.dll", "Inspix", "LiveSceneController", "PrepareChangeSceneAsync"));
//       ADD_HOOK(LiveSceneControllerLogic_FindAssetPaths, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneControllerLogic", "FindAssetPaths"));
//        ADD_HOOK(LiveContentsLoader_LoadLiveContentsAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "LiveContentsLoader", "LoadLiveContentsAsync"));
//        ADD_HOOK(CameraManager_RemoveCamera, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "RemoveCamera"));
        ADD_HOOK(LiveConnectChapterModel_NewChapterConfirmed, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "LiveConnectChapterModel", "NewChapterConfirmed"));
        ADD_HOOK(LiveSceneController_FinalizeSceneAsync, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneController", "FinalizeSceneAsync"));
        ADD_HOOK(StoryModelSpaceManager_Init, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryModelSpaceManager", "Init"));
        ADD_HOOK(StoryScene_OnFinalize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "OnFinalize"));
//        ADD_HOOK(VLDOF_IsActive,
//                 Il2cppUtils::GetMethodPointer("Unity.RenderPipelines.Universal.Runtime.dll", "VL.Rendering",
//                                               "VLDOF", "IsActive"));
        ADD_HOOK(Unity_set_position_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)"));
        ADD_HOOK(Unity_set_rotation_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)"));
        ADD_HOOK(Unity_get_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "get_fieldOfView"));
        ADD_HOOK(Unity_set_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "set_fieldOfView"));
        ADD_HOOK(EndCameraRendering, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine.Rendering",
                                                                   "RenderPipeline", "EndCameraRendering"));
#pragma endregion

#pragma region Story
//        ADD_HOOK(StoryModelSpace_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryModelSpace", "get_StoryCamera"));
//        ADD_HOOK(StoryScene_LoadStoryData, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "LoadStoryData"));
//        ADD_HOOK(StoryScriptConverter_AdvScriptFileToMnemonics, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScriptConverter", "AdvScriptFileToMnemonics"));
//        ADD_HOOK(StorySystem_OnInitialize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StorySystem", "OnInitialize"));
//        ADD_HOOK(StoryScene_SetCameraColor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "SetCameraColor"));
//        ADD_HOOK(StoryModelSpaceManager_get_modelSpace, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryModelSpaceManager", "get_modelSpace"));
#pragma endregion

        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));

        ADD_HOOK(Unity_set_targetFrameRate, Il2cppUtils::il2cpp_resolve_icall(
                 "UnityEngine.Application::set_targetFrameRate(System.Int32)"));


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
        L4Camera::initCameraSettings(); // free camera

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
#ifndef GKMS_WINDOWS
        ADD_HOOK(HookMain::il2cpp_init,
                 Plugin::GetInstance().GetHookInstaller()->LookupSymbol("il2cpp_init"));
#else
        HookMain::il2cpp_init_Hook(nullptr);
#endif


        Log::Info("Hook installed");
    }

    std::vector<uint8_t> getCameraInfoProtobuf() {
        return HookMain::getCameraInfoProtobuf();
    }
}
