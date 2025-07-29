#include "../HookMain.h"
#include "../config/Config.hpp"
#include "../camera/camera.hpp"
#include "../Misc.hpp"
#include "../../build/linkura_messages.pb.h"
#include "thread"
#include "chrono"

namespace LinkuraLocal::HookCamera {
#pragma region FreeCamera
    UnityResolve::UnityType::Camera* mainFreeCameraCache = nullptr;
    UnityResolve::UnityType::Transform* freeCameraTransformCache = nullptr;
    UnityResolve::UnityType::Transform* cacheTrans = nullptr;
    UnityResolve::UnityType::Quaternion cacheRotation{};
    UnityResolve::UnityType::Vector3 cachePosition{};
    UnityResolve::UnityType::Vector3 cacheForward{};
    UnityResolve::UnityType::Vector3 cacheLookAt{};
    bool initialCameraRendered = false;

    void registerMainFreeCamera(UnityResolve::UnityType::Camera* mainCamera) {
        if (!Config::enableFreeCamera) {
            L4Camera::SetCameraMode(L4Camera::CameraMode::SYSTEM_CAMERA);
            return;
        };
        L4Camera::SetCameraMode(L4Camera::CameraMode::FREE);
        mainFreeCameraCache = mainCamera;
        if (mainFreeCameraCache) freeCameraTransformCache = mainFreeCameraCache->GetTransform();
    }

    void sanitizeFreeCamera(UnityResolve::UnityType::Camera* mainCamera) {
        auto camera = mainCamera;
        if (camera) {
            auto transform = camera->GetTransform();
            if (transform) {
                auto position = transform->GetPosition();
                L4Camera::originCamera.setPos(position.x, position.y, position.z);
                L4Camera::originCamera.fov = 26.225;
                L4Camera::originCamera.lookAt = cacheLookAt;
                L4Camera::baseCamera.setCamera(&L4Camera::originCamera);
            }
        }
        initialCameraRendered = true;
    }

