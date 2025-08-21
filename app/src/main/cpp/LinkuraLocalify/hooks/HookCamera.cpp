#include "../HookMain.h"
#include "../config/Config.hpp"
#include "../camera/camera.hpp"
#include "../Misc.hpp"
#include "../../build/linkura_messages.pb.h"
#include "thread"
#include "chrono"
#include "vector"
#include <re2/re2.h>

namespace LinkuraLocal::HookCamera {
#pragma region FreeCamera
    UnityResolve::UnityType::Camera* mainFreeCameraCache = nullptr;
    UnityResolve::UnityType::Camera* backgroundColorCameraCache = nullptr;
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
                auto forward = transform->GetForward();
                L4Camera::originCamera.setPos(position.x, position.y, position.z);
                L4Camera::originCamera.fov = 26.225;
                L4Camera::originCamera.lookAt = UnityResolve::UnityType::Vector3{
                        position.x + forward.x,
                        position.y + forward.y,
                        position.z + forward.z
                };
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
        L4Camera::clearRenderSet();
        unregisterMainFreeCamera(true);
        unregisterCurrentCamera();
        L4Camera::reset_camera();
        HookShare::Shareable::realtimeRenderingArchiveControllerCache = nullptr;
        HookShare::Shareable::currentArchiveDuration = 0;
        HookShare::Shareable::currentArchiveId = "";
        initialCameraRendered = false;
        backgroundColorCameraCache = nullptr;
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
        } else if (currentCameraCache && Il2cppUtils::IsNativeObjectAlive(currentCameraCache)) {
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

    DEFINE_HOOK(void, Unity_camera_set_backgroundColor_Injected, (UnityResolve::UnityType::Camera* self, UnityResolve::UnityType::Color* value)) {
        backgroundColorCameraCache = self;
        Unity_camera_set_backgroundColor_Injected_Orig(self, &L4Camera::backgroundColor);
    }

    void setCameraBackgroundColor(float red, float green, float blue, float alpha) {
        UnityResolve::UnityType::Color color{red, green, blue, alpha};
        auto storedColor = &L4Camera::backgroundColor;
        *storedColor = color;
        if (backgroundColorCameraCache && !HookShare::Shareable::renderSceneIsNone()) {
            Unity_camera_set_backgroundColor_Injected_Orig(backgroundColorCameraCache, &color);
        }
    }

    DEFINE_HOOK(void, Unity_set_rotation_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Quaternion* value)) {
        if (Config::enableFreeCamera && !HookShare::Shareable::renderSceneIsNone()) {
            if (Il2cppUtils::IsNativeObjectAlive(freeCameraTransformCache)) {
                static auto lookat_injected = reinterpret_cast<void (*)(void*freeCameraTransformCache,
                                                                        UnityResolve::UnityType::Vector3* worldPosition, UnityResolve::UnityType::Vector3* worldUp)>(
                        Il2cppUtils::il2cpp_resolve_icall(
                                "UnityEngine.Transform::Internal_LookAt_Injected(UnityEngine.Vector3&,UnityEngine.Vector3&)"));
                static auto worldUp = UnityResolve::UnityType::Vector3(0, 1, 0);
                const auto cameraMode = L4Camera::GetCameraMode();
                if (cameraMode == L4Camera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && Il2cppUtils::IsNativeObjectAlive(cacheTrans)) {
                        if (L4Camera::GetFirstPersonRoll() == L4Camera::FirstPersonRoll::ENABLE_ROLL) {
                            // maybe not working
                            Unity_set_rotation_Injected_Orig(freeCameraTransformCache, &cacheRotation);
                            return;
                        } else {
//                            Log::DebugFmt("set rotation, cacheLookAt is at (%f, %f, %f)", cacheLookAt.x, cacheLookAt.y, cacheLookAt.z);
                            static LinkuraLocal::Misc::FixedSizeQueue<float> recordsY(60);
                            const auto newY = L4Camera::CheckNewY(cacheLookAt, true, recordsY);
                            UnityResolve::UnityType::Vector3 newCacheLookAt{cacheLookAt.x, newY, cacheLookAt.z};
                            lookat_injected(freeCameraTransformCache, &newCacheLookAt, &worldUp);
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
            if (Il2cppUtils::IsNativeObjectAlive(freeCameraTransformCache)) {
                const auto cameraMode = L4Camera::GetCameraMode();
                if (cameraMode == L4Camera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && Il2cppUtils::IsNativeObjectAlive(cacheTrans)) {
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
            if (Il2cppUtils::IsNativeObjectAlive(mainFreeCameraCache) && self == mainFreeCameraCache) {
                value = L4Camera::baseCamera.fov;
            }
        }
        Unity_set_fieldOfView_Orig(self, value);
    }
    DEFINE_HOOK(float, Unity_get_fieldOfView, (UnityResolve::UnityType::Camera* self)) {
        if (Config::enableFreeCamera && !HookShare::Shareable::renderSceneIsNone()) {
            if (Il2cppUtils::IsNativeObjectAlive(mainFreeCameraCache)) {
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
            if (Il2cppUtils::IsNativeObjectAlive(mainFreeCameraCache)) {
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


    DEFINE_HOOK(void, CameraManager_AddCamera, (Il2cppUtils::Il2CppObject* self, void* baseCamera, void* method)) {
        if (Config::isLegacyMrsVersion()) {
            Log::DebugFmt("CameraManager_AddCamera HOOKED");
            static auto CameraManager_klass = Il2cppUtils::GetClass("Core.dll", "Inspix", "BaseCamera");
            static auto CameraManager_get_Name = CameraManager_klass->Get<UnityResolve::Method>("get_Name");
            static auto CameraManager_get_CameraComponent = CameraManager_klass->Get<UnityResolve::Method>("get_CameraComponent");
            auto name = CameraManager_get_Name->Invoke<Il2cppUtils::Il2CppString*>(baseCamera);
            auto camera = CameraManager_get_CameraComponent->Invoke<UnityResolve::UnityType::Camera*>(baseCamera);
            Log::DebugFmt("CameraManager_AddCamera: %s, camera is at %p", name->ToString().c_str(), camera);

            // Use RE2 to match pattern: 6 digits + underscore + letters (e.g., 250222_abc、 250412)
            static re2::RE2 pattern(R"(^\d{6}(_[a-zA-Z]+$)?)");
            std::string nameStr = name->ToString();
            if (re2::RE2::FullMatch(nameStr, pattern) && !nameStr.ends_with("after")) {
                if (!HookShare::Shareable::renderSceneIsFesLive()) {
                    HookShare::Shareable::renderScene = HookShare::Shareable::RenderScene::WithLive;
                    if (!initialCameraRendered) {
                        sanitizeFreeCamera(camera);
                    }
                    registerMainFreeCamera(camera);
                    registerCurrentCamera(camera);
                }
            }
        }
        CameraManager_AddCamera_Orig(self, baseCamera, method);
    }

    // useless
    DEFINE_HOOK(void*, CameraManager_get_Cameras, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("CameraManager_get_Cameras HOOKED");
        return CameraManager_get_Cameras_Addr(self, method);
    }
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
            if (!Config::isLegacyMrsVersion()) {
                L4Camera::clearRenderSet();
                // fes live will use the same fixed camera at all time
                if (HookShare::Shareable::renderSceneIsWithLive()) {
                    Log::DebugFmt("LiveConnectChapterModel_NewChapterConfirmed HOOKED");
                    auto cameraMode = L4Camera::GetCameraMode();
                    if (cameraMode == L4Camera::CameraMode::FOLLOW || cameraMode == L4Camera::CameraMode::FIRST_PERSON) {
                        // Log::DebugFmt("set camera mode to FREE");
                        L4Camera::SetCameraMode(L4Camera::CameraMode::FREE);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    HookShare::Shareable::resetRenderScene();
                    unregisterMainFreeCamera();
                    unregisterCurrentCamera();
                }
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
        HookLiveRender::applyCameraGraphicSettings(storyCamera);
        backgroundColorCameraCache = storyCamera;
        Unity_camera_set_backgroundColor_Injected_Orig(backgroundColorCameraCache, &L4Camera::backgroundColor);
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
        return AvatarPool_AddIfAvatar_Orig(self, gameObject, method);
    }

    void printChildren(UnityResolve::UnityType::Transform* obj, const std::string& obj_name) {
        const auto childCount = obj->GetChildCount();
        for (int i = 0;i < childCount; i++) {
            auto child = obj->GetChild(i);
            const auto childName = child->GetName();
            Log::DebugFmt("%s child: %s", obj_name.c_str(), childName.c_str());
        }
    }

    DEFINE_HOOK(void, FaceBonesCopier_LastUpdate, (void* self, void* mtd)) {
        static auto FaceBonesCopier_klass = Il2cppUtils::GetClass("Core.dll", "Inspix", "FaceBonesCopier");
        static auto head_field = FaceBonesCopier_klass->Get<UnityResolve::Field>("head");
        static auto spine03_field = FaceBonesCopier_klass->Get<UnityResolve::Field>("spine03");
        if (!L4Camera::charaRenderSet.contains(self)) {
            L4Camera::charaRenderSet.add(self);
        }
        if (Config::hideCharacterBody) {
            L4Camera::charaRenderSet.hide(self);
        }
        if (!Config::enableFreeCamera || (L4Camera::GetCameraMode() == L4Camera::CameraMode::FREE)) {
            return FaceBonesCopier_LastUpdate_Orig(self, mtd);
        }

        if (!L4Camera::followCharaSet.contains(self)) {
            L4Camera::followCharaSet.add(self);
        }

        auto currentFace = L4Camera::followCharaSet.getCurrentValue();

        {
            auto headTrans = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(currentFace, head_field);
            cacheTrans = headTrans;
            cacheRotation = cacheTrans->GetRotation();
            cachePosition = cacheTrans->GetPosition();
            cacheForward = cacheTrans->GetUp();
            cacheLookAt = cacheTrans->GetPosition() + cacheForward * 3;
        }

        auto spineTrans = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(currentFace, spine03_field);
        auto modelParent = spineTrans->GetParent();
        auto faceMeshes = Il2cppUtils::GetNestedTransformChildren(modelParent, {
                [](const std::string& childName) {
                    return childName == "Mesh";
                }
        });
        if (!faceMeshes.empty()) {
            auto faceHolder = faceMeshes[0];
            recursiveAddFaceMesh(faceHolder, L4Camera::followCharaSet);
        }

        auto costume = modelParent->GetParent()->GetParent()->GetParent();
        // costume -> SCSch* -> Model -> Mesh
        auto hairResult = Il2cppUtils::GetNestedTransformChildren(costume, {
                [](const std::string& name) { return name.starts_with("SCSch"); },
                [](const std::string& name) { return name == "Model"; },
                [](const std::string& name) { return name == "Mesh"; },
                [](const std::string& name) { return name.starts_with("Hair"); }
        });
        if (hairResult.empty()) {
            hairResult = Il2cppUtils::GetNestedTransformChildren(costume, {
                    [](const std::string &name) { return name.starts_with("SCSch"); },
                    [](const std::string &name) { return name == "Model"; },
                    [](const std::string& name) { return name == "Mesh"; },
                    [](const std::string& name) { return name == "Body"; },
                    [](const std::string& name) { return name.starts_with("Hair"); }
            });
        }

        if (!hairResult.empty()) {
            L4Camera::followCharaSet.addCharaHair(hairResult[0]);
        }

        if (L4Camera::GetCameraMode() == L4Camera::CameraMode::FIRST_PERSON) {
            if (L4Camera::followCharaSet.currentHairIsRendered()) {
                L4Camera::followCharaSet.hideCurrentCharaMeshes();
            }
        }

        return FaceBonesCopier_LastUpdate_Orig(self, mtd);
    }

    DEFINE_HOOK(void, Unity_Renderer_set_enabled, (void* renderer, bool enabled, void* method)) {
        if (Config::hideCharacterBody) {
            if (L4Camera::charaRenderSet.containsRender(renderer)) {
                enabled = false;
            }
        }
        return Unity_Renderer_set_enabled_Orig(renderer, enabled, method);
    }

    // hooked  in fes live
    DEFINE_HOOK(void, CharacterEyeController_LastUpdate, (void* self, void* mtd)) {
        return CharacterEyeController_LastUpdate_Orig(self, mtd);
    }

    // story
    DEFINE_HOOK(void, SubBoneController_LateUpdate, (void* self, void* mtd)) {
//        Log::DebugFmt("SubBoneController_LateUpdate HOOKED, %p", self);
        return SubBoneController_LateUpdate_Orig(self, mtd);
    }

    // hooked in fes live
    DEFINE_HOOK(void, ItemSyncTransform_SyncTransform, (void* self, UnityResolve::UnityType::Transform* bindBone, void* method)) {
        return ItemSyncTransform_SyncTransform_Orig(self, bindBone, method);
    }

    //  not hooked
    DEFINE_HOOK(void, ItemSyncTransform_SetupSyncTransform, (void* self,uint32_t c_id, UnityResolve::UnityType::Transform* bindBone, void* mtd)) {
        Log::DebugFmt("ItemSyncTransform_SetupSyncTransform HOOKED, %p", self);
        return ItemSyncTransform_SetupSyncTransform_Orig(self, c_id, bindBone, mtd);
    }

    DEFINE_HOOK(void*, SubBoneController_GetAllChildren, (Il2cppUtils::Il2CppObject* self,UnityResolve::UnityType::Transform* target,void* candidates , void* method)) {
        return SubBoneController_GetAllChildren_Orig(self,target,candidates, method);
    }

    DEFINE_HOOK(void, FaceBonesCopier_Start, (void* self, void* mtd)) {
        return FaceBonesCopier_Start_Orig(self, mtd);
    }

    DEFINE_HOOK(void, ModelHandler_SetRendererEnable, (Il2cppUtils::Il2CppObject* self, bool enable, void* method)) {
//        Log::DebugFmt("ModelHandler_SetRendererEnable HOOKED, %p, %s",self , enable ? "enable" : "disable");
        return ModelHandler_SetRendererEnable_Orig(self, enable, method);
    }

    // not work
    DEFINE_HOOK(void, CategorizedMeshRenderer_ctor, (Il2cppUtils::Il2CppObject* self, int32_t category, void* method)) {
        Log::DebugFmt("CategorizedMeshRenderer_ctor HOOKED, category is %d", category);
        return CategorizedMeshRenderer_ctor_Orig(self, category, method);
    }

    DEFINE_HOOK(UnityResolve::UnityType::Transform*, AvatarSettings_GetBoneTransform, (Il2cppUtils::Il2CppObject* self, int32_t humanBodyBones, void* method)) {
        Log::DebugFmt("AvatarSettings_GetBoneTransform HOOKED, humanBodyBones is %d", humanBodyBones);
        return AvatarSettings_GetBoneTransform_Orig(self, humanBodyBones, method);
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
#pragma region FreeCamera_ADD_HOOK
        ADD_HOOK(FesLiveFixedCamera_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "FesLiveFixedCamera", "GetCamera"));
        ADD_HOOK(CameraManager_GetCamera, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "GetCamera"));
        ADD_HOOK(CameraManager_AddCamera, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "AddCamera"));
        ADD_HOOK(CameraManager_get_Cameras, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CameraManager", "get_Cameras"));
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
        ADD_HOOK(AvatarPool_AddIfAvatar, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "AvatarPool", "AddIfAvatar"));
        ADD_HOOK(CharacterEyeController_LastUpdate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.CharacterAttention", "CharacterEyeController", "LateUpdate"));
        ADD_HOOK(FaceBonesCopier_LastUpdate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "FaceBonesCopier", "LateUpdate"));
        ADD_HOOK(SubBoneController_LateUpdate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "SubBoneController", "LateUpdate"));

        ADD_HOOK(ItemSyncTransform_SyncTransform, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.Item", "ItemSyncTransform", "SyncTransform"));
        ADD_HOOK(ItemSyncTransform_SetupSyncTransform, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.Item", "ItemSyncTransform", "SetupSyncTransform"));

        ADD_HOOK(SubBoneController_GetAllChildren, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "SubBoneController", "GetAllChildren"));
        ADD_HOOK(FaceBonesCopier_Start, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "FaceBonesCopier", "Start"));
        ADD_HOOK(ModelHandler_SetRendererEnable, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "ModelHandler", "SetRendererEnable"));
        ADD_HOOK(CategorizedMeshRenderer_ctor, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CategorizedMeshRenderer", ".ctor"));
        ADD_HOOK(AvatarSettings_GetBoneTransform, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "AvatarSettings", "GetBoneTransform"));
#pragma endregion
        ADD_HOOK(LiveConnectChapterModel_NewChapterConfirmed, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.LiveMain", "LiveConnectChapterModel", "NewChapterConfirmed"));
        ADD_HOOK(LiveSceneController_FinalizeSceneAsync, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneController", "FinalizeSceneAsync"));
        ADD_HOOK(StoryModelSpaceManager_Init, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryModelSpaceManager", "Init"));
        ADD_HOOK(StoryScene_OnFinalize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "OnFinalize"));

        ADD_HOOK(Unity_set_position_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)"));
        ADD_HOOK(Unity_set_rotation_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)"));
        ADD_HOOK(Unity_camera_set_backgroundColor_Injected, Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Camera::set_backgroundColor_Injected(UnityEngine.Color&)"));
        ADD_HOOK(Unity_get_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "get_fieldOfView"));
        ADD_HOOK(Unity_set_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "set_fieldOfView"));
        ADD_HOOK(EndCameraRendering, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine.Rendering",
                                                                   "RenderPipeline", "EndCameraRendering"));

        ADD_HOOK(Unity_Renderer_set_enabled, Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::set_enabled(System.Boolean)"));
#pragma endregion
    }
}