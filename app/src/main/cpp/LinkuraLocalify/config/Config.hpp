#pragma once

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
    extern float storyNovelVocalTextDurationRate;
    extern float storyNovelNonVocalTextDurationRate;
    extern bool firstPersonCameraHideHead;
    extern bool firstPersonCameraHideHair;

    void LoadConfig(const std::string& configStr);
    void SaveConfig(const std::string& configPath);
    void UpdateConfig(const linkura::ipc::ConfigUpdate& configUpdate);
}
