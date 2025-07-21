#include <string>
#include "nlohmann/json.hpp"
#include "../Log.h"
#include <thread>
#include <fstream>
#include "../build/linkura_messages.pb.h"

namespace LinkuraLocal::Config {
    bool isConfigInit = false;

    bool dbgMode = false;
    bool enabled = true;
    bool renderHighResolution = true;
    bool fesArchiveUnlockTicket = false;
    bool lazyInit = true;
    bool replaceFont = true;
    bool textTest = false;
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
            GetConfigItem(textTest);
            GetConfigItem(dumpText);
            GetConfigItem(targetFrameRate);
            GetConfigItem(enableFreeCamera);
        }
        catch (std::exception& e) {
            Log::ErrorFmt("LoadConfig error: %s", e.what());
        }
        isConfigInit = true;
    }
    
    void UpdateConfig(const linkura::ipc::ConfigUpdate& configUpdate) {
        try {
            Log::InfoFmt("Applying config update: type=%d", static_cast<int>(configUpdate.update_type()));
            
            if (configUpdate.update_type() == linkura::ipc::ConfigUpdateType::FULL_UPDATE) {
                // Full configuration update - apply all provided fields
                if (configUpdate.has_dbg_mode()) dbgMode = configUpdate.dbg_mode();
                if (configUpdate.has_enabled()) enabled = configUpdate.enabled();
                if (configUpdate.has_render_high_resolution()) renderHighResolution = configUpdate.render_high_resolution();
                if (configUpdate.has_fes_archive_unlock_ticket()) fesArchiveUnlockTicket = configUpdate.fes_archive_unlock_ticket();
                if (configUpdate.has_lazy_init()) lazyInit = configUpdate.lazy_init();
                if (configUpdate.has_replace_font()) replaceFont = configUpdate.replace_font();
                if (configUpdate.has_text_test()) textTest = configUpdate.text_test();
                if (configUpdate.has_dump_text()) dumpText = configUpdate.dump_text();
                if (configUpdate.has_enable_free_camera()) enableFreeCamera = configUpdate.enable_free_camera();
                if (configUpdate.has_target_frame_rate()) targetFrameRate = configUpdate.target_frame_rate();
            } else if (configUpdate.update_type() == linkura::ipc::ConfigUpdateType::PARTIAL_UPDATE) {
                // Partial update - only apply fields that are explicitly set
                if (configUpdate.has_dbg_mode()) {
                    dbgMode = configUpdate.dbg_mode();
                    Log::InfoFmt("Updated dbgMode: %s", dbgMode ? "true" : "false");
                }
                if (configUpdate.has_enabled()) {
                    enabled = configUpdate.enabled();
                    Log::InfoFmt("Updated enabled: %s", enabled ? "true" : "false");
                }
                if (configUpdate.has_render_high_resolution()) {
                    renderHighResolution = configUpdate.render_high_resolution();
                    Log::InfoFmt("Updated renderHighResolution: %s", renderHighResolution ? "true" : "false");
                }
                if (configUpdate.has_enable_free_camera()) {
                    enableFreeCamera = configUpdate.enable_free_camera();
                    Log::InfoFmt("Updated enableFreeCamera: %s", enableFreeCamera ? "true" : "false");
                }
                if (configUpdate.has_target_frame_rate()) {
                    targetFrameRate = configUpdate.target_frame_rate();
                    Log::InfoFmt("Updated targetFrameRate: %d", targetFrameRate);
                }
                // Add more fields as needed for partial updates
            }
            
        } catch (const std::exception& e) {
            Log::ErrorFmt("UpdateConfig error: %s", e.what());
        }
    }
}
