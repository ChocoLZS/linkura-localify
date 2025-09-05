#include "../HookMain.h"

namespace LinkuraLocal::HookDebug {
    using Il2cppString = UnityResolve::UnityType::String;

    DEFINE_HOOK(void, Internal_LogException, (void* ex, void* obj)) {
        Internal_LogException_Orig(ex, obj);
        static auto Exception_ToString = Il2cppUtils::GetMethod("mscorlib.dll", "System", "Exception", "ToString");
        Log::LogUnityLog(ANDROID_LOG_ERROR, "UnityLog - Internal_LogException:\n%s", Exception_ToString->Invoke<Il2cppString*>(ex)->ToString().c_str());
    }

    DEFINE_HOOK(void, Internal_Log, (int logType, int logOption, UnityResolve::UnityType::String* content, void* context)) {
        Internal_Log_Orig(logType, logOption, content, context);
        // 2022.3.21f1
        Log::LogUnityLog(ANDROID_LOG_VERBOSE, "Internal_Log:\n%s", content->ToString().c_str());
    }

    // 👀
    DEFINE_HOOK(void, CoverImageCommandReceiver_Awake, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("CoverImageCommandReceiver_Awake HOOKED");
        if (Config::removeRenderImageCover) return;
        CoverImageCommandReceiver_Awake_Orig(self, method);
    }
    // 👀　work for both als and mrs
    DEFINE_HOOK(void, CharacterVisibleReceiver_SetupExistCharacter, (Il2cppUtils::Il2CppObject* self,int character, void* method)) {
        Log::DebugFmt("CharacterVisibleReceiver_SetupExistCharacter HOOKED");
        if (Config::avoidCharacterExit) return;
        CharacterVisibleReceiver_SetupExistCharacter_Orig(self, character, method);
    }
    // old Config::enableLegacyCompatibility
    DEFINE_HOOK(void, CharacterVisibleReceiver_UpdateAvatarVisibility, (Il2cppUtils::Il2CppObject* self, bool isVisible, void* method)) {
        Log::DebugFmt("CharacterVisibleReceiver_UpdateAvatarVisibility HOOKED");
        if (Config::avoidCharacterExit) isVisible = true;
        CharacterVisibleReceiver_UpdateAvatarVisibility_Orig(self, isVisible, method);
    }

    DEFINE_HOOK(void, FootShadowManipulator_CreateFootShadow, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("FootShadowManipulator_CreateFootShadow HOOKED");
        if (Config::removeCharacterShadow) return;
        FootShadowManipulator_CreateFootShadow_Orig(self, method);
    }

    DEFINE_HOOK(void, ItemManipulator_OnInstantiate, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("ItemManipulator_OnInstantiate HOOKED, item id is");
        ItemManipulator_OnInstantiate_Orig(self, method);
    }

    // old Config::enableLegacyCompatibility
    DEFINE_HOOK(void, MRS_AppsCoverScreen_SetActiveCoverImage, (Il2cppUtils::Il2CppObject* self, bool isActive, void* method)) {
        Log::DebugFmt("AppsCoverScreen_SetActiveCoverImage HOOKED");
        if (Config::removeRenderImageCover) isActive = false;
        MRS_AppsCoverScreen_SetActiveCoverImage_Orig(self, isActive, method);
    }

    DEFINE_HOOK(Il2cppString*, Hailstorm_AssetDownloadJob_get_UrlBase, (Il2cppUtils::Il2CppObject* self, void* method)) {
        auto base = Hailstorm_AssetDownloadJob_get_UrlBase_Orig(self, method);
        if (!Config::assetsUrlPrefix.empty()) {
            base = Il2cppString::New(HookShare::replaceUriHost(base->ToString(), Config::assetsUrlPrefix));
        }
        return base;
    }

    void Install(HookInstaller* hookInstaller) {
        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));
        
        // 👀
        ADD_HOOK(CoverImageCommandReceiver_Awake, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CoverImageCommandReceiver", "Awake"));
        ADD_HOOK(CharacterVisibleReceiver_SetupExistCharacter, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character", "CharacterVisibleReceiver", "SetupExistCharacter"));
        ADD_HOOK(FootShadowManipulator_CreateFootShadow, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.FootShadow", "FootShadowManipulator", "CreateFootShadow"));
        ADD_HOOK(ItemManipulator_OnInstantiate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.Item", "ItemManipulator", "OnInstantiate"));
        // 👀 old
        ADD_HOOK(MRS_AppsCoverScreen_SetActiveCoverImage, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.LiveMain", "AppsCoverScreen", "SetActiveCoverImage"));
        ADD_HOOK(CharacterVisibleReceiver_UpdateAvatarVisibility, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.Character", "CharacterVisibleReceiver", "UpdateAvatarVisibility"));
        ADD_HOOK(Hailstorm_AssetDownloadJob_get_UrlBase, Il2cppUtils::GetMethodPointer("Core.dll", "Hailstorm", "AssetDownloadJob", "get_UrlBase"));
    }
}