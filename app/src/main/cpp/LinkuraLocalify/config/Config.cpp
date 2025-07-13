#include <string>
#include "nlohmann/json.hpp"
#include "../Log.h"
#include <thread>
#include <fstream>

namespace LinkuraLocal::Config {
    bool isConfigInit = false;

    bool dbgMode = false;
    bool enabled = true;
    bool renderHighResolution = true;
    bool fesArchiveUnlockTicket = false;
    bool lazyInit = true;
    bool replaceFont = true;
    bool dumpText = false;
    bool enableFreeCamera = false;
    int targetFrameRate = 0;

    void LoadConfig(const std::string& configStr) {
        try {
            const auto config = nlohmann::json::parse(configStr);

            #define GetConfigItem(name) if (config.contains(#name)) name = config[#name]

            GetConfigItem(dbgMode);
            GetConfigItem(enabled);
            GetConfigItem(renderHighResolution);
            GetConfigItem(fesArchiveUnlockTicket);
            GetConfigItem(lazyInit);
            GetConfigItem(replaceFont);
            GetConfigItem(dumpText);
            GetConfigItem(targetFrameRate);
            GetConfigItem(enableFreeCamera);
        }
        catch (std::exception& e) {
            Log::ErrorFmt("LoadConfig error: %s", e.what());
        }
        isConfigInit = true;
    }

    void SaveConfig(const std::string& configPath) {
        try {
            nlohmann::json config;

            #define SetConfigItem(name) config[#name] = name

            SetConfigItem(dbgMode);
            SetConfigItem(enabled);
            SetConfigItem(lazyInit);
            SetConfigItem(renderHighResolution);
            SetConfigItem(fesArchiveUnlockTicket);
            SetConfigItem(replaceFont);
            SetConfigItem(dumpText);
            SetConfigItem(targetFrameRate);
            SetConfigItem(enableFreeCamera);

            std::ofstream out(configPath);
            if (!out) {
                Log::ErrorFmt("SaveConfig error: Cannot open file: %s", configPath.c_str());
                return;
            }
            out << config.dump(4);
			Log::Info("SaveConfig success");
        }
        catch (std::exception& e) {
            Log::ErrorFmt("SaveConfig error: %s", e.what());
        }
    }
}
