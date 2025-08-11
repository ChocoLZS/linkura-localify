#pragma once

#include "nlohmann/json.hpp"
#include <unordered_map>

namespace linkura { namespace ipc { class ConfigUpdate; } }

namespace LinkuraLocal::Config {
    extern bool isConfigInit;

    extern bool dbgMode;
    extern bool enabled;
    extern bool renderHighResolution;
    extern bool fesArchiveUnlockTicket;
    extern bool lazyInit;
    extern bool replaceFont;
    extern bool textTest;
    extern bool dumpText;
    extern bool enableFreeCamera;
    extern int targetFrameRate;
    extern bool removeRenderImageCover;
    extern bool avoidCharacterExit;
    extern bool storyHideBackground;
    extern bool storyHideTransition;
    extern bool storyHideNonCharacter3d;
    extern bool storyHideDof;
    extern bool storyHideEffect;
    extern float storyNovelVocalTextDurationRate;
    extern float storyNovelNonVocalTextDurationRate;
    extern bool firstPersonCameraHideHead;
    extern bool firstPersonCameraHideHair;
    extern bool enableMotionCaptureReplay;
    extern bool enableInGameReplayDisplay;
    extern std::string motionCaptureResourceUrl;
    extern int withliveOrientation;
    extern bool lockRenderTextureResolution;
    extern int renderTextureLongSide;
    extern int renderTextureShortSide;
    extern bool hideCharacterBody;
    extern int renderTextureAntiAliasing;
    extern bool unlockAfter;
    extern float cameraMovementSensitivity;
    extern float cameraVerticalSensitivity;
    extern float cameraFovSensitivity;
    extern float cameraRotationSensitivity;

    extern std::unordered_map<std::string, nlohmann::json> archiveConfigMap;

    void LoadConfig(const std::string& configStr);
    void LoadArchiveConfig(const std::string& configStr);
    void SaveConfig(const std::string& configPath);
    void UpdateConfig(const linkura::ipc::ConfigUpdate& configUpdate);
}
