#ifndef LINKURA_LOCALIFY_PLUGIN_H
#define LINKURA_LOCALIFY_PLUGIN_H

#include "Misc.hpp"
#include <string>
#include <memory>

#include "../platformDefine.hpp"

#ifndef GKMS_WINDOWS
    #include <jni.h>
#endif // !GKMS_WINDOWS


namespace LinkuraLocal {
    struct HookInstaller
    {
        virtual ~HookInstaller();
        virtual void* InstallHook(void* addr, void* hook, void** orig) = 0;
        virtual OpaqueFunctionPointer LookupSymbol(const char* name) = 0;

        std::string m_il2cppLibraryPath;
        std::string localizationFilesDir;
    };

    class Plugin
    {
    public:
        static Plugin& GetInstance();

        void InstallHook(std::unique_ptr<HookInstaller>&& hookInstaller);

        HookInstaller* GetHookInstaller() const;

        Plugin(Plugin const&) = delete;
        Plugin& operator=(Plugin const&) = delete;

    private:
        Plugin() = default;

        std::unique_ptr<HookInstaller> m_HookInstaller;
    };

}

#endif //LINKURA_LOCALIFY_PLUGIN_H
