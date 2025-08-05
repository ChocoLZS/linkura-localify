#pragma once
#include "baseCamera.hpp"
#include "../../deps/Joystick/JoystickEvent.h"
#include "map"
#include "../Il2cppUtils.hpp"
#include "../config/Config.hpp"

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
    protected:
        std::vector<std::map<std::string, UnityResolve::UnityType::Transform*>> charaMeshes;
        std::map<T, UnityResolve::UnityType::Transform*> charaHairMeshes; // trick for judgement

    public:
        void clear() override {
            charaMeshes.clear();
            charaHairMeshes.clear();
            LinkuraLocal::Misc::IndexedSet<T>::clear();
        }

        void add(void* value)  {
            LinkuraLocal::Misc::IndexedSet<T>::add(value);
            std::map<std::string, UnityResolve::UnityType::Transform*> newMeshMap;
            charaMeshes.push_back(newMeshMap);
        }

        void addCharaMesh(const std::string& key, UnityResolve::UnityType::Transform* mesh) {
            if (LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex() < charaMeshes.size()) {
                std::map<std::string, UnityResolve::UnityType::Transform*>& currentMeshMap = charaMeshes[LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex()];
                currentMeshMap.insert_or_assign(key, mesh);
            }
        }
        void addCharaHair(UnityResolve::UnityType::Transform* hairMesh) {
            auto& current = LinkuraLocal::Misc::IndexedSet<T>::getCurrentValue();
            if (current) {
                charaHairMeshes.insert_or_assign(current, hairMesh);
            }
        }
        UnityResolve::UnityType::Transform* getCurrentHair() {
            auto& current = LinkuraLocal::Misc::IndexedSet<T>::getCurrentValue();
//            LinkuraLocal::Log::DebugFmt("getCurrentHair of index: %d", LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex());
            if (current) {
                return charaHairMeshes[current];
            }
            return nullptr;
        }
        bool containsCharaMesh(const std::string& key) {
            if (LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex() < charaMeshes.size()) {
                auto &currentMeshMap = charaMeshes[LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex()];
                return currentMeshMap.find(key) != currentMeshMap.end();
            }
            return false;
        }

        void setMeshRenderActive(UnityResolve::UnityType::Transform* transform, bool active, std::string str = "") {
            static auto get_component = reinterpret_cast<UnityResolve::UnityType::Component* (*)(UnityResolve::UnityType::GameObject*, void*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.GameObject::GetComponent(System.Type)"));
            static auto get_enabled = reinterpret_cast<bool (*)(UnityResolve::UnityType::Component*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::get_enabled()"));
            static auto set_enabled = reinterpret_cast<void (*)(UnityResolve::UnityType::Component*, bool)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::set_enabled(System.Boolean)"));
            static auto rendererType = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer");
            if (!(transform && Il2cppUtils::IsNativeObjectAlive(transform))) return;
            auto gameObject = transform->GetGameObject();
            if (!(gameObject && Il2cppUtils::IsNativeObjectAlive(gameObject))) return;
            auto renderer = gameObject->GetComponent<UnityResolve::UnityType::Component*>(rendererType);
            if (renderer && Il2cppUtils::IsNativeObjectAlive(renderer)) {
                set_enabled(renderer, active);
            } else {
                LinkuraLocal::Log::DebugFmt("No renderer found for %s", str.c_str());
            }
        }
    };

    template <typename T>
    class CharacterMeshFirstPersonManager : public CharacterMeshManager<T> {
        std::map<T, bool> snapshotRenderedState;
    public:
        bool currentHairIsRendered() {
            static auto get_component = reinterpret_cast<UnityResolve::UnityType::Component* (*)(UnityResolve::UnityType::GameObject*, void*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.GameObject::GetComponent(System.Type)"));
            static auto get_enabled = reinterpret_cast<bool (*)(UnityResolve::UnityType::Component*)>(
                    Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::get_enabled()"));
            static auto rendererType = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer");
            auto& current = LinkuraLocal::Misc::IndexedSet<T>::getCurrentValue();
            auto it = CharacterMeshManager<T>::charaHairMeshes.find(current);
            if (it == CharacterMeshManager<T>::charaHairMeshes.end()) {
                return false;
            }
            UnityResolve::UnityType::Transform* currentHair = it->second;
            if (!(currentHair && Il2cppUtils::IsNativeObjectAlive(currentHair))) return false;
            auto gameObject = currentHair->GetGameObject();
            if (!(gameObject && Il2cppUtils::IsNativeObjectAlive(gameObject))) return false;
            auto renderer = gameObject->GetComponent<UnityResolve::UnityType::Component*>(rendererType);
            if (renderer && Il2cppUtils::IsNativeObjectAlive(renderer)) {
                return get_enabled(renderer);
            }
            return false;
        }
        void setSnapshotRenderState(bool state) {
            auto& current = CharacterMeshManager<T>::getCurrentValue();
            if (current) {
                snapshotRenderedState.insert_or_assign(current, state);
            }
        }
        bool getSnapshotRenderState() {
            auto& current = CharacterMeshManager<T>::getCurrentValue();
            if (current) {
                auto it = snapshotRenderedState.find(current);
                if (it != snapshotRenderedState.end()) {
                    return it->second;
                }
            }
            return false;
        }
        void restoreCurrentCharaMeshes() {
            if (CharacterMeshManager<T>::getCurrentIndex() <
                CharacterMeshManager<T>::charaMeshes.size()) {
                if (!LinkuraLocal::Config::firstPersonCameraHideHead) return;
//                LinkuraLocal::Log::DebugFmt("Restore current chara meshes using Renderer.enabled");
                std::map<std::string, UnityResolve::UnityType::Transform *> &meshMap = CharacterMeshManager<T>::charaMeshes[CharacterMeshManager<T>::getCurrentIndex()];
                auto snapshotRendered = getSnapshotRenderState();
                // LinkuraLocal::Log::DebugFmt("Snapshot rendered: %s", snapshotRendered ? "true" : "false");
                if (!snapshotRendered) return; // if rendered, means it been hidden.
                for (auto &pair: meshMap) {
//                    LinkuraLocal::Log::DebugFmt("Trying to restore renderer for %s", pair.first.c_str());
                    auto transform = pair.second;
                    CharacterMeshManager<T>::setMeshRenderActive(transform, true, pair.first);
                }
                if (LinkuraLocal::Config::firstPersonCameraHideHair) {
                    auto hair = CharacterMeshManager<T>::getCurrentHair();
                    CharacterMeshManager<T>::setMeshRenderActive(hair, true, "hair");
                }
            }
        }
        void hideCurrentCharaMeshes() {
            if (LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex() < CharacterMeshManager<T>::charaMeshes.size()) {
                if (!LinkuraLocal::Config::firstPersonCameraHideHead) return;
//                LinkuraLocal::Log::DebugFmt("Hide current chara meshes using Renderer.enabled");
                std::map<std::string, UnityResolve::UnityType::Transform*>& meshMap = CharacterMeshManager<T>::charaMeshes[LinkuraLocal::Misc::IndexedSet<T>::getCurrentIndex()];
                auto currentIsRendered = currentHairIsRendered();
                setSnapshotRenderState(currentIsRendered);
                // LinkuraLocal::Log::DebugFmt("Current hair is rendered: %s", currentIsRendered ? "true" : "false");
                if (!currentIsRendered) return; // if current hair is not rendered, do not hide
                for (auto& pair : meshMap) {
                    auto transform = pair.second;
                    CharacterMeshManager<T>::setMeshRenderActive(transform, false, pair.first);
                }
                if (LinkuraLocal::Config::firstPersonCameraHideHair) {
                    auto hair = CharacterMeshManager<T>::getCurrentHair();
                    CharacterMeshManager<T>::setMeshRenderActive(hair, false, "hair");
                }
            }
        }
        void onCameraModeChange(L4Camera::CameraMode cameraMode) {
            if (cameraMode != L4Camera::CameraMode::FIRST_PERSON) {
                LinkuraLocal::Log::DebugFmt("Trying to restore the meshes due to camera mode is not first_person");
                restoreCurrentCharaMeshes();
            }
        }
        void clear() override {
            snapshotRenderedState.clear();
            CharacterMeshManager<T>::clear();
        }
        void next() override {
            if (L4Camera::GetCameraMode() == L4Camera::CameraMode::FIRST_PERSON) {
                restoreCurrentCharaMeshes();
                LinkuraLocal::Misc::IndexedSet<T>::next();
            } else {
                LinkuraLocal::Misc::IndexedSet<T>::next();
            }
        }

        void prev() override {
            if (L4Camera::GetCameraMode() == L4Camera::CameraMode::FIRST_PERSON) {
                restoreCurrentCharaMeshes();
                LinkuraLocal::Misc::IndexedSet<T>::prev();
            } else {
                LinkuraLocal::Misc::IndexedSet<T>::prev();
            }
        }

    };

    extern BaseCamera::Camera baseCamera;
    extern BaseCamera::Camera originCamera;
    extern UnityResolve::UnityType::Vector3 firstPersonPosOffset;
    extern UnityResolve::UnityType::Vector3 followPosOffset;
    extern CharacterMeshFirstPersonManager<void*> followCharaSet;
    extern CharacterMeshManager<void*> charaRenderSet;

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
    void clearRenderSet();

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