    void unregisterMainFreeCamera(bool cleanup = false) {
        if (!Config::enableFreeCamera) return;
        pauseCameraInfoLoopFromNative();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

    void onRenderExit() {
        HookShare::Shareable::resetRenderScene();
        unregisterMainFreeCamera(true);
        unregisterCurrentCamera();
        L4Camera::reset_camera();
        L4Camera::followCharaSet.clear();
        HookShare::Shareable::realtimeRenderingArchiveControllerCache = nullptr;
        initialCameraRendered = false;
    }

    std::vector<uint8_t> getCameraInfoProtobuf() {
        linkura::ipc::CameraData cameraData;
        if (HookShare::Shareable::renderSceneIsNone() || (HookShare::Shareable::renderSceneIsWithLive() && !currentCameraRegistered)) {
            // connecting status
            Log::DebugFmt("Camera Data return connecting");
            cameraData.set_is_connecting(true);
            cameraData.set_is_valid(false);
        } else if (!currentCameraRegistered) {
            Log::DebugFmt("Camera Data return false");
            cameraData.set_is_valid(false);
        } else if (currentCameraCache && IsNativeObjectAlive(currentCameraCache)) {
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
                cameraData.set_scene_type(static_cast<int32_t>(HookShare::Shareable::renderScene));
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
        if (Config::enableFreeCamera && !HookShare::Shareable::renderSceneIsNone()) {
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
                            // maybe not working
                            Unity_set_rotation_Injected_Orig(freeCameraTransformCache, &cacheRotation);
                            return;
                        }
                        else {
//                            Log::DebugFmt("set rotation, cacheLookAt is at (%f, %f, %f)", cacheLookAt.x, cacheLookAt.y, cacheLookAt.z);
                            static LinkuraLocal::Misc::FixedSizeQueue<float> recordsY(60);
                            const auto newY = L4Camera::CheckNewY(cacheLookAt, true, recordsY);
                            UnityResolve::UnityType::Vector3 newCacheLookAt{cacheLookAt.x, newY, cacheLookAt.z};
                            lookat_injected(freeCameraTransformCache, &newCacheLookAt, &worldUp);
                            return;
                        }
                    }
                }
                else if (cameraMode == L4Camera::CameraMode::FOLLOW) {
                    auto newLookAtPos = L4Camera::CalcFollowModeLookAt(cachePosition,
                                                                       L4Camera::followPosOffset, true);
                    lookat_injected(freeCameraTransformCache, &newLookAtPos, &worldUp);
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
        if (Config::enableFreeCamera && !HookShare::Shareable::renderSceneIsNone()) {
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
        if (Config::enableFreeCamera && !HookShare::Shareable::renderSceneIsNone()) {
            if (IsNativeObjectAlive(mainFreeCameraCache) && self == mainFreeCameraCache) {
                value = L4Camera::baseCamera.fov;
            }
        }
        Unity_set_fieldOfView_Orig(self, value);
    }
    DEFINE_HOOK(float, Unity_get_fieldOfView, (UnityResolve::UnityType::Camera* self)) {
        if (Config::enableFreeCamera && !HookShare::Shareable::renderSceneIsNone()) {
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
        if (Config::enableFreeCamera && !HookShare::Shareable::renderSceneIsNone()) {
            if (IsNativeObjectAlive(mainFreeCameraCache)) {
                // prevent crash for with live and fes live & remain the free fov for story
                if (HookShare::Shareable::renderSceneIsStory()) Unity_set_fieldOfView_Orig(mainFreeCameraCache, L4Camera::baseCamera.fov);
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
        if (!HookShare::Shareable::renderSceneIsFesLive()) {
            HookShare::Shareable::renderScene = HookShare::Shareable::RenderScene::WithLive;
            if (!initialCameraRendered) {
                sanitizeFreeCamera(camera);
            }
            registerMainFreeCamera(camera);
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
            L4Camera::followCharaSet.clear();
            // fes live will use the same fixed camera at all time
            if (HookShare::Shareable::renderSceneIsWithLive()) {
                Log::DebugFmt("LiveConnectChapterModel_NewChapterConfirmed HOOKED");
                HookShare::Shareable::resetRenderScene();
                unregisterMainFreeCamera();
                unregisterCurrentCamera();
            }
        }
        LiveConnectChapterModel_NewChapterConfirmed_Orig(self, method);
    }
    // exit
    DEFINE_HOOK(void*, LiveSceneController_FinalizeSceneAsync, (Il2cppUtils::Il2CppObject* self, void* token, void* method)) {
        Log::DebugFmt("LiveSceneController_FinalizeSceneAsync HOOKED");
        onRenderExit();
        return LiveSceneController_FinalizeSceneAsync_Orig(self, token, method);
    }

    DEFINE_HOOK(void, StoryModelSpaceManager_Init, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("StoryModelSpaceManager_Init HOOKED");
        HookShare::Shareable::renderScene = HookShare::Shareable::RenderScene::Story;

        static auto get_ModelSpace = UnityResolve::Get("Assembly-CSharp.dll")->Get(
                "StoryModelSpaceManager")->Get<UnityResolve::Method>("get_modelSpace");
        static auto get_StoryCamera = UnityResolve::Get("Assembly-CSharp.dll")->Get("StoryModelSpace")->Get<UnityResolve::Method>("get_StoryCamera");
        StoryModelSpaceManager_Init_Orig(self, method);
        auto modelSpace = get_ModelSpace->Invoke<Il2cppUtils::Il2CppObject*>(self);
        auto storyCamera = get_StoryCamera->Invoke<UnityResolve::UnityType::Camera*>(modelSpace);
//        if (!initialCameraRendered) {
//            sanitizeFreeCamera(storyCamera); // not working as expected
//        }
        registerMainFreeCamera(storyCamera);
        registerCurrentCamera(storyCamera);
    }
    DEFINE_HOOK(void, StoryScene_OnFinalize, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("StoryScene_OnFinalize HOOKED");
        onRenderExit();
        StoryScene_OnFinalize_Orig(self, method);
    }

#pragma endregion

#pragma region Camera
    // fes live only
    DEFINE_HOOK(UnityResolve::UnityType::Camera*, FesLiveFixedCamera_GetCamera, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("FesLiveFixedCamera_GetCamera HOOKED");
        auto camera = FesLiveFixedCamera_GetCamera_Orig(self, method);
        if (!initialCameraRendered) {
            sanitizeFreeCamera(camera);
        }
        registerMainFreeCamera(camera);
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

#pragma region FirstPersonCamera

    DEFINE_HOOK(void, AvatarPool_AddIfAvatar, (Il2cppUtils::Il2CppObject* self, UnityResolve::UnityType::Object* gameObject, void* method)) {
        Log::DebugFmt("AvatarPool_AddIfAvatar HOOKED");
        return AvatarPool_AddIfAvatar_Orig(self, gameObject, method);
    }

    // ✅
    // 由于无法像学马一样获取管理器与，只能记录出现过的地址，并判断
    // 活动记录的无法及时清除退出渲染的FaceBonesCopier，但是问题不大
    DEFINE_HOOK(void, FaceBonesCopier_LastUpdate, (void* self, void* mtd)) {
        if (!Config::enableFreeCamera || (L4Camera::GetCameraMode() == L4Camera::CameraMode::FREE)) {
//            if (needRestoreHides) {
//                needRestoreHides = false;
//                HideHead(nullptr, false);
//                HideHead(nullptr, true);
//            }
            return FaceBonesCopier_LastUpdate_Orig(self, mtd);
        }

        static auto FaceBonesCopier_klass = Il2cppUtils::GetClass("Core.dll", "Inspix", "FaceBonesCopier");
        static auto head_field = FaceBonesCopier_klass->Get<UnityResolve::Field>("head");
//        static auto origin_head_field = FaceBonesCopier_klass->Get<UnityResolve::Field>("originalHead");
//        static auto left_eye_field = FaceBonesCopier_klass->Get<UnityResolve::Field>("leftEye");
        if (!L4Camera::followCharaSet.contains(self)) {
            L4Camera::followCharaSet.add(self);
        }
        auto currentFace = L4Camera::followCharaSet.getCurrentValue();
        auto headTrans = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(currentFace, head_field);
        cacheTrans = headTrans;
        cacheRotation = cacheTrans->GetRotation();
        cachePosition = cacheTrans->GetPosition();
        cacheForward = cacheTrans->GetUp();
        cacheLookAt = cacheTrans->GetPosition() + cacheForward * 3;
//            Log::DebugFmt("headTrans: pos: %f %f %f, rot: %f %f %f %f, forward: %f %f %f, lookat: %f %f %f",
//                          cachePosition.x, cachePosition.y, cachePosition.z,
//                          cacheRotation.x, cacheRotation.y, cacheRotation.z, cacheRotation.w,
//                          cacheForward.x, cacheForward.y, cacheForward.z,
//                          cacheLookAt.x, cacheLookAt.y, cacheLookAt.z
//                          );
        return FaceBonesCopier_LastUpdate_Orig(self, mtd);
    }

#pragma endregion

    void Install(HookInstaller* hookInstaller) {
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
        ADD_HOOK(AvatarPool_AddIfAvatar, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "AvatarPool", "AddIfAvatar"));
        ADD_HOOK(FaceBonesCopier_LastUpdate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "FaceBonesCopier", "LateUpdate"));
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
#pragma region FirstPersonCamera


#pragma endregion
        ADD_HOOK(LiveConnectChapterModel_NewChapterConfirmed, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "LiveConnectChapterModel", "NewChapterConfirmed"));
        ADD_HOOK(LiveSceneController_FinalizeSceneAsync, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneController", "FinalizeSceneAsync"));
        ADD_HOOK(StoryModelSpaceManager_Init, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryModelSpaceManager", "Init"));
        ADD_HOOK(StoryScene_OnFinalize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "OnFinalize"));

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
    }
}