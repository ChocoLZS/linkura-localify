#pragma once
#include "baseCamera.hpp"
#include "../../deps/Joystick/JoystickEvent.h"
#include "map"
#include "../Il2cppUtils.hpp"

namespace L4Camera {
    enum class CameraMode {
        SYSTEM_CAMERA,
        FREE,
        FIRST_PERSON,
        FOLLOW
    };

    enum class FirstPersonRoll {
        ENABLE_ROLL,
        DISABLE_ROLL
    };

    enum class FollowModeY {
        APPLY_Y,
        SMOOTH_Y
    };

    void SetCameraMode(CameraMode mode);
    CameraMode GetCameraMode();
    void SetFirstPersonRoll(FirstPersonRoll mode);
    FirstPersonRoll GetFirstPersonRoll();


    template <typename T>
    class CharacterMeshManager : public LinkuraLocal::Misc::IndexedSet<T> {
    private:
        std::vector<std::map<std::string, UnityResolve::UnityType::Transform*>> charaMeshes;

    public:
        void onCameraModeChange(L4Camera::CameraMode cameraMode) {
            if (cameraMode != L4Camera::CameraMode::FIRST_PERSON) {
                LinkuraLocal::Log::DebugFmt("Trying to restore the meshes due to camera mode is not first_person");
                restoreCurrentCharaMeshes();
            }
        }

        void next() override {
            if (L4Camera::GetCameraMode() == L4Camera::CameraMode::FIRST_PERSON) {
                restoreCurrentCharaMeshes();
                LinkuraLocal::Misc::IndexedSet<T>::next();
                hideCurrentCharaMeshes();
            } else {
                LinkuraLocal::Misc::IndexedSet<T>::next();
            }
        }

        void prev() override {
            if (L4Camera::GetCameraMode() == L4Camera::CameraMode::FIRST_PERSON) {
                restoreCurrentCharaMeshes();
                LinkuraLocal::Misc::IndexedSet<T>::prev();
                hideCurrentCharaMeshes();
            } else {
                LinkuraLocal::Misc::IndexedSet<T>::prev();
            }
        }

        void clear() override {
            charaMeshes.clear();
            LinkuraLocal::Misc::IndexedSet<T>::clear();
        }

        void add(void* value)  {
            LinkuraLocal::Misc::IndexedSet<T>::add(value);
            std::map<std::string, UnityResolve::UnityType::Transform*> newMeshMap;
            charaMeshes.push_back(newMeshMap);
        }

        void addCharaMesh(const std::string& key, UnityResolve::UnityType::Transform* mesh) {
            if (LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex() < charaMeshes.size()) {
                auto& currentMeshMap = charaMeshes[LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex()];
                currentMeshMap[key] = mesh;
            }
        }

        bool containsCharaMesh(const std::string& key) {
            if (LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex() < charaMeshes.size()) {
                auto &currentMeshMap = charaMeshes[LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex()];
                return currentMeshMap.find(key) != currentMeshMap.end();
            }
            return false;
        }

