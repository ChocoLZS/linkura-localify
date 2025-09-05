#pragma once

#include "nlohmann/json.hpp"
#include <unordered_map>
#include "version_compatibility.h"

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
    extern std::string assetsUrlPrefix;
    extern bool removeCharacterShadow;
    /**
     * Only show the archive with motion captures
     */
    extern bool filterMotionCaptureReplay;
    /**
     * Only show playable archive
     */
    extern bool filterPlayableMotionCapture;
    extern bool avoidAccidentalTouch;

    extern std::unordered_map<std::string, nlohmann::json> archiveConfigMap;
    extern VersionCompatibility::Version currentClientVersion;
    extern std::string currentResVersion;
    extern VersionCompatibility::Version latestClientVersion;
    extern std::string latestResVersion;

    void LoadConfig(const std::string& configStr);
    void LoadArchiveConfig(const std::string& configStr);
    void SaveConfig(const std::string& configPath);
    void UpdateConfig(const linkura::ipc::ConfigUpdate& configUpdate);

    // Legacy version comparison functions (keep for compatibility)
    std::vector<int> parseVersion(const std::string& version);
    int compareVersions(const std::string& version1, const std::string& version2);
    
    // New enhanced version compatibility functions
    bool checkVersionCompatibility(const std::string& rule, const std::string& version);
    bool isVersionInRange(const std::string& version, const std::string& minVersion, const std::string& maxVersion);
    std::string getVersionRuleDescription(const std::string& rule);
    std::string getRecommendVersion(const std::string& rule);
    
    // Version check functions (now using enhanced compatibility checker)
    bool isLegacyMrsVersion();
    bool isFirstYearVersion();
}
