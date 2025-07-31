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
    bool removeRenderImageCover = false;
    bool avoidCharacterExit = false;
    bool storyHideBackground = false;
    bool storyHideTransition = false;
    bool storyHideNonCharacter3d = false;
    bool storyHideDof = false;

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
            GetConfigItem(removeRenderImageCover);
            GetConfigItem(avoidCharacterExit);
            GetConfigItem(storyHideBackground);
            GetConfigItem(storyHideTransition);
            GetConfigItem(storyHideNonCharacter3d);
            GetConfigItem(storyHideDof);
        }
        catch (std::exception& e) {
            Log::ErrorFmt("LoadConfig error: %s", e.what());
        }
        isConfigInit = true;
    }
    
    void UpdateConfig(const linkura::ipc::ConfigUpdate& configUpdate) {
        try {
            Log::InfoFmt("Applying config update: type=%d", static_cast<int>(configUpdate.update_type()));
            // only allow hot reload config update
            if (configUpdate.update_type() == linkura::ipc::ConfigUpdateType::FULL_UPDATE) {
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
                if (configUpdate.has_remove_render_image_cover()) removeRenderImageCover = configUpdate.remove_render_image_cover();
                if (configUpdate.has_avoid_character_exit()) avoidCharacterExit = configUpdate.avoid_character_exit();
                if (configUpdate.has_story_hide_background()) storyHideBackground = configUpdate.story_hide_background();
                if (configUpdate.has_story_hide_transition()) storyHideTransition = configUpdate.story_hide_transition();
                if (configUpdate.has_story_hide_non_character_3d()) storyHideNonCharacter3d = configUpdate.story_hide_non_character_3d();
                if (configUpdate.has_story_hide_dof()) storyHideDof = configUpdate.story_hide_dof();
            }
        } catch (const std::exception& e) {
            Log::ErrorFmt("UpdateConfig error: %s", e.what());
        }
    }
}
