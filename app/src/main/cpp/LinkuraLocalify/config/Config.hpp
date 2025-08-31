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
    extern bool enableLegacyCompatibility;
    extern bool enableSetArchiveStartTime;
    extern int archiveStartTime;
    /**
     * Only show the archive with motion captures
     */
    extern bool filterMotionCaptureReplay;
    /**
     * Only show playable archive
     */
    extern bool filterPlayableMotionCapture;

    extern std::unordered_map<std::string, nlohmann::json> archiveConfigMap;
    extern std::string currentClientVersion;
    extern std::string currentResVersion;
    extern std::string latestClientVersion;
    extern std::string latestResVersion;

    void LoadConfig(const std::string& configStr);
    void LoadArchiveConfig(const std::string& configStr);
    void SaveConfig(const std::string& configPath);
    void UpdateConfig(const linkura::ipc::ConfigUpdate& configUpdate);

    bool isLegacyMrsVersion();
    bool isFirstYearVersion();
}
