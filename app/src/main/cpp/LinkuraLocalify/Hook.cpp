#include "Hook.h"
#include "HookMain.h"
#include "Local.h"
#include "MasterLocal.h"
#include "camera/camera.hpp"

// #include <jni.h>
#include <thread>
#include <map>
#include <set>
#include <string_view>
#include <locale>
#include <codecvt>
#include <chrono>
#include <future>
#include "linkura_messages.pb.h"


std::unordered_set<void*> hookedStubs{};
extern std::filesystem::path linkuraLocalPath;

bool IsNativeObjectAlive(void* obj) {
    if (!obj) return false;
    static UnityResolve::Method* IsNativeObjectAliveMtd = nullptr;
    if (!IsNativeObjectAliveMtd) IsNativeObjectAliveMtd = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                                 "Object", "IsNativeObjectAlive");
    return IsNativeObjectAliveMtd->Invoke<bool>(obj);
}

/*
void UnHookAll() {
    for (const auto i: hookedStubs) {
        int result = shadowhook_unhook(i);
        if(result != 0)
        {
            int error_num = shadowhook_get_errno();
            const char *error_msg = shadowhook_to_errmsg(error_num);
            LinkuraLocal::Log::ErrorFmt("unhook failed: %d - %s", error_num, error_msg);
        }
    }
}*/

namespace LinkuraLocal::HookMain {
    using Il2cppString = UnityResolve::UnityType::String;
    UnityResolve::UnityType::String* environment_get_stacktrace() {
        /*
        static auto mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System",
                                                 "Environment", "get_StackTrace");
        return mtd->Invoke<UnityResolve::UnityType::String*>();*/
        const auto pClass = Il2cppUtils::GetClass("mscorlib.dll", "System.Diagnostics",
                                                  "StackTrace");

        const auto ctor_mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System.Diagnostics",
                                                     "StackTrace", ".ctor");
        const auto toString_mtd = Il2cppUtils::GetMethod("mscorlib.dll", "System.Diagnostics",
                                                         "StackTrace", "ToString");

        const auto klassInstance = pClass->New<void*>();
        ctor_mtd->Invoke<void>(klassInstance);
        return toString_mtd->Invoke<Il2cppString*>(klassInstance);
    }

    void StartInjectFunctions() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();
        // problem here
        auto hmodule = xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_LAZY);
        UnityResolve::Init(hmodule, UnityResolve::Mode::Il2Cpp, Config::lazyInit);
//        UnityResolve::Init(xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_LAZY),
//            UnityResolve::Mode::Il2Cpp, Config::lazyInit);

        HookDebug::Install(hookInstaller);
        HookLiveRender::Install(hookInstaller);
        HookCamera::Install(hookInstaller);
        HookShare::Install(hookInstaller);
    }
    // 77 2640 5000

    DEFINE_HOOK(int, il2cpp_init, (const char* domain_name)) {
#ifndef GKMS_WINDOWS
        const auto ret = il2cpp_init_Orig(domain_name);
#else
        //        const auto ret = 0;
#endif
        // InjectFunctions();

        Log::Info("Waiting for config...");

        while (!Config::isConfigInit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!Config::enabled) {
            Log::Info("Plugin not enabled");
            return ret;
        }

        Log::Info("Start init plugin...");

        if (Config::lazyInit) {
            UnityResolveProgress::startInit = true;
            UnityResolveProgress::assembliesProgress.total = 2;
            UnityResolveProgress::assembliesProgress.current = 1;
            UnityResolveProgress::classProgress.total = 36;
            UnityResolveProgress::classProgress.current = 0;
        }

        StartInjectFunctions();
        L4Camera::initCameraSettings(); // free camera

        if (Config::lazyInit) {
            UnityResolveProgress::assembliesProgress.current = 2;
            UnityResolveProgress::classProgress.total = 1;
            UnityResolveProgress::classProgress.current = 0;
        }

        UnityResolveProgress::startInit = false;

        Log::Info("Plugin init finished.");
        return ret;
    }
}


namespace LinkuraLocal::Hook {
    void Install() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();
#ifndef GKMS_WINDOWS
        ADD_HOOK(HookMain::il2cpp_init,
                 Plugin::GetInstance().GetHookInstaller()->LookupSymbol("il2cpp_init"));
#else
        HookMain::il2cpp_init_Hook(nullptr);
#endif

        Log::Info("Hook installed");
    }

    std::vector<uint8_t> getCameraInfoProtobuf() {
        return HookCamera::getCameraInfoProtobuf();
    }
}