        void hideCurrentCharaMeshes() {
            if (LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex() < charaMeshes.size()) {
                LinkuraLocal::Log::DebugFmt("Hide current chara meshes using Renderer.enabled");
                std::map<std::string, UnityResolve::UnityType::Transform*>& meshMap = charaMeshes[LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex()];
                
                static auto get_component = reinterpret_cast<UnityResolve::UnityType::Component* (*)(UnityResolve::UnityType::GameObject*, void*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.GameObject::GetComponent(System.Type)"));
                static auto get_enabled = reinterpret_cast<bool (*)(UnityResolve::UnityType::Component*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::get_enabled()"));
                static auto set_enabled = reinterpret_cast<void (*)(UnityResolve::UnityType::Component*, bool)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::set_enabled(System.Boolean)"));
                static auto rendererType = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer");

                for (auto& pair : meshMap) {
                    LinkuraLocal::Log::DebugFmt("Trying to hide renderer for %s", pair.first.c_str());
                    if (!(pair.second && Il2cppUtils::IsNativeObjectAlive(pair.second))) continue;
                    auto gameObject = pair.second->GetGameObject();
                    if (!(gameObject && Il2cppUtils::IsNativeObjectAlive(gameObject))) continue;
                    auto renderer = gameObject->GetComponent<UnityResolve::UnityType::Component*>(rendererType);
                    if (renderer && Il2cppUtils::IsNativeObjectAlive(renderer)) {
                        if (get_enabled && get_enabled(renderer)) {
                            set_enabled(renderer, false);
                        }
                    } else {
                        LinkuraLocal::Log::DebugFmt("No renderer found for %s", pair.first.c_str());
                    }
                }
            }
        }

        void restoreCurrentCharaMeshes() {
            if (LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex() < charaMeshes.size()) {
                LinkuraLocal::Log::DebugFmt("Restore current chara meshes using Renderer.enabled");
                std::map<std::string, UnityResolve::UnityType::Transform*>& meshMap = charaMeshes[LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex()];
                
                static auto get_component = reinterpret_cast<UnityResolve::UnityType::Component* (*)(UnityResolve::UnityType::GameObject*, void*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.GameObject::GetComponent(System.Type)"));
                static auto get_enabled = reinterpret_cast<bool (*)(UnityResolve::UnityType::Component*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::get_enabled()"));
                static auto set_enabled = reinterpret_cast<void (*)(UnityResolve::UnityType::Component*, bool)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::set_enabled(System.Boolean)"));
                static auto rendererType = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer");
                for (auto& pair : meshMap) {
                    LinkuraLocal::Log::DebugFmt("Trying to restore renderer for %s", pair.first.c_str());
                    if (!(pair.second && Il2cppUtils::IsNativeObjectAlive(pair.second))) continue;
                    auto gameObject = pair.second->GetGameObject();
                    if (!(gameObject && Il2cppUtils::IsNativeObjectAlive(gameObject))) continue;
                    auto renderer = gameObject->GetComponent<UnityResolve::UnityType::Component*>(rendererType);
                    if (renderer && Il2cppUtils::IsNativeObjectAlive(renderer)) {
                        if (get_enabled && !get_enabled(renderer)) {
                            set_enabled(renderer, true);
                        }
                    } else {
                        LinkuraLocal::Log::DebugFmt("No renderer found for %s", pair.first.c_str());
                    }
                }
            }
        }
    };

    extern BaseCamera::Camera baseCamera;
    extern BaseCamera::Camera originCamera;
    extern UnityResolve::UnityType::Vector3 firstPersonPosOffset;
    extern UnityResolve::UnityType::Vector3 followPosOffset;
    extern CharacterMeshManager<void*> followCharaSet;
    extern LinkuraLocal::Misc::CSEnum bodyPartsEnum;
    extern UnityResolve::UnityType::Color backgroundColor;

    float CheckNewY(const UnityResolve::UnityType::Vector3& targetPos, const bool recordY,
                    LinkuraLocal::Misc::FixedSizeQueue<float>& recordsY);

    UnityResolve::UnityType::Vector3 CalcPositionFromLookAt(const UnityResolve::UnityType::Vector3& target,
                                                            const UnityResolve::UnityType::Vector3& offset);

    UnityResolve::UnityType::Vector3 CalcFirstPersonPosition(const UnityResolve::UnityType::Vector3& position,
                                                             const UnityResolve::UnityType::Vector3& forward,
                                                             const UnityResolve::UnityType::Vector3& offset);

    UnityResolve::UnityType::Vector3 CalcFollowModeLookAt(const UnityResolve::UnityType::Vector3& targetPos,
                                                          const UnityResolve::UnityType::Vector3& posOffset,
                                                          const bool recordY = false);
    void reset_camera();
    void on_cam_rawinput_keyboard(int message, int key);
    void on_cam_rawinput_joystick(JoystickEvent event);
	void initCameraSettings();

    struct CameraInfo {
        UnityResolve::UnityType::Vector3 position{0, 0, 0};
        UnityResolve::UnityType::Quaternion rotation{0, 0, 0, 1};
        float fov = 60.0f;
        bool isValid = false;
    };

    extern CameraInfo currentCameraInfo;
    void UpdateCameraInfo(const UnityResolve::UnityType::Vector3& pos, 
                         const UnityResolve::UnityType::Quaternion& rot, 
                         float fieldOfView);
    CameraInfo GetCurrentCameraInfo();
}
