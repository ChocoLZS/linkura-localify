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
//            Log::DebugFmt("切换分辨率至: %d x %d, 返回结果: %p", width, height, result);
        }
        // debug field
        {
           auto mainCamera =  UnityResolve::UnityType::Camera::GetMain();
           Log::DebugFmt("MainCamera: %p", mainCamera);
           auto cameras = UnityResolve::UnityType::Camera::GetAllCamera();
           for (auto camera : cameras) {
               Log::DebugFmt("Camera: %p", camera);
           }


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
                json["has_extra_admission"] = "true";
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }
        
        return result;
    }

    // render fps
    DEFINE_HOOK(void, Unity_set_targetFrameRate, (int value)) {
        const auto configFps = Config::targetFrameRate;
        return Unity_set_targetFrameRate_Orig(configFps == 0 ? value: configFps);
    }

#pragma region Camera

    void PrintMemoryBytes(const void* ptr, size_t numBytes, const char* description = nullptr) {
        if (!ptr) {
            Log::DebugFmt("PrintMemoryBytes: ptr is null");
            return;
        }

        const unsigned char* bytes = static_cast<const unsigned char*>(ptr);
        std::string hexStr;
        std::string charStr;

        // 预分配字符串空间以提高性能
        hexStr.reserve(numBytes * 3);
        charStr.reserve(numBytes);

        for (size_t i = 0; i < numBytes; ++i) {
            unsigned char byte = bytes[i];

            // 添加十六进制表示
            char hexBuffer[4];
            snprintf(hexBuffer, sizeof(hexBuffer), "%02X ", byte);
            hexStr += hexBuffer;

            // 添加字符表示（忽略\0和不可打印字符）
            if (byte != 0 && byte >= 32 && byte <= 126) {
                charStr += static_cast<char>(byte);
            } else if (byte != 0) {
                charStr += '.';
            }
            // 如果是\0，则完全忽略
        }

        if (description) {
            Log::DebugFmt("%s - Memory dump (%zu bytes):", description, numBytes);
        } else {
            Log::DebugFmt("Memory dump (%zu bytes):", numBytes);
        }
        Log::DebugFmt("Hex: %s", hexStr.c_str());
        Log::DebugFmt("Str: %s", charStr.c_str());
    }
    enum struct LiveCameraType {
        LiveCameraTypeUndefined,
        LiveCameraTypeDynamicView,
        LiveCameraTypeArenaView,
        LiveCameraTypeStandView,
        LiveCameraTypeSchoolIdle
    };
    DEFINE_HOOK(void, FesLiveCameraSwitcher_SwitchCamera, (Il2cppUtils::Il2CppObject* self, LiveCameraType enableCamera, void* method_info )) {
        static auto FesLiveCameraSwitcher_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "School.LiveMain", "FesLiveCameraSwitcher");
        static auto currentCamera_filed = FesLiveCameraSwitcher_klass->Get<UnityResolve::Field>("currentCamera");
        static auto fesLiveCameras_filed = FesLiveCameraSwitcher_klass->Get<UnityResolve::Field>("fesLiveCameras");
        auto cur_camera = Il2cppUtils::ClassGetFieldValue<Il2cppUtils::Il2CppObject* >(self, currentCamera_filed);
        Log::DebugFmt("Current enable Camera is: %d", enableCamera);
        // once you select the camera, this will be here
        if (cur_camera) {
            auto camera_klass = Il2cppUtils::get_class_from_instance(cur_camera);
            Log::DebugFmt("camera_klass name is: %s", camera_klass->name);
            if (std::string(camera_klass->name) == "DynamicCamera") {
//                static auto GetCamera_DynamicCamera = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "School.LiveMain", "DynamicCamera", "GetCamera");
//                auto camera = GetCamera_DynamicCamera->Invoke<UnityResolve::UnityType::Camera*>(cur_camera);
//                auto camera_class = Il2cppUtils::get_class_from_instance(camera);
            }
            if (std::string(camera_klass->name) == "FesLiveFixedCamera") {
//                static auto GetCamera_FesLiveFixedCamera = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "School.LiveMain", "FesLiveFixedCamera", "GetCamera");
//                auto camera = GetCamera_FesLiveFixedCamera->Invoke<UnityResolve::UnityType::Camera*>(cur_camera);
//                auto camera_class = Il2cppUtils::get_class_from_instance(camera);
            }
            if (std::string(camera_klass->name) == "IdolTargetingCamera") {
//                static auto GetCamera_IdolTargetingCamera = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "School.LiveMain", "IdolTargetingCamera", "GetCamera");
//                auto camera = GetCamera_IdolTargetingCamera->Invoke<UnityResolve::UnityType::Camera*>(cur_camera);
//                auto camera_class = Il2cppUtils::get_class_from_instance(camera);
            }
//            PrintMemoryBytes(fesLiveCameras, 200);
//            Log::DebugFmt("fesLiveCameras length is: %d", fesLiveCameras.bounds->length);

//            for (auto i = 0; i < cameras_array->max_length; i++) {
//                auto camera_instance = reinterpret_cast<Il2cppUtils::Il2CppObject*>(cameras_array->At(i));
//                auto camera_class = Il2cppUtils::get_class_from_instance(camera_instance);
//                Log::DebugFmt("camera_class name is: %s at index %d", camera_class->name, i);
//            }
            
        }
