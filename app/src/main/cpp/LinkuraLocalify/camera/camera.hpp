#pragma once
#include "baseCamera.hpp"
#include "../../deps/Joystick/JoystickEvent.h"

namespace L4Camera {
    enum class CameraMode {
        FREE,
        FIRST_PERSON,
        FOLLOW
    };

    enum class CameraSceneType {
        NONE,
        WITH_LIVE,
        FES_LIVE,
        STORY
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
    void SetCameraSceneType(CameraSceneType type);
    CameraSceneType GetCameraSceneType();

    extern BaseCamera::Camera baseCamera;
    extern UnityResolve::UnityType::Vector3 firstPersonPosOffset;
    extern UnityResolve::UnityType::Vector3 followPosOffset;
    extern int followCharaIndex;
    extern LinkuraLocal::Misc::CSEnum bodyPartsEnum;

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
}
