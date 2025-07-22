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
}

namespace LinkuraLocal::HookLiveRender {
    void Install(HookInstaller* hookInstaller);
}

namespace LinkuraLocal::HookStory {
    void Install(HookInstaller* hookInstaller);
}

namespace LinkuraLocal::HookShare {
    void Install(HookInstaller* hookInstaller);
}