//        static auto cameras_arr = FesLiveCameraSwitcher_klass->Get<UnityResolve::Array>("cameras");
//        Log::DebugFmt("fesLiveCameras length is: %d", cameras_arr->max_length);
//        for (int i = 0; i < cameras_arr->max_length; i++) {
//            auto camera_klass = Il2cppUtils::get_class_from_instance(cameras_arr->At(i));
//            Log::DebugFmt("camera_klass name is: %s", camera_klass->name);
//        }
        FesLiveCameraSwitcher_SwitchCamera_Orig(self, enableCamera, method_info);
    }

    DEFINE_HOOK(UnityResolve::UnityType::Camera*, FesLiveFixedCamera_GetCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("FesLiveFixedCamera_GetCamera HOOKED");
        return  FesLiveFixedCamera_GetCamera_Orig(self, method);
    }
    DEFINE_HOOK(UnityResolve::UnityType::Camera*, DynamicCamera_GetCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("DynamicCamera_GetCamera HOOKED");
        return  DynamicCamera_GetCamera_Orig(self, method);
    }
    DEFINE_HOOK(UnityResolve::UnityType::Camera*, DynamicCamera_GetSubCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("DynamicCamera_GetSubCamera HOOKED");
        return DynamicCamera_GetSubCamera_Orig(self, method);
    }
    DEFINE_HOOK(UnityResolve::UnityType::Camera*, IdolTargetingCamera_GetCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("IdolTargetingCamera_GetCamera HOOKED");
        return  IdolTargetingCamera_GetCamera_Orig(self, method);
    }
    DEFINE_HOOK(void, IdolTargetingCamera_SetTargetIdol, (Il2cppUtils::Il2CppObject* self,int32_t characterId, void* method)) {
        Log::DebugFmt("IdolTargetingCamera_SetTargetIdol HOOKED, current characterId is: %d", characterId);
        return  IdolTargetingCamera_SetTargetIdol_Orig(self, characterId, method);
    }

    DEFINE_HOOK(void, WithLiveCameraReference_ctor, (Il2cppUtils::Il2CppObject* self, Il2cppUtils::Il2CppObject* sceneModel, int32_t renderType, int32_t sceneType,
            Il2cppUtils::Il2CppObject* liveRenderResolution, void* method)) {
        Log::DebugFmt("WithLiveCameraReference_ctor HOOKED");
        return WithLiveCameraReference_ctor_Orig(self, sceneModel, renderType, sceneType, liveRenderResolution, method);
    }

    DEFINE_HOOK(void, DynamicCamera_ctor, (Il2cppUtils::Il2CppObject* self, UnityResolve::UnityType::Camera* mainCamera,UnityResolve::UnityType::Vector2 targetResolution, void* method)) {
        Log::DebugFmt("DynamicCamera_ctor HOOKED");
        Log::DebugFmt("targetResolution: %d, %d", targetResolution.x, targetResolution.y);
        Log::DebugFmt("mainCamera: %f", mainCamera->GetFoV());
        // not working
//        mainCamera->SetFoV(120);
        auto transform = mainCamera->GetTransform();
        Log::DebugFmt("transform: x: %f, y: %f, z: %f", transform->GetPosition().x, transform->GetPosition().y, transform->GetPosition().z);
        // not working
//        transform->SetPosition(UnityResolve::UnityType::Vector3(50, 50, 50));
        return DynamicCamera_ctor_Orig(self, mainCamera, targetResolution, method);
    }

    DEFINE_HOOK(Il2cppUtils::Il2CppObject*, DynamicCamera_FindCamera, (int32_t cameraType, int32_t cameraId)) {
        Log::DebugFmt("DynamicCamera_FindCamera HOOKED");
        auto mainCamera = DynamicCamera_FindCamera_Orig(cameraType, cameraId);
        auto mainCamera_klass = Il2cppUtils::get_class_from_instance(mainCamera);
        Log::DebugFmt("mainCamera_klass name is: %s", mainCamera_klass->name);

        return mainCamera;
    }

    // 动捕渲染时都会被调用
    DEFINE_HOOK(void, FixedCamera_Initialize, (Il2cppUtils::Il2CppObject* self, Il2cppUtils::Il2CppObject* cameraPoint, int32_t index)) {
        Log::DebugFmt("FixedCamera_Initialize HOOKED");
        auto cameraPoint_klass = Il2cppUtils::ToUnityResolveClass(Il2cppUtils::get_class_from_instance(cameraPoint));
        auto cameraPoint_displayName = Il2cppUtils::ClassGetFieldValue<Il2cppString*>(cameraPoint, cameraPoint_klass->Get<UnityResolve::Field>("displayName"));
        auto cameraPoint_Position = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Vector3>(cameraPoint, cameraPoint_klass->Get<UnityResolve::Field>("position"));
        auto cameraPoint_Rotation = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Vector3>(cameraPoint, cameraPoint_klass->Get<UnityResolve::Field>("rotation"));
        auto cameraPoint_isOrthographic = Il2cppUtils::ClassGetFieldValue<bool*>(cameraPoint, cameraPoint_klass->Get<UnityResolve::Field>("isOrthographic"));
        auto displayName = Misc::ToUTF8(cameraPoint_displayName->chars);
        Log::DebugFmt("camera point display Name %s", displayName.c_str());
//        Log::DebugFmt("cameraPoint_Position: %f, %f, %f", cameraPoint_Position.x, cameraPoint_Position.y, cameraPoint_Position.z);
//        Log::DebugFmt("cameraPoint_Rotation: %f, %f, %f", cameraPoint_Rotation.x, cameraPoint_Rotation.y, cameraPoint_Rotation.z);
//        Log::DebugFmt("cameraPoint_isOrthographic: %d", cameraPoint_isOrthographic);
//        Log::DebugFmt("Index is %d", index);
        // 有用 wm最后一个相机
        if (displayName == "250703") {
            // x 水平于画面的左右位移，正值向左，负值向右
            // y 高度？
            // z 垂直于画面的前后位移，正值向后，负值向前
//            cameraPoint_klass->Get<UnityResolve::Method>("set_Position")->Invoke<void>(cameraPoint, UnityResolve::UnityType::Vector3(0, 3, 0));
            // x 垂直于画面的垂直旋转
            // y 水平旋转
            // z 平行于画面的垂直旋转
//            cameraPoint_klass->Get<UnityResolve::Method>("set_Rotation")->Invoke<void>(cameraPoint, UnityResolve::UnityType::Vector3(20, cameraPoint_Rotation.y, cameraPoint_Rotation.z));
        }
//        Log::DebugFmt("End for class");
//        auto cameraPoint_displayName = Il2cppUtils::ClassGetFieldValue<Il2cppString*>(cameraPoint, cameraPoint_displayName_filed);
//        Log::DebugFmt("cameraPoint_displayName is: %s", cameraPoint_displayName->ToString().c_str());
        FixedCamera_Initialize_Orig(self, cameraPoint, index);

        // 打印camera component
        static auto get_CameraComponent = UnityResolve::Get("Core.dll")->Get("BaseCamera")->Get<UnityResolve::Method>("get_CameraComponent");
        auto cameraComponent = get_CameraComponent->Invoke<UnityResolve::UnityType::Camera*>(self);
        Log::DebugFmt("camera component is at %p", cameraComponent);


    }

    // debug mode preset cameras, fixed camera and timeline camera
    // 直播的camera 不会在这里初始化，直播的camera类型在动捕文件中定义
    // 对于直播来说理论上没有用
    DEFINE_HOOK(void, CameraManager_CreatePresetCameras, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("CameraManager_CreatePresetCameras HOOKED");
        CameraManager_CreatePresetCameras_Orig(self, method);
        auto cameraManager_klass = Il2cppUtils::ToUnityResolveClass(Il2cppUtils::get_class_from_instance(self));
        auto cameraDict = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Dictionary<void*, Il2cppUtils::Il2CppObject *>*>(self, cameraManager_klass->Get<UnityResolve::Field>("cameraDict"));
        Log::DebugFmt("cameraDict: %p, arr max_length is %d, arr bounds %p",
                      cameraDict, cameraDict->pEntries->max_length, cameraDict->pEntries->bounds);
        auto cameraDictArr = cameraDict->pEntries; // 遍历key value
        for (int i = 0; i < cameraDictArr->max_length; i++) {
            auto camera = cameraDictArr->At(i);
            Log::DebugFmt("preset camera is at %p", camera);
        }

    }

    enum CameraType {
        // Token: 0x04001010 RID: 4112
        Invalid,
        // Token: 0x04001011 RID: 4113
        FixedCamera,
        // Token: 0x04001012 RID: 4114
        TargetCamera,
        // Token: 0x04001013 RID: 4115
        VirtualCamera,
        // Token: 0x04001014 RID: 4116
        CameraMan,
        // Token: 0x04001015 RID: 4117
        LoopCamera,
        // Token: 0x04001016 RID: 4118
        AudienceCamera,
        // Token: 0x04001017 RID: 4119
        AnimationCamera,
        // Token: 0x04001018 RID: 4120
        SmartPhoneCamera,
        // Token: 0x04001019 RID: 4121
        RedSpyCamera,
        // Token: 0x0400101A RID: 4122
        TimelineCamera,
        // Token: 0x0400101B RID: 4123
        LookAtCamera
    };

    // 没用
    DEFINE_HOOK(void, SwipeCamera_ctor, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("SwipeCamera_ctor HOOKED");
        return SwipeCamera_ctor_Orig(self, method);
    }
#pragma endregion

#pragma endregion

#pragma region FreeCamera
    bool IsNativeObjectAlive(void* obj) {
        if (!obj) return false;
        static UnityResolve::Method* IsNativeObjectAliveMtd = nullptr;
        if (!IsNativeObjectAliveMtd) IsNativeObjectAliveMtd = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                                     "Object", "IsNativeObjectAlive");
        return IsNativeObjectAliveMtd->Invoke<bool>(obj);
    }
    UnityResolve::UnityType::Camera* mainCameraCache = nullptr;
    UnityResolve::UnityType::Transform* cameraTransformCache = nullptr;
    UnityResolve::UnityType::Transform* cacheTrans = nullptr;
    UnityResolve::UnityType::Quaternion cacheRotation{};
    UnityResolve::UnityType::Vector3 cachePosition{};
    UnityResolve::UnityType::Vector3 cacheForward{};
    UnityResolve::UnityType::Vector3 cacheLookAt{};
    // 设置主相机以及相机移动 transform
    void CheckAndUpdateMainCamera() {
        if (!Config::enableFreeCamera) return;
        if (IsNativeObjectAlive(mainCameraCache) && IsNativeObjectAlive(cameraTransformCache)) return;
        Log::DebugFmt("mainCameraCache is Alive");

        mainCameraCache = UnityResolve::UnityType::Camera::GetMain();
        Log::DebugFmt("mainCameraCache is at %p", mainCameraCache);
        if (mainCameraCache) cameraTransformCache = mainCameraCache->GetTransform();
    }
    // 不知何用
