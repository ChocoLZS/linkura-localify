#pragma once

namespace LinkuraLocal::Config {
    extern bool isConfigInit;

    extern bool dbgMode;
    extern bool enabled;
    extern bool renderHighResolution;
    extern bool fesArchiveUnlockTicket;
    extern bool lazyInit;
    extern bool replaceFont;
    extern bool dumpText;
    extern bool enableFreeCamera;
    extern int targetFrameRate;

    void LoadConfig(const std::string& configStr);
    void SaveConfig(const std::string& configPath);
}
