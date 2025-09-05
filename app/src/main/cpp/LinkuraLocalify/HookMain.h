#pragma once

#include <set>
#include <unordered_set>
#include "../platformDefine.hpp"
#include "config/Config.hpp"
#include <nlohmann/json.hpp>
#include "Log.h"
#include "Plugin.h"
#include "UnityResolve/UnityResolve.hpp"
#include "Il2cppUtils.hpp"
#include "JniBridge.h"


extern std::unordered_set<void*> hookedStubs;

extern bool IsNativeObjectAlive(void* obj);

namespace LinkuraLocal::HookDebug {
    void Install(HookInstaller* hookInstaller);
}

#include <vector>
#include <cstdint>

namespace LinkuraLocal::HookCamera {
    void Install(HookInstaller* hookInstaller);
    std::vector<uint8_t> getCameraInfoProtobuf();

    void unregisterMainFreeCamera(bool cleanup);
    void unregisterCurrentCamera();
    void setCameraBackgroundColor(float red, float green, float blue, float alpha);
}

namespace LinkuraLocal::HookLiveRender {
    enum struct LiveScreenOrientation {
        Landscape,
        Portrait
    };
    void Install(HookInstaller* hookInstaller);
    std::vector<uint8_t> getCurrentArchiveInfo();
    void setArchivePosition(float seconds);
    void applyCameraGraphicSettings(UnityResolve::UnityType::Camera* mainCamera);
    void applyRenderTextureGraphicSettings(void* renderTexture);
}

namespace LinkuraLocal::HookStory {
    void Install(HookInstaller* hookInstaller);
}

namespace LinkuraLocal::HookShare {
    void Install(HookInstaller* hookInstaller);
    std::string replaceUriHost(const std::string& external_link, const std::string& assets_url);
    namespace Shareable {
        struct ArchiveData {
            std::string id;
            long long duration;
        };
        enum RenderScene {
            None,
            FesLive,
            WithLive,
            Story
        };
        enum SetPlayPosition_State {
            Nothing,
            UpdateReceived
        };
        extern std::unordered_map<std::string, ArchiveData> archiveData;
        extern void* realtimeRenderingArchiveControllerCache;
        extern float realtimeRenderingArchivePositionSeconds;
        extern std::string currentArchiveId;
        extern float currentArchiveDuration;
        extern RenderScene renderScene;
        extern SetPlayPosition_State setPlayPositionState;

        // Function declarations (implementations in HookShare.cpp)
        void resetRenderScene();
        bool renderSceneIsNone();
        bool renderSceneIsFesLive();
        bool renderSceneIsWithLive();
        bool renderSceneIsStory();
        
        // Template function for better type safety and performance
        template<RenderScene scene>
        constexpr bool isRenderScene() {
            return renderScene == scene;
        }
    }
}