//    DEFINE_HOOK(bool, VLDOF_IsActive, (void* self)) {
////        if (Config::enableFreeCamera) return false;
//        Log::DebugFmt("VLDOF_IsActive HOOKED");
//        return VLDOF_IsActive_Orig(self);
//    }
    // hook set FOV函数，如果是自由相机，则强制设置fov = 60
    DEFINE_HOOK(void, Unity_set_fieldOfView, (UnityResolve::UnityType::Camera* self, float value)) {

        static auto last_log_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count() >= 5) {
            Log::DebugFmt("Unity_set_fieldOfView HOOKED: camera is %p", self);
            last_log_time = now;
        }
        if (Config::enableFreeCamera && mainCameraCache) {
//            Log::DebugFmt("Unity_set_fieldOfView HOOKED");
            Unity_set_fieldOfView_Orig(mainCameraCache, GKCamera::baseCamera.fov);
//            if (self == mainCameraCache) {
//                value = GKCamera::baseCamera.fov;
//            }
        }
        Unity_set_fieldOfView_Orig(self, value);
    }
    // hook get FOV函数，如果是自由相机，则强制返回fov = 60
    DEFINE_HOOK(float, Unity_get_fieldOfView, (UnityResolve::UnityType::Camera* self)) {
        if (Config::enableFreeCamera) {
//            Log::DebugFmt("Unity_get_fieldOfView HOOKED");
            if (mainCameraCache) {
//                static auto get_orthographic = reinterpret_cast<bool (*)(void*)>(Il2cppUtils::il2cpp_resolve_icall(
//                        "UnityEngine.Camera::get_orthographic()"
//                ));
//                static auto set_orthographic = reinterpret_cast<bool (*)(void*, bool)>(Il2cppUtils::il2cpp_resolve_icall(
//                        "UnityEngine.Camera::set_orthographic(System.Boolean)"
//                ));

                for (const auto& i : UnityResolve::UnityType::Camera::GetAllCamera()) {
                    // Log::DebugFmt("get_orthographic: %d", get_orthographic(i));
                    // set_orthographic(i, false);
                    Unity_set_fieldOfView_Orig(i, GKCamera::baseCamera.fov);
                }
                Unity_set_fieldOfView_Orig(mainCameraCache, GKCamera::baseCamera.fov);

                // Log::DebugFmt("main - get_orthographic: %d", get_orthographic(self));
//                return GKCamera::baseCamera.fov;
            }
        }
        return Unity_get_fieldOfView_Orig(self);
    }
    // 设置旋转操作
    DEFINE_HOOK(void, Unity_set_rotation_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Quaternion* value)) {
        if (Config::enableFreeCamera && cameraTransformCache) {
//            Log::DebugFmt("Unity_set_rotation_Injected HOOKED");
            static auto lookat_injected = reinterpret_cast<void (*)(void*cameraTransformCache,
                                                                    UnityResolve::UnityType::Vector3* worldPosition, UnityResolve::UnityType::Vector3* worldUp)>(
                    Il2cppUtils::il2cpp_resolve_icall(
                            "UnityEngine.Transform::Internal_LookAt_Injected(UnityEngine.Vector3&,UnityEngine.Vector3&)"));
            static auto worldUp = UnityResolve::UnityType::Vector3(0, 1, 0);

//            if (cameraTransformCache == self) {
                const auto cameraMode = GKCamera::GetCameraMode();
                if (cameraMode == GKCamera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        if (GKCamera::GetFirstPersonRoll() == GKCamera::FirstPersonRoll::ENABLE_ROLL) {
                            *value = cacheRotation;
                        }
                        else {
                            static LinkuraLocal::Misc::FixedSizeQueue<float> recordsY(60);
                            const auto newY = GKCamera::CheckNewY(cacheLookAt, true, recordsY);
                            UnityResolve::UnityType::Vector3 newCacheLookAt{cacheLookAt.x, newY, cacheLookAt.z};
                            lookat_injected(self, &newCacheLookAt, &worldUp);
                            return;
                        }
                    }
                }
                else if (cameraMode == GKCamera::CameraMode::FOLLOW) {
                    auto newLookAtPos = GKCamera::CalcFollowModeLookAt(cachePosition,
                                                                       GKCamera::followPosOffset, true);
                    lookat_injected(self, &newLookAtPos, &worldUp);
                    return;
                }
                else {
                    auto& origCameraLookat = GKCamera::baseCamera.lookAt;
                    lookat_injected(cameraTransformCache, &origCameraLookat, &worldUp);
                    // Log::DebugFmt("fov: %f, target: %f", Unity_get_fieldOfView_Orig(mainCameraCache), GKCamera::baseCamera.fov);
//                    return;
                }
//            }
        }
        return Unity_set_rotation_Injected_Orig(self, value);
    }
    // hook 位移操作
    DEFINE_HOOK(void, Unity_set_position_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Vector3* data)) {
        if (Config::enableFreeCamera && cameraTransformCache) {
//            Log::DebugFmt("Unity_set_position_Injected HOOKED");
//            CheckAndUpdateMainCamera();
//            Log::DebugFmt("Check and update main camera finished");
//            if (cameraTransformCache == self) {
                const auto cameraMode = GKCamera::GetCameraMode();
                if (cameraMode == GKCamera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        *data = GKCamera::CalcFirstPersonPosition(cachePosition, cacheForward, GKCamera::firstPersonPosOffset);
                    }

                }
                else if (cameraMode == GKCamera::CameraMode::FOLLOW) {
                    auto newLookAtPos = GKCamera::CalcFollowModeLookAt(cachePosition, GKCamera::followPosOffset);
                    auto pos = GKCamera::CalcPositionFromLookAt(newLookAtPos, GKCamera::followPosOffset);
                    data->x = pos.x;
                    data->y = pos.y;
                    data->z = pos.z;
                }
                else {
                    auto& origCameraPos = GKCamera::baseCamera.pos;
                    UnityResolve::UnityType::Vector3 newPos{origCameraPos.x, origCameraPos.y, origCameraPos.z};
                    Log::DebugFmt("MainCamera set pos: %f, %f, %f", newPos.x, newPos.y, newPos.z);
                    Unity_set_position_Injected_Orig(cameraTransformCache, &newPos);
//                    data->x = origCameraPos.x;
//                    data->y = origCameraPos.y;
//                    data->z = origCameraPos.z;
                }
//            }
        }

        return Unity_set_position_Injected_Orig(self, data);
    }
    // 这是？但是与Unity有关
    DEFINE_HOOK(void, EndCameraRendering, (void* ctx, void* camera, void* method)) {
        EndCameraRendering_Orig(ctx, camera, method);

        if (Config::enableFreeCamera && mainCameraCache) {
//            Log::DebugFmt("EndCameraRendering HOOKED: mainCameraCache %p", mainCameraCache);
            Unity_set_fieldOfView_Orig(mainCameraCache, GKCamera::baseCamera.fov);
            if (GKCamera::GetCameraMode() == GKCamera::CameraMode::FIRST_PERSON) {
                mainCameraCache->SetNearClipPlane(0.001f);
            }
        }
    }

    // 可能只对wm有用，wm只会调用一次，而且调用的就是我们需要的camera?
    // fes live一定没用
    // 活动记录没用
    DEFINE_HOOK(UnityResolve::UnityType::Camera*, CameraManager_GetCamera, (Il2cppUtils::Il2CppObject* self, CameraType cameraType , int cameraId , void* method)) {
        Log::DebugFmt("CameraManager_GetCamera HOOKED");
        Log::DebugFmt("cameraType: %d, cameraId: %d", cameraType, cameraId);
        auto camera =  CameraManager_GetCamera_Orig(self, cameraType, cameraId, method);
        Log::DebugFmt("Get camera at %p", camera);
        mainCameraCache = camera;
        if (mainCameraCache) cameraTransformCache = mainCameraCache->GetTransform();
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

#pragma region Camera
        ADD_HOOK(FesLiveCameraSwitcher_SwitchCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesLiveCameraSwitcher", "SwitchCamera"));
        ADD_HOOK(FesLiveFixedCamera_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesLiveFixedCamera", "GetCamera"));
        ADD_HOOK(DynamicCamera_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "DynamicCamera", "GetCamera"));
        ADD_HOOK(IdolTargetingCamera_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "IdolTargetingCamera", "GetCamera"));
        ADD_HOOK(IdolTargetingCamera_SetTargetIdol, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "IdolTargetingCamera", "SetTargetIdol"));
        ADD_HOOK(WithLiveCameraReference_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "WithLiveCameraReference", ".ctor"));

        ADD_HOOK(DynamicCamera_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "DynamicCamera", ".ctor"));
        ADD_HOOK(DynamicCamera_FindCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "DynamicCamera", "FindCamera"));
        ADD_HOOK(FixedCamera_Initialize, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "FixedCamera", "Initialize"));
        ADD_HOOK(CameraManager_CreatePresetCameras, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "CreatePresetCameras"));
        ADD_HOOK(CameraManager_GetCamera, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "GetCamera"));

        // not used
        ADD_HOOK(SwipeCamera_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.LiveMain", "SwipeCamera", ".ctor"));
#pragma endregion

#pragma region FreeCamera_ADD_HOOK
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

        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));

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
        GKCamera::initCameraSettings(); // free camera

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
}
