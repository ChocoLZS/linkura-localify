#include "Hook.h"
#include "Plugin.h"
#include "Log.h"
#include "../deps/UnityResolve/UnityResolve.hpp"
#include "Il2cppUtils.hpp"
#include "Local.h"
#include "MasterLocal.h"
#include <unordered_set>
#include "camera/camera.hpp"
#include "config/Config.hpp"
// #include <jni.h>
#include <thread>
#include <map>
#include <set>
#include "../platformDefine.hpp"

#ifdef GKMS_WINDOWS
    #include "../windowsPlatform.hpp"
    #include "cpprest/details/http_helpers.h"
    #include "../resourceUpdate/resourceUpdate.hpp"
#endif


std::unordered_set<void*> hookedStubs{};
extern std::filesystem::path gakumasLocalPath;

#define DEFINE_HOOK(returnType, name, params)                                                      \
	using name##_Type = returnType(*) params;                                                      \
	name##_Type name##_Addr = nullptr;                                                             \
	name##_Type name##_Orig = nullptr;                                                             \
	returnType name##_Hook params

/*
void UnHookAll() {
    for (const auto i: hookedStubs) {
        int result = shadowhook_unhook(i);
        if(result != 0)
        {
            int error_num = shadowhook_get_errno();
            const char *error_msg = shadowhook_to_errmsg(error_num);
            GakumasLocal::Log::ErrorFmt("unhook failed: %d - %s", error_num, error_msg);
        }
    }
}*/

namespace GakumasLocal::HookMain {
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

    bool IsNativeObjectAlive(void* obj) {
        static UnityResolve::Method* IsNativeObjectAliveMtd = nullptr;
        if (!IsNativeObjectAliveMtd) IsNativeObjectAliveMtd = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                                     "Object", "IsNativeObjectAlive");
        return IsNativeObjectAliveMtd->Invoke<bool>(obj);
    }

    UnityResolve::UnityType::Camera* mainCameraCache = nullptr;
    UnityResolve::UnityType::Transform* cameraTransformCache = nullptr;
    void CheckAndUpdateMainCamera() {
        if (!Config::enableFreeCamera) return;
        if (IsNativeObjectAlive(mainCameraCache) && IsNativeObjectAlive(cameraTransformCache)) return;

        mainCameraCache = UnityResolve::UnityType::Camera::GetMain();
        cameraTransformCache = mainCameraCache->GetTransform();
    }

    Il2cppUtils::Resolution_t GetResolution() {
        static auto GetResolution = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                           "Screen", "get_currentResolution");
        return GetResolution->Invoke<Il2cppUtils::Resolution_t>();
    }

    Il2cppString* ToJsonStr(void* object) {
        static Il2cppString* (*toJsonStr)(void*) = nullptr;
		if (!toJsonStr) {
			toJsonStr = reinterpret_cast<Il2cppString * (*)(void*)>(Il2cppUtils::GetMethodPointer("Newtonsoft.Json.dll", "Newtonsoft.Json",
                "JsonConvert", "SerializeObject", { "*" }));
        }
        if (!toJsonStr) {
			return nullptr;
        }
		return toJsonStr(object);
    }

    DEFINE_HOOK(void, Unity_set_fieldOfView, (UnityResolve::UnityType::Camera* self, float value)) {
        if (Config::enableFreeCamera) {
            if (self == mainCameraCache) {
                value = GKCamera::baseCamera.fov;
            }
        }
        Unity_set_fieldOfView_Orig(self, value);
    }

    DEFINE_HOOK(float, Unity_get_fieldOfView, (UnityResolve::UnityType::Camera* self)) {
        if (Config::enableFreeCamera) {
            if (self == mainCameraCache) {
                static auto get_orthographic = reinterpret_cast<bool (*)(void*)>(Il2cppUtils::il2cpp_resolve_icall(
                        "UnityEngine.Camera::get_orthographic()"
                ));
                static auto set_orthographic = reinterpret_cast<bool (*)(void*, bool)>(Il2cppUtils::il2cpp_resolve_icall(
                        "UnityEngine.Camera::set_orthographic(System.Boolean)"
                ));

                for (const auto& i : UnityResolve::UnityType::Camera::GetAllCamera()) {
                    // Log::DebugFmt("get_orthographic: %d", get_orthographic(i));
                    // set_orthographic(i, false);
                    Unity_set_fieldOfView_Orig(i, GKCamera::baseCamera.fov);
                }
                Unity_set_fieldOfView_Orig(self, GKCamera::baseCamera.fov);

                // Log::DebugFmt("main - get_orthographic: %d", get_orthographic(self));
                return GKCamera::baseCamera.fov;
            }
        }
        return Unity_get_fieldOfView_Orig(self);
    }

    UnityResolve::UnityType::Transform* cacheTrans = nullptr;
    UnityResolve::UnityType::Quaternion cacheRotation{};
    UnityResolve::UnityType::Vector3 cachePosition{};
    UnityResolve::UnityType::Vector3 cacheForward{};
    UnityResolve::UnityType::Vector3 cacheLookAt{};

    DEFINE_HOOK(void, Unity_set_rotation_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Quaternion* value)) {
        if (Config::enableFreeCamera) {
            static auto lookat_injected = reinterpret_cast<void (*)(void*self,
                                                                    UnityResolve::UnityType::Vector3* worldPosition, UnityResolve::UnityType::Vector3* worldUp)>(
                    Il2cppUtils::il2cpp_resolve_icall(
                            "UnityEngine.Transform::Internal_LookAt_Injected(UnityEngine.Vector3&,UnityEngine.Vector3&)"));
            static auto worldUp = UnityResolve::UnityType::Vector3(0, 1, 0);

            if (cameraTransformCache == self) {
                const auto cameraMode = GKCamera::GetCameraMode();
                if (cameraMode == GKCamera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        if (GKCamera::GetFirstPersonRoll() == GKCamera::FirstPersonRoll::ENABLE_ROLL) {
                            *value = cacheRotation;
                        }
                        else {
                            static GakumasLocal::Misc::FixedSizeQueue<float> recordsY(60);
                            const auto newY = GKCamera::CheckNewY(cacheLookAt, true, recordsY);
                            UnityResolve::UnityType::Vector3 newCacheLookAt{cacheLookAt.x, newY, cacheLookAt.z};
                            lookat_injected(self, &newCacheLookAt, &worldUp);
                            return;
                        }
                    }
                }
                else if (cameraMode == GKCamera::CameraMode::FOLLOW) {
                    auto newLookAtPos = GKCamera::CalcFollowModeLookAt(cachePosition,
                                                                       GKCamera::followPosOffset, true);
                    lookat_injected(self, &newLookAtPos, &worldUp);
                    return;
                }
                else {
                    auto& origCameraLookat = GKCamera::baseCamera.lookAt;
                    lookat_injected(self, &origCameraLookat, &worldUp);
                    // Log::DebugFmt("fov: %f, target: %f", Unity_get_fieldOfView_Orig(mainCameraCache), GKCamera::baseCamera.fov);
                    return;
                }
            }
        }
        return Unity_set_rotation_Injected_Orig(self, value);
    }

    DEFINE_HOOK(void, Unity_set_position_Injected, (UnityResolve::UnityType::Transform* self, UnityResolve::UnityType::Vector3* data)) {
        if (Config::enableFreeCamera) {
            CheckAndUpdateMainCamera();

            if (cameraTransformCache == self) {
                const auto cameraMode = GKCamera::GetCameraMode();
                if (cameraMode == GKCamera::CameraMode::FIRST_PERSON) {
                    if (cacheTrans && IsNativeObjectAlive(cacheTrans)) {
                        *data = GKCamera::CalcFirstPersonPosition(cachePosition, cacheForward, GKCamera::firstPersonPosOffset);
                    }

                }
                else if (cameraMode == GKCamera::CameraMode::FOLLOW) {
                    auto newLookAtPos = GKCamera::CalcFollowModeLookAt(cachePosition, GKCamera::followPosOffset);
                    auto pos = GKCamera::CalcPositionFromLookAt(newLookAtPos, GKCamera::followPosOffset);
                    data->x = pos.x;
                    data->y = pos.y;
                    data->z = pos.z;
                }
                else {
                    //Log::DebugFmt("MainCamera set pos: %f, %f, %f", data->x, data->y, data->z);
                    auto& origCameraPos = GKCamera::baseCamera.pos;
                    data->x = origCameraPos.x;
                    data->y = origCameraPos.y;
                    data->z = origCameraPos.z;
                }
            }
        }

        return Unity_set_position_Injected_Orig(self, data);
    }

#ifdef GKMS_WINDOWS
    DEFINE_HOOK(void*, InternalSetOrientationAsync, (void* retstr, void* self, int type, void* c, void* tc, void* mtd)) {
        switch (Config::gameOrientation) {
        case 1: type = 0x2; break;  // FixedPortrait
        case 2: type = 0x3; break;  // FixedLandscape
        default: break;
        }
        return InternalSetOrientationAsync_Orig(retstr, self, type, c, tc, mtd);
    }
#else
    DEFINE_HOOK(void*, InternalSetOrientationAsync, (void* self, int type, void* c, void* tc, void* mtd)) {
        switch (Config::gameOrientation) {
        case 1: type = 0x2; break;  // FixedPortrait
        case 2: type = 0x3; break;  // FixedLandscape
        default: break;
        }
        return InternalSetOrientationAsync_Orig(self, type, c, tc, mtd);
    }
#endif

    DEFINE_HOOK(void, EndCameraRendering, (void* ctx, void* camera, void* method)) {
        EndCameraRendering_Orig(ctx, camera, method);

        if (Config::enableFreeCamera && mainCameraCache) {
            Unity_set_fieldOfView_Orig(mainCameraCache, GKCamera::baseCamera.fov);
            if (GKCamera::GetCameraMode() == GKCamera::CameraMode::FIRST_PERSON) {
                mainCameraCache->SetNearClipPlane(0.001f);
            }
        }
    }

    DEFINE_HOOK(void, Unity_set_targetFrameRate, (int value)) {
        const auto configFps = Config::targetFrameRate;
        return Unity_set_targetFrameRate_Orig(configFps == 0 ? value: configFps);
    }

    std::unordered_map<void*, std::string> loadHistory{};

    DEFINE_HOOK(void*, AssetBundle_LoadAssetAsync, (void* self, Il2cppString* name, void* type)) {
        // Log::InfoFmt("AssetBundle_LoadAssetAsync: %s, type: %s", name->ToString().c_str());
        auto ret = AssetBundle_LoadAssetAsync_Orig(self, name, type);
        loadHistory.emplace(ret, name->ToString());
        return ret;
    }

    DEFINE_HOOK(void*, AssetBundleRequest_GetResult, (void* self)) {
        auto result = AssetBundleRequest_GetResult_Orig(self);
        if (const auto iter = loadHistory.find(self); iter != loadHistory.end()) {
            const auto name = iter->second;
            loadHistory.erase(iter);

            // const auto assetClass = Il2cppUtils::get_class_from_instance(result);
            // Log::InfoFmt("AssetBundleRequest_GetResult: %s, type: %s", name.c_str(), static_cast<Il2CppClassHead*>(assetClass)->name);
        }
        return result;
    }

    DEFINE_HOOK(void*, Resources_Load, (Il2cppString* path, void* systemTypeInstance)) {
        auto ret = Resources_Load_Orig(path, systemTypeInstance);

        // if (ret) Log::DebugFmt("Resources_Load: %s, type: %s", path->ToString().c_str(), Il2cppUtils::get_class_from_instance(ret)->name);

        return ret;
    }

    DEFINE_HOOK(void, I18nHelper_SetUpI18n, (void* self, Il2cppString* lang, Il2cppString* localizationText, int keyComparison)) {
        // Log::InfoFmt("SetUpI18n lang: %s, key: %d text: %s", lang->ToString().c_str(), keyComparison, localizationText->ToString().c_str());
        // TODO 此处为 dump 原文 csv
        I18nHelper_SetUpI18n_Orig(self, lang, localizationText, keyComparison);
    }

    DEFINE_HOOK(void, I18nHelper_SetValue, (void* self, Il2cppString* key, Il2cppString* value)) {
        // Log::InfoFmt("I18nHelper_SetValue: %s - %s", key->ToString().c_str(), value->ToString().c_str());
        std::string local;
        if (Local::GetI18n(key->ToString(), &local)) {
            I18nHelper_SetValue_Orig(self, key, UnityResolve::UnityType::String::New(local));
            return;
        }
        Local::DumpI18nItem(key->ToString(), value->ToString());
        if (Config::textTest) {
            I18nHelper_SetValue_Orig(self, key, Il2cppString::New("[I18]" + value->ToString()));
        }
        else {
            I18nHelper_SetValue_Orig(self, key, value);
        }
    }

    
#ifdef GKMS_WINDOWS
    struct TransparentStringHash : std::hash<std::wstring>, std::hash<std::wstring_view>
    {
        using is_transparent = void;
    };

    typedef std::unordered_set<std::wstring, TransparentStringHash, std::equal_to<void>> AssetPathsType;
    std::map<std::string, AssetPathsType> CustomAssetBundleAssetPaths;
    std::unordered_map<std::string, uint32_t> CustomAssetBundleHandleMap{};
    std::list<std::string> g_extra_assetbundle_paths{};

    void LoadExtraAssetBundle() {
        using Il2CppString = UnityResolve::UnityType::String;

        if (g_extra_assetbundle_paths.empty()) {
            return;
        }
        // CustomAssetBundleHandleMap.clear();
        // CustomAssetBundleAssetPaths.clear();
        // assert(!ExtraAssetBundleHandle && ExtraAssetBundleAssetPaths.empty());

        static auto AssetBundle_GetAllAssetNames = reinterpret_cast<void* (*)(void*)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.AssetBundle::GetAllAssetNames()")
            );

        for (const auto& i : g_extra_assetbundle_paths) {
            if (CustomAssetBundleHandleMap.contains(i)) continue;

            const auto extraAssetBundle = WinHooks::LoadAssetBundle(i);
            if (extraAssetBundle)
            {
                const auto allAssetPaths = AssetBundle_GetAllAssetNames(extraAssetBundle);
                AssetPathsType assetPath{};
                Il2cppUtils::iterate_IEnumerable<Il2CppString*>(allAssetPaths, [&assetPath](Il2CppString* path)
                    {
                        // ExtraAssetBundleAssetPaths.emplace(path->start_char);
                        // printf("Asset loaded: %ls\n", path->start_char);
                        assetPath.emplace(path->start_char);
                    });
                CustomAssetBundleAssetPaths.emplace(i, assetPath);
                CustomAssetBundleHandleMap.emplace(i, UnityResolve::Invoke<uint32_t>("il2cpp_gchandle_new", extraAssetBundle, false));
            }
            else
            {
                Log::ErrorFmt("Cannot load asset bundle: %s\n", i.c_str());
            }
        }
    }

    uint32_t GetBundleHandleByAssetName(std::wstring assetName) {
        for (const auto& i : CustomAssetBundleAssetPaths) {
            for (const auto& m : i.second) {
                if (std::equal(m.begin(), m.end(), assetName.begin(), assetName.end(),
                    [](wchar_t c1, wchar_t c2) {
                        return std::tolower(c1, std::locale()) == std::tolower(c2, std::locale());
                    })) {
                    return CustomAssetBundleHandleMap.at(i.first);
                }
            }
        }
        return NULL;
    }

    uint32_t GetBundleHandleByAssetName(std::string assetName) {
        return GetBundleHandleByAssetName(utility::conversions::to_string_t(assetName));
    }

    uint32_t ReplaceFontHandle;

    void* GetReplaceFont() {
        static auto FontClass = Il2cppUtils::GetClass("UnityEngine.TextRenderingModule.dll", "UnityEngine", "Font");
        static auto Font_Type = UnityResolve::Invoke<Il2cppUtils::Il2CppReflectionType*>("il2cpp_type_get_object", 
            UnityResolve::Invoke<void*>("il2cpp_class_get_type", FontClass->address));

        using Il2CppString = UnityResolve::UnityType::String;
        const auto fontPath = "assets/fonts/gkamszhfontmix.otf";

        void* replaceFont{};
        const auto& bundleHandle = GetBundleHandleByAssetName(fontPath);
        if (bundleHandle)
        {
            if (ReplaceFontHandle)
            {
                replaceFont = UnityResolve::Invoke<void*>("il2cpp_gchandle_get_target", ReplaceFontHandle);
                // 加载场景时会被 Resources.UnloadUnusedAssets 干掉，且不受 DontDestroyOnLoad 影响，暂且判断是否存活，并在必要的时候重新加载
                // TODO: 考虑挂载到 GameObject 上
                // AssetBundle 不会被干掉
                if (IsNativeObjectAlive(replaceFont))
                {
                    return replaceFont;
                }
                else
                {
                    UnityResolve::Invoke<void>("il2cpp_gchandle_free", std::exchange(ReplaceFontHandle, 0));
                }
            }

            const auto extraAssetBundle = UnityResolve::Invoke<void*>("il2cpp_gchandle_get_target", bundleHandle);
			static auto AssetBundle_LoadAsset = reinterpret_cast<void* (*)(void* _this, Il2CppString* name, Il2cppUtils::Il2CppReflectionType* type)>(
                Il2cppUtils::il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadAsset_Internal(System.String,System.Type)")
				);;

            replaceFont = AssetBundle_LoadAsset(extraAssetBundle, Il2cppString::New(fontPath), Font_Type);
            if (replaceFont)
            {
                ReplaceFontHandle = UnityResolve::Invoke<uint32_t>("il2cpp_gchandle_new", replaceFont, false);
            }
            else
            {
                Log::Error("Cannot load asset font\n");
            }
        }
        else
        {
            Log::Error("Cannot find asset font\n");
        }
        return replaceFont;
    }
#else
    void* fontCache = nullptr;
    void* GetReplaceFont() {
        static auto fontName = Local::GetBasePath() / "local-files" / "gkamsZHFontMIX.otf";
        if (!std::filesystem::exists(fontName)) {
            return nullptr;
        }

        static auto CreateFontFromPath = reinterpret_cast<void (*)(void* self, Il2cppString* path)>(
                Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Font::Internal_CreateFontFromPath(UnityEngine.Font,System.String)")
        );
        static auto Font_klass = Il2cppUtils::GetClass("UnityEngine.TextRenderingModule.dll",
                                                       "UnityEngine", "Font");
        static auto Font_ctor = Il2cppUtils::GetMethod("UnityEngine.TextRenderingModule.dll",
                                                       "UnityEngine", "Font", ".ctor");
        if (fontCache) {
            if (IsNativeObjectAlive(fontCache)) {
                return fontCache;
            }
        }

        const auto newFont = Font_klass->New<void*>();
        Font_ctor->Invoke<void>(newFont);

        CreateFontFromPath(newFont, Il2cppString::New(fontName.string()));
        fontCache = newFont;
        return newFont;
    }
#endif

    std::unordered_set<void*> updatedFontPtrs{};
    void UpdateFont(void* TMP_Textself) {
        if (!Config::replaceFont) return;
        static auto get_font = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll",
                                                      "TMPro", "TMP_Text", "get_font");
        static auto set_font = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll",
                                                      "TMPro", "TMP_Text", "set_font");
//        static auto set_fontMaterial = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll",
//                                                      "TMPro", "TMP_Text", "set_fontMaterial");
//        static auto ForceMeshUpdate = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll",
//                                                      "TMPro", "TMP_Text", "ForceMeshUpdate");
//
//        static auto get_material = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll",
//                                                      "TMPro", "TMP_Asset", "get_material");

        static auto set_sourceFontFile = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll", "TMPro",
                                                                "TMP_FontAsset", "set_sourceFontFile");
        static auto UpdateFontAssetData = Il2cppUtils::GetMethod("Unity.TextMeshPro.dll", "TMPro",
                                                                 "TMP_FontAsset", "UpdateFontAssetData");

        auto newFont = GetReplaceFont();
        if (!newFont) return;

        auto fontAsset = get_font->Invoke<void*>(TMP_Textself);
        if (fontAsset) {
            set_sourceFontFile->Invoke<void>(fontAsset, newFont);
            if (!updatedFontPtrs.contains(fontAsset)) {
                updatedFontPtrs.emplace(fontAsset);
                UpdateFontAssetData->Invoke<void>(fontAsset);
            }
            if (updatedFontPtrs.size() > 200) updatedFontPtrs.clear();
        }
        else {
			Log::Error("UpdateFont: fontAsset is null.");
        }
        set_font->Invoke<void>(TMP_Textself, fontAsset);

//        auto fontMaterial = get_material->Invoke<void*>(fontAsset);
//        set_fontMaterial->Invoke<void>(TMP_Textself, fontMaterial);
//        ForceMeshUpdate->Invoke<void>(TMP_Textself, false, false);
    }

    DEFINE_HOOK(void, TMP_Text_PopulateTextBackingArray, (void* self, UnityResolve::UnityType::String* text, int start, int length)) {
        if (!text) {
            return TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length);
        }

        static auto Substring = Il2cppUtils::GetMethod("mscorlib.dll", "System", "String", "Substring",
                                                       {"System.Int32", "System.Int32"});

        const std::string origText = Substring->Invoke<Il2cppString*>(text, start, length)->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            UpdateFont(self);
            return TMP_Text_PopulateTextBackingArray_Orig(self, newText, 0, newText->length);
        }

        if (Config::textTest) {
            TMP_Text_PopulateTextBackingArray_Orig(self, UnityResolve::UnityType::String::New("[TP]" + text->ToString()), start, length + 4);
        }
        else {
            TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length);
        }
        UpdateFont(self);
    }

    DEFINE_HOOK(void, TMP_Text_SetText_2, (void* self, Il2cppString* sourceText, bool syncTextInputBox, void* mtd)) {
		if (!sourceText) {
			return TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
		}
		const std::string origText = sourceText->ToString();
		std::string transText;
		if (Local::GetGenericText(origText, &transText)) {
			const auto newText = UnityResolve::UnityType::String::New(transText);
			UpdateFont(self);
			return TMP_Text_SetText_2_Orig(self, newText, syncTextInputBox, mtd);
		}
		if (Config::textTest) {
			TMP_Text_SetText_2_Orig(self, UnityResolve::UnityType::String::New("[TS]" + sourceText->ToString()), syncTextInputBox, mtd);
		}
		else {
			TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
		}
		UpdateFont(self);
    }

    DEFINE_HOOK(void, TextMeshProUGUI_Awake, (void* self, void* method)) {
        // Log::InfoFmt("TextMeshProUGUI_Awake at %p, self at %p", TextMeshProUGUI_Awake_Orig, self);

        const auto TMP_Text_klass = Il2cppUtils::GetClass("Unity.TextMeshPro.dll",
                                                                     "TMPro", "TMP_Text");
        const auto get_Text_method = TMP_Text_klass->Get<UnityResolve::Method>("get_text");
        const auto set_Text_method = TMP_Text_klass->Get<UnityResolve::Method>("set_text");
        const auto currText = get_Text_method->Invoke<UnityResolve::UnityType::String*>(self);
        if (currText) {
            //Log::InfoFmt("TextMeshProUGUI_Awake: %s", currText->ToString().c_str());
            std::string transText;
            if (Local::GetGenericText(currText->ToString(), &transText)) {
                if (Config::textTest) {
                    set_Text_method->Invoke<void>(self, UnityResolve::UnityType::String::New("[TA]" + transText));
                }
                else {
                    set_Text_method->Invoke<void>(self, UnityResolve::UnityType::String::New(transText));
                }
            }
        }

        // set_font->Invoke<void>(self, font);
        UpdateFont(self);
        TextMeshProUGUI_Awake_Orig(self, method);
    }

    // TODO 文本未hook完整
    DEFINE_HOOK(void, TextField_set_value, (void* self, Il2cppString* value)) {
        Log::DebugFmt("TextField_set_value: %s", value->ToString().c_str());
        TextField_set_value_Orig(self, value);
    }

    // 未使用的 Hook
    DEFINE_HOOK(void, EffectGroup_ctor, (void* self, void* mtd)) {
        // auto self_klass = Il2cppUtils::get_class_from_instance(self);
        // Log::DebugFmt("EffectGroup_ctor: self: %s::%s", self_klass->namespaze, self_klass->name);
        EffectGroup_ctor_Orig(self, mtd);
    }

    // 用于本地化 MasterDB
    DEFINE_HOOK(void, MessageExtensions_MergeFrom, (void* message, void* span, void* mtd)) {
        MessageExtensions_MergeFrom_Orig(message, span, mtd);
        if (message) {
            auto ret_klass = Il2cppUtils::get_class_from_instance(message);
            if (ret_klass) {
                // Log::DebugFmt("LocalizeMasterItem: %s", ret_klass->name);
                MasterLocal::LocalizeMasterItem(message, ret_klass->name);
            }
        }
    }

    /*
    // 未使用的 Hook
    DEFINE_HOOK(void, MasterBase_GetAll, (void* self, UnityResolve::UnityType::Array<UnityResolve::UnityType::Byte>* getAllSQL,
            int sqlLength, UnityResolve::UnityType::List<void*>* result, void* predicate, void* comparison, void* mtd)) {
        // result: List<Campus.Common.Proto.Client.Master.*>, 和 query 的表名一致

        MasterBase_GetAll_Orig(self, getAllSQL, sqlLength, result, predicate, comparison, mtd);

        auto data_ptr = reinterpret_cast<std::uint8_t*>(getAllSQL->GetData());
        std::string qS(data_ptr, data_ptr + sqlLength);


        Il2cppUtils::Tools::CSListEditor resultList(result);
        MasterLocal::LocalizeMaster(qS, result);
    }

    void LocalizeFindByKey(void* result, void* self) {
        return;  // 暂时不需要了
        auto self_klass = Il2cppUtils::get_class_from_instance(self);
        Log::DebugFmt("Localize: %s", self_klass->name);  // FeatureLockMaster
        // return;

        if (!result) return;
        auto result_klass = Il2cppUtils::get_class_from_instance(result);
        std::string klassName = result_klass->name;

        auto MasterBase_klass = Il2cppUtils::get_class_from_instance(self);
        auto MasterBase_GetTableName = Il2cppUtils::il2cpp_class_get_method_from_name(MasterBase_klass, "GetTableName", 0);
        if (MasterBase_GetTableName) {
            auto tableName = reinterpret_cast<Il2cppString* (*)(void*, void*)>(MasterBase_GetTableName->methodPointer)(self, MasterBase_GetTableName);
            // Log::DebugFmt("MasterBase_FindByKey: %s", tableName->ToString().c_str());

            if (klassName == "List`1") {
                MasterLocal::LocalizeMaster(result, tableName->ToString());
            }
            else {
                MasterLocal::LocalizeMasterItem(result, tableName->ToString());
            }
        }
    }*/

    DEFINE_HOOK(Il2cppString*, OctoCaching_GetResourceFileName, (void* data, void* method)) {
        auto ret = OctoCaching_GetResourceFileName_Orig(data, method);
        //Log::DebugFmt("OctoCaching_GetResourceFileName: %s", ret->ToString().c_str());
        return ret;
    }

    DEFINE_HOOK(void, OctoResourceLoader_LoadFromCacheOrDownload,
                (void* self, Il2cppString* resourceName, void* onComplete, void* onProgress, void* method)) {

        Log::DebugFmt("OctoResourceLoader_LoadFromCacheOrDownload: %s\n", resourceName->ToString().c_str());

        std::string replaceStr;
        if (Local::GetResourceText(resourceName->ToString(), &replaceStr)) {
            const auto onComplete_klass = Il2cppUtils::get_class_from_instance(onComplete);
            const auto onComplete_invoke_mtd = UnityResolve::Invoke<Il2cppUtils::MethodInfo*>(
                    "il2cpp_class_get_method_from_name", onComplete_klass, "Invoke", 2);
            if (onComplete_invoke_mtd) {
                const auto onComplete_invoke = reinterpret_cast<void (*)(void*, Il2cppString*, void*)>(
                        onComplete_invoke_mtd->methodPointer
                );
                onComplete_invoke(onComplete, UnityResolve::UnityType::String::New(replaceStr), nullptr);
                return;
            }
        }

        return OctoResourceLoader_LoadFromCacheOrDownload_Orig(self, resourceName, onComplete, onProgress, method);
    }

    DEFINE_HOOK(void, OnDownloadProgress_Invoke, (void* self, Il2cppString* name, uint64_t receivedLength, uint64_t contentLength)) {
        Log::DebugFmt("OnDownloadProgress_Invoke: %s, %lu/%lu", name->ToString().c_str(), receivedLength, contentLength);
        OnDownloadProgress_Invoke_Orig(self, name, receivedLength, contentLength);
    }

    // UnHooked
    DEFINE_HOOK(UnityResolve::UnityType::String*, UI_I18n_GetOrDefault, (void* self,
            UnityResolve::UnityType::String* key, UnityResolve::UnityType::String* defaultKey, void* method)) {

        auto ret = UI_I18n_GetOrDefault_Orig(self, key, defaultKey, method);

        // Log::DebugFmt("UI_I18n_GetOrDefault: key: %s, default: %s, result: %s", key->ToString().c_str(), defaultKey->ToString().c_str(), ret->ToString().c_str());

        return ret;
        // return UnityResolve::UnityType::String::New("[I18]" + ret->ToString());
    }

    /*
    DEFINE_HOOK(void*, UserDataManagerBase_get__userIdolCardSkinList, (void* self, void* mtd)) {  // Live默认选择
        auto ret = UserDataManagerBase_get__userIdolCardSkinList_Orig(self, mtd);
        Log::DebugFmt("UserDataManagerBase_get__userIdolCardSkinList: %p", ret);
        return ret;
    }
    DEFINE_HOOK(void*, UserDataManagerBase_get__userCostumeList, (void* self, void* mtd)) {  // 服装选择界面
        auto ret = UserDataManagerBase_get__userCostumeList_Orig(self, mtd);
        Log::DebugFmt("UserDataManagerBase_get__userCostumeList: %p", ret);
        return ret;
    }
    DEFINE_HOOK(void*, UserDataManagerBase_get__userCostumeHeadList, (void* self, void* mtd)) {  // 服装选择界面
        auto ret = UserDataManagerBase_get__userCostumeHeadList_Orig(self, mtd);
        Log::DebugFmt("UserDataManagerBase_get__userCostumeHeadList: %p", ret);
        return ret;
    }*/

    DEFINE_HOOK(bool, UserIdolCardSkinCollection_Exists, (void* self, Il2cppString* id, void* mtd)) { // Live默认选择
        auto ret = UserIdolCardSkinCollection_Exists_Orig(self, id, mtd);
        // Log::DebugFmt("UserIdolCardSkinCollection_Exists: %s, ret: %d", id->ToString().c_str(), ret);
        if (!Config::unlockAllLive) return ret;

        if (id) {
            std::string idStr = id->ToString();
            if (idStr.starts_with("music") || idStr.starts_with("i_card-skin")) {  // eg. music-all-kllj-006, i_card-skin-hski-3-002
                return true;
            }
        }
        return ret;
    }

#ifdef GKMS_WINDOWS
    DEFINE_HOOK(void, PictureBookLiveThumbnailView_SetDataAsync, (void* retstr, void* self, void* liveData, Il2cppString* characterId, bool isReleased, bool isUnlocked, bool isNew, bool hasLiveSkin, void* ct, void* mtd)) {
        // Log::DebugFmt("PictureBookLiveThumbnailView_SetDataAsync: isReleased: %d, isUnlocked: %d, isNew: %d, hasLiveSkin: %d", isReleased, isUnlocked, isNew, hasLiveSkin);
        if (Config::dbgMode && Config::unlockAllLive) {
            isUnlocked = true;
            isReleased = true;
            hasLiveSkin = true;
        }
        PictureBookLiveThumbnailView_SetDataAsync_Orig(retstr, self, liveData, characterId, isReleased, isUnlocked, isNew, hasLiveSkin, ct, mtd);
    }
#else
    DEFINE_HOOK(void, PictureBookLiveThumbnailView_SetDataAsync, (void* self, void* liveData, Il2cppString* characterId, bool isReleased, bool isUnlocked, bool isNew, bool hasLiveSkin, void* ct, void* mtd)) {
        // Log::DebugFmt("PictureBookLiveThumbnailView_SetDataAsync: isReleased: %d, isUnlocked: %d, isNew: %d, hasLiveSkin: %d", isReleased, isUnlocked, isNew, hasLiveSkin);
        if (Config::dbgMode && Config::unlockAllLive) {
            isUnlocked = true;
            isReleased = true;
            hasLiveSkin = true;
        }
        PictureBookLiveThumbnailView_SetDataAsync_Orig(self, liveData, characterId, isReleased, isUnlocked, isNew, hasLiveSkin, ct, mtd);
    }
#endif

    enum class GetIdolIdType {
        MusicId,
        CostumeId,
        CostumeHeadId
    };

    std::vector<std::string> GetIdolMusicIdAll(const std::string& charaNameId = "", GetIdolIdType getType = GetIdolIdType::MusicId) {
        // 传入例: fktn
        // System.Collections.Generic.List`1<valuetype [mscorlib]System.ValueTuple`2<class Campus.Common.Proto.Client.Master.IdolCardSkin, class Campus.Common.Proto.Client.Master.Music>>
        static auto get_IdolCardSkinMaster = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Master", "MasterManager", "get_IdolCardSkinMaster");
        static auto Master_GetAllWithSortByKey = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Master", "IdolCardSkinMaster", "GetAllWithSortByKey");
        static auto IdolCardSkin_get_Id = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master", "IdolCardSkin", "get_Id");
        static auto IdolCardSkin_get_IdolCardId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master", "IdolCardSkin", "get_IdolCardId");
        static auto IdolCardSkin_GetMusic = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master", "IdolCardSkin", "GetMusic");
        static auto IdolCardSkin_get_MusicId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master", "IdolCardSkin", "get_MusicId");
        static auto IdolCardSkin_get_CostumeId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master", "IdolCardSkin", "get_CostumeId");
        static auto IdolCardSkin_get_CostumeHeadId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master", "IdolCardSkin", "get_CostumeHeadId");
        static auto GetLiveMusics = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.OutGame",
                                                           "PictureBookWindowPresenter", "GetLiveMusics");

        auto idolCardSkinMaster = get_IdolCardSkinMaster->Invoke<void*>(nullptr);  // IdolCardSkinMaster

        std::vector<std::string> ret{};

        if (!idolCardSkinMaster) {
            Log::ErrorFmt("get_IdolCardSkinMaster failed: %p", idolCardSkinMaster);
            return ret;
        }
        // List<IdolCardSkin>
        auto idolCardSkinList = Master_GetAllWithSortByKey->Invoke<UnityResolve::UnityType::List<void*>*>(idolCardSkinMaster, 0x0, nullptr);

        auto idolCardSkins = idolCardSkinList->ToArray()->ToVector();
        const auto checkStartCharaId = "i_card-" + charaNameId;
        // Log::DebugFmt("checkStartCharaId: %s", checkStartCharaId.c_str());

        // origMusics->Clear();
        UnityResolve::Method* idGetFunc = nullptr;
        switch (getType) {
            case GetIdolIdType::MusicId: idGetFunc = IdolCardSkin_get_MusicId;
                break;
            case GetIdolIdType::CostumeId: idGetFunc = IdolCardSkin_get_CostumeId;
                break;
            case GetIdolIdType::CostumeHeadId: idGetFunc = IdolCardSkin_get_CostumeHeadId;
                break;
            default:
                idGetFunc = IdolCardSkin_get_MusicId;
        }

        for (auto i : idolCardSkins) {
            if (!i) continue;
            // auto charaId = IdolCardSkin_get_Id->Invoke<Il2cppString*>(i);
            auto targetId = idGetFunc->Invoke<Il2cppString*>(i);
            auto cardId = IdolCardSkin_get_IdolCardId->Invoke<Il2cppString*>(i)->ToString();
            auto music = IdolCardSkin_GetMusic->Invoke<void*>(i);

            if (charaNameId.empty() || cardId.starts_with(checkStartCharaId)) {
                std::string musicIdStr = targetId->ToString();
                // Log::DebugFmt("Add cardId: %s, musicId: %s", cardId.c_str(), musicIdStr.c_str());
                if (std::find(ret.begin(), ret.end(), musicIdStr) == ret.end()) {
                    ret.emplace_back(musicIdStr);
                }
            }
        }
        return ret;
    }

    void* AddIdsToUserDataCollectionFromMaster(void* origList, std::vector<std::string>& allIds,
                                               UnityResolve::Method* get_CostumeId, UnityResolve::Method* set_CostumeId, UnityResolve::Method* Clone) {
        std::unordered_set<std::string> existIds{};
        Il2cppUtils::Tools::CSListEditor listEditor(origList);
        if (listEditor.get_Count() <= 0) {
            return origList;
        }

        for (auto i : listEditor) {
            auto costumeId = get_CostumeId->Invoke<Il2cppString*>(i);
            if (!costumeId) continue;
            existIds.emplace(costumeId->ToString());
        }

        for (auto& i : allIds) {
            if (i.empty()) continue;
            // Log::DebugFmt("Try add %s", i.c_str());
            if (existIds.contains(i)) continue;

            auto userCostume = Clone->Invoke<void*>(listEditor.get_Item(0));
            set_CostumeId->Invoke<void>(userCostume, Il2cppString::New(i));
            listEditor.Add(userCostume);
        }
        return origList;
    }

    DEFINE_HOOK(void*, UserCostumeCollection_FindBy, (void* self, void* predicate, void* mtd)) {
        auto ret = UserCostumeCollection_FindBy_Orig(self, predicate, mtd);
        if (!Config::unlockAllLiveCostume) return ret;

        auto this_klass = Il2cppUtils::get_class_from_instance(self);
        // auto predicate_klass = Il2cppUtils::get_class_from_instance(predicate);  // System::Predicate`1
        // Log::DebugFmt("UserCostumeCollection_FindBy this: %s::%s, predicate: %s::%s", this_klass->namespaze, this_klass->name,
        //               predicate_klass->namespaze, predicate_klass->name);

        static auto UserCostumeCollection_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.Common.User",
                                                                        "UserCostumeCollection");
        static auto UserCostumeCollection_GetAllList_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(
                UserCostumeCollection_klass->address, "GetAllList", 1);
        static auto UserCostumeCollection_GetAllList = reinterpret_cast<void* (*)(void*, void*)>(UserCostumeCollection_GetAllList_mtd->methodPointer);

        std::string thisKlassName(this_klass->name);
        // Campus.Common.User::UserCostumeHeadCollection || Campus.Common.User::UserCostumeCollection
        // 两个 class 的 GetAllList 均使用的父类 Qua.UserDataManagement.UserDataCollectionBase`2 的方法，地址一致
        if (thisKlassName == "UserCostumeHeadCollection") {
            static auto UserCostume_Clone = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Transaction", "UserCostumeHead", "Clone");
            static auto UserCostume_get_CostumeHeadId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Transaction", "UserCostumeHead", "get_CostumeHeadId");
            static auto UserCostume_set_CostumeHeadId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Transaction", "UserCostumeHead", "set_CostumeHeadId");

            // auto ret_klass = Il2cppUtils::get_class_from_instance(ret);  // WhereEnumerableIterator
            auto origList = UserCostumeCollection_GetAllList(self, nullptr);

            auto allIds = GetIdolMusicIdAll("", GetIdolIdType::CostumeHeadId);

            // List<Campus.Common.Proto.Client.Transaction.UserCostumeHead>
            return AddIdsToUserDataCollectionFromMaster(origList, allIds, UserCostume_get_CostumeHeadId, UserCostume_set_CostumeHeadId, UserCostume_Clone);
        }
        else if (thisKlassName == "UserCostumeCollection") {
            // static auto UserCostume_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Transaction", "UserCostume");
            static auto UserCostume_Clone = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Transaction", "UserCostume", "Clone");
            static auto UserCostume_get_CostumeId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Transaction", "UserCostume", "get_CostumeId");
            static auto UserCostume_set_CostumeId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Transaction", "UserCostume", "set_CostumeId");

            // auto ret_klass = Il2cppUtils::get_class_from_instance(ret);  // WhereEnumerableIterator
            auto origList = UserCostumeCollection_GetAllList(self, nullptr);

            auto allIds = GetIdolMusicIdAll("", GetIdolIdType::CostumeId);

            // List<Campus.Common.Proto.Client.Transaction.UserCostume>
            return AddIdsToUserDataCollectionFromMaster(origList, allIds, UserCostume_get_CostumeId, UserCostume_set_CostumeId, UserCostume_Clone);
        }

        return ret;
    }

    void* getCompletedUniTask() {
        static auto unitask_klass = Il2cppUtils::GetClass("UniTask.dll", "Cysharp.Threading.Tasks", "UniTask");
        static auto CompletedTask_field = unitask_klass->Get<UnityResolve::Field>("CompletedTask");
        auto ret = UnityResolve::Invoke<void*>("il2cpp_object_new", unitask_klass->address);
        UnityResolve::Invoke<void>("il2cpp_field_static_get_value", CompletedTask_field->address, ret);
        return ret;
    }

#ifdef GKMS_WINDOWS
    // 绕过切歌时的等待以及网络请求
    DEFINE_HOOK(void*, Produce_ViewPictureBookLiveAsync, (void* retstr, Il2cppString* musicId, Il2cppString* characterId,
        void* ct, void* callOption, void* errorHandlerIl, Il2cppString* requestIdForResponseCache, void* mtd)) {

        // Log::DebugFmt("Produce_ViewPictureBookLiveAsync: %s - %s", musicId->ToString().c_str(), characterId->ToString().c_str());
        if (Config::unlockAllLive) return getCompletedUniTask();
        return Produce_ViewPictureBookLiveAsync_Orig(retstr, musicId, characterId, ct, callOption, errorHandlerIl, requestIdForResponseCache, mtd);
    }
#else
    DEFINE_HOOK(void*, Produce_ViewPictureBookLiveAsync, (void* retstr, void* musicId, void* characterId,
        void* ct, void* callOption, void* errorHandlerIl, void* requestIdForResponseCache, void* mtd, void* wenhao)) {

        // Log::DebugFmt("Produce_ViewPictureBookLiveAsync: %s - %s", musicId->ToString().c_str(), characterId->ToString().c_str());
        if (Config::unlockAllLive) return getCompletedUniTask();
        return Produce_ViewPictureBookLiveAsync_Orig(retstr, musicId, characterId, ct, callOption, errorHandlerIl, requestIdForResponseCache, mtd, wenhao);
    }
#endif // GKMS_WINDOWS


    void* PictureBookWindowPresenter_instance = nullptr;
    std::string PictureBookWindowPresenter_charaId;
    DEFINE_HOOK(void*, PictureBookWindowPresenter_GetLiveMusics, (void* self, Il2cppString* charaId, void* mtd)) {
        // Log::DebugFmt("GetLiveMusics: %s", charaId->ToString().c_str());

        if (Config::unlockAllLive) {
            PictureBookWindowPresenter_instance = self;
            PictureBookWindowPresenter_charaId = charaId->ToString();

            static auto PictureBookWindowPresenter_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.OutGame",
                                                                                 "PictureBookWindowPresenter");
            static auto existsMusicIds_field = PictureBookWindowPresenter_klass->Get<UnityResolve::Field>("_existsMusicIds");
            // auto existsMusicIds = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::List<Il2cppString*>*>(self, existsMusicIds_field);
            auto existsMusicIds = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Dictionary<Il2cppString*, UnityResolve::UnityType::List<Il2cppString*>>*>(self, existsMusicIds_field);

            if (!existsMusicIds) {
                static auto Dict_List_String_klass = Il2cppUtils::get_system_class_from_reflection_type_str(
                    "System.Collections.Generic.Dictionary`2[System.String, System.Collections.Generic.List`1[System.String]]");
                static auto List_String_klass = Il2cppUtils::get_system_class_from_reflection_type_str(
                    "System.Collections.Generic.List`1[System.String]");
                static auto List_String_ctor_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(List_String_klass, ".ctor", 0);
                static auto List_String_ctor = reinterpret_cast<void (*)(void*, void*)>(List_String_ctor_mtd->methodPointer);

                auto fullIds = GetIdolMusicIdAll();

                static auto Dict_List_String_ctor_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(Dict_List_String_klass, ".ctor", 0);
                static auto Dict_List_String_ctor = reinterpret_cast<void (*)(void*, void*)>(Dict_List_String_ctor_mtd->methodPointer);

                auto newDict = UnityResolve::Invoke<void*>("il2cpp_object_new", Dict_List_String_klass);
                Dict_List_String_ctor(newDict, Dict_List_String_ctor_mtd);
                Il2cppUtils::Tools::CSDictEditor<Il2cppString*, void*> newDictEditor(newDict, Dict_List_String_klass);

                // auto fullIds = GetIdolMusicIdAll();

                for (auto& i : fullIds) {
                    // Log::DebugFmt("GetLiveMusics - Add: %s", i.c_str());  // eg. music-all-amao-001, music-char-hski-001
                    //newListEditor.Add(Il2cppString::New(i));
					auto newList = UnityResolve::Invoke<void*>("il2cpp_object_new", List_String_klass);
                    List_String_ctor(newList, List_String_ctor_mtd);
					newDictEditor.Add(Il2cppString::New(i), newList);
                }
                Il2cppUtils::ClassSetFieldValue(self, existsMusicIds_field, newDict);
                existsMusicIds = reinterpret_cast<decltype(existsMusicIds)>(newDict);
                // Log::DebugFmt("GetLiveMusics - set end: %d", fullIds.size());
            }

            /*
            Il2cppUtils::Tools::CSDictEditor<Il2cppString*, void*> dicCheckEditor(existsMusicIds, Dict_List_String_klass);
            for (auto& i : fullIds) {
				auto currKeyStr = Il2cppString::New(i);
                void* currList;
                if (dicCheckEditor.ContainsKey(currKeyStr)) {
					currList = dicCheckEditor.get_Item(currKeyStr);
                }
                else {
					currList = UnityResolve::Invoke<void*>("il2cpp_object_new", List_String_klass);
                    List_String_ctor(currList, List_String_ctor_mtd);
                }
                Il2cppUtils::Tools::CSListEditor<Il2cppString*> currListEditor(currList);
                if (!currListEditor.Contains(charaId)) {
                    currListEditor.Add(charaId);
                }
                
            }*/

        }

        return PictureBookWindowPresenter_GetLiveMusics_Orig(self, charaId, mtd);
    }

    DEFINE_HOOK(void, PictureBookLiveSelectScreenModel_ctor, (void* self, void* transitionParam, UnityResolve::UnityType::List<void*>* musics, void* mtd)) {
        // Log::DebugFmt("PictureBookLiveSelectScreenModel_ctor");

        if (Config::dbgMode && Config::unlockAllLive) {
            static auto GetLiveMusics = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.OutGame",
                                                               "PictureBookWindowPresenter", "GetLiveMusics");
            if (PictureBookWindowPresenter_instance && !PictureBookWindowPresenter_charaId.empty()) {
                auto fullMusics = GetLiveMusics->Invoke<UnityResolve::UnityType::List<void*>*>(PictureBookWindowPresenter_instance,
                                                               Il2cppString::New(PictureBookWindowPresenter_charaId));
                return PictureBookLiveSelectScreenModel_ctor_Orig(self, transitionParam, fullMusics, mtd);
            }
        }

        return PictureBookLiveSelectScreenModel_ctor_Orig(self, transitionParam, musics, mtd);
    }

    bool needRestoreHides = false;
    DEFINE_HOOK(void*, PictureBookLiveSelectScreenPresenter_MoveLiveScene, (void* self, void* produceLive, bool isPlayCharacterFocusCamera, void* mtd)) {
        needRestoreHides = false;
        // Log::InfoFmt("MoveLiveScene: characterId: %s, idolCardId: %s, costumeId: %s, costumeHeadId: %s,",
        //              characterId->ToString().c_str(), idolCardId->ToString().c_str(), costumeId->ToString().c_str(), costumeHeadId->ToString().c_str());

        /*
         characterId: hski, costumeId: hski-cstm-0002, costumeHeadId: costume_head_hski-cstm-0002,
         characterId: shro, costumeId: shro-cstm-0006, costumeHeadId: costume_head_shro-cstm-0006,
         */
        /*
        if (Config::dbgMode && Config::enableLiveCustomeDress) {
            // 修改 LiveFixedData_GetCharacter 可以更改 Loading 角色和演唱者名字，而不变更实际登台人
            return PictureBookLiveSelectScreenPresenter_MoveLiveScene_Orig(self, produceLive, characterId, idolCardId,
                                                                           Config::liveCustomeCostumeId.empty() ? costumeId : Il2cppString::New(Config::liveCustomeCostumeId),
                                                                           Config::liveCustomeHeadId.empty() ? costumeHeadId : Il2cppString::New(Config::liveCustomeHeadId),
                                                                           mtd);
        }
         */
        // return PictureBookLiveSelectScreenPresenter_MoveLiveScene_Orig(self, produceLive, characterId, idolCardId, costumeId, costumeHeadId, mtd);
        return PictureBookLiveSelectScreenPresenter_MoveLiveScene_Orig(self, produceLive, isPlayCharacterFocusCamera, mtd);
    }

    // std::string lastMusicId;
#ifdef GKMS_WINDOWS
    DEFINE_HOOK(void*, PictureBookLiveSelectScreenPresenter_OnSelectMusic, (void* retstr, void* self, void* itemModel, void* ct, void* mtd)) {
        // if (!itemModel) return nullptr;
        return PictureBookLiveSelectScreenPresenter_OnSelectMusic_Orig(retstr, self, itemModel, ct, mtd);
    }
#else
    DEFINE_HOOK(void, PictureBookLiveSelectScreenPresenter_OnSelectMusic, (void* self, void* itemModel, void* ct, void* mtd)) {
        /*  // 修改角色后，Live 结束返回时, itemModel 为 null
        Log::DebugFmt("OnSelectMusic itemModel at %p", itemModel);

        static auto GetMusic = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.OutGame",
                                                      "PlaylistMusicContext", "GetMusic");
        static auto GetCurrMusic = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.OutGame.PictureBook",
                                                      "PictureBookLiveSelectMusicListItemModel", "get_Music");
        static auto GetMusicId = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master",
                                                          "Music", "get_Id");

        static auto PictureBookLiveSelectMusicListItemModel_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.OutGame.PictureBook",
                                                                                          "PictureBookLiveSelectMusicListItemModel");
        static auto PictureBookLiveSelectMusicListItemModel_ctor = Il2cppUtils::GetMethod("Assembly-CSharp.dll", "Campus.OutGame.PictureBook",
                                                                                          "PictureBookLiveSelectMusicListItemModel", ".ctor", {"*", "*"});

        if (!itemModel) {
            Log::DebugFmt("OnSelectMusic block", itemModel);
            auto music = GetMusic->Invoke<void*>(lastMusicId);
            auto newItemModel = PictureBookLiveSelectMusicListItemModel_klass->New<void*>();
            PictureBookLiveSelectMusicListItemModel_ctor->Invoke<void>(newItemModel, music, false);

            return PictureBookLiveSelectScreenPresenter_OnSelectMusic_Orig(self, newItemModel, isFirst, mtd);
        }

        if (itemModel) {
            auto currMusic = GetCurrMusic->Invoke<void*>(itemModel);
            auto musicId = GetMusicId->Invoke<Il2cppString*>(currMusic);
            lastMusicId = musicId->ToString();
        }*/
        if (!itemModel) return;
        return PictureBookLiveSelectScreenPresenter_OnSelectMusic_Orig(self, itemModel, ct, mtd);
    }
#endif

    DEFINE_HOOK(bool, VLDOF_IsActive, (void* self)) {
        if (Config::enableFreeCamera) return false;
        return VLDOF_IsActive_Orig(self);
    }

    DEFINE_HOOK(void, CampusQualityManager_set_TargetFrameRate, (void* self, float value)) {
        // Log::InfoFmt("CampusQualityManager_set_TargetFrameRate: %f", value);
        const auto configFps = Config::targetFrameRate;
        CampusQualityManager_set_TargetFrameRate_Orig(self, configFps == 0 ? value : (float)configFps);
    }

    DEFINE_HOOK(void, CampusQualityManager_ApplySetting, (void* self, int qualitySettingsLevel, int maxBufferPixel, float renderScale, int volumeIndex)) {
        if (Config::targetFrameRate != 0) {
            CampusQualityManager_set_TargetFrameRate_Orig(self, Config::targetFrameRate);
        }
        if (Config::useCustomeGraphicSettings) {
            static auto SetReflectionQuality = Il2cppUtils::GetMethod("campus-submodule.Runtime.dll", "Campus.Common",
                                                                      "CampusQualityManager", "SetReflectionQuality");
            static auto SetLODQuality = Il2cppUtils::GetMethod("campus-submodule.Runtime.dll", "Campus.Common",
                                                               "CampusQualityManager", "SetLODQuality");

            static auto Enum_GetValues = Il2cppUtils::GetMethod("mscorlib.dll", "System", "Enum", "GetValues");

            static auto QualityLevel_klass = Il2cppUtils::GetClass("campus-submodule.Runtime.dll", "", "QualityLevel");

            static auto values = Enum_GetValues->Invoke<UnityResolve::UnityType::Array<int>*>(QualityLevel_klass->GetType())->ToVector();
            if (values.empty()) {
                values = {0x0, 0xa, 0x14, 0x1e, 0x28, 0x64};
            }
            if (Config::lodQualityLevel >= values.size()) Config::lodQualityLevel = values.size() - 1;
            if (Config::reflectionQualityLevel >= values.size()) Config::reflectionQualityLevel = values.size() - 1;

            SetLODQuality->Invoke<void>(self, values[Config::lodQualityLevel]);
            SetReflectionQuality->Invoke<void>(self, values[Config::reflectionQualityLevel]);

            qualitySettingsLevel = Config::qualitySettingsLevel;
            maxBufferPixel = Config::maxBufferPixel;
            renderScale = Config::renderScale;
            volumeIndex = Config::volumeIndex;

            Log::ShowToastFmt("ApplySetting\nqualityLevel: %d, maxBufferPixel: %d\nenderScale: %f, volumeIndex: %d\nLODQualityLv: %d, ReflectionLv: %d",
                              qualitySettingsLevel, maxBufferPixel, renderScale, volumeIndex, Config::lodQualityLevel, Config::reflectionQualityLevel);
        }

        CampusQualityManager_ApplySetting_Orig(self, qualitySettingsLevel, maxBufferPixel, renderScale, volumeIndex);
    }

    DEFINE_HOOK(void, UIManager_UpdateRenderTarget, (UnityResolve::UnityType::Vector2 ratio, void* mtd)) {
        // const auto resolution = GetResolution();
        // Log::DebugFmt("UIManager_UpdateRenderTarget: %f, %f", ratio.x, ratio.y);
        return UIManager_UpdateRenderTarget_Orig(ratio, mtd);
    }

    DEFINE_HOOK(void, VLSRPCameraController_UpdateRenderTarget, (void* self, int width, int height, bool forceAlpha, void* method)) {
        // const auto resolution = GetResolution();
        // Log::DebugFmt("VLSRPCameraController_UpdateRenderTarget: %d, %d", width, height);
        return VLSRPCameraController_UpdateRenderTarget_Orig(self, width, height, forceAlpha, method);
    }

    DEFINE_HOOK(void*, VLUtility_GetLimitedResolution, (int32_t screenWidth, int32_t screenHeight,
            UnityResolve::UnityType::Vector2 aspectRatio, int32_t maxBufferPixel, float bufferScale, bool firstCall)) {

        if (Config::useCustomeGraphicSettings && (Config::renderScale > 1.0f)) {
            screenWidth *= Config::renderScale;
            screenHeight *= Config::renderScale;
        }
        //Log::DebugFmt("VLUtility_GetLimitedResolution: %d, %d, %f, %f", screenWidth, screenHeight, aspectRatio.x, aspectRatio.y);
        return VLUtility_GetLimitedResolution_Orig(screenWidth, screenHeight, aspectRatio, maxBufferPixel, bufferScale, firstCall);
    }


    DEFINE_HOOK(void, CampusActorModelParts_OnRegisterBone, (void* self, Il2cppString** name, UnityResolve::UnityType::Transform* bone)) {
        CampusActorModelParts_OnRegisterBone_Orig(self, name, bone);
        // Log::DebugFmt("CampusActorModelParts_OnRegisterBone: %s, %p", (*name)->ToString().c_str(), bone);
    }

    bool InitBodyParts() {
        static auto isInit = false;
        if (isInit) return true;

        const auto Enum_GetValues = Il2cppUtils::GetMethod("mscorlib.dll", "System", "Enum", "GetValues");
        const auto Enum_GetNames = Il2cppUtils::GetMethod("mscorlib.dll", "System", "Enum", "GetNames");

        const auto HumanBodyBones_klass = Il2cppUtils::GetClass(
                "UnityEngine.AnimationModule.dll", "UnityEngine", "HumanBodyBones");

        const auto values = Enum_GetValues->Invoke<UnityResolve::UnityType::Array<int>*>(HumanBodyBones_klass->GetType())->ToVector();
        const auto names = Enum_GetNames->Invoke<UnityResolve::UnityType::Array<Il2cppString*>*>(HumanBodyBones_klass->GetType())->ToVector();
        if (values.size() != names.size()) {
            Log::ErrorFmt("InitBodyParts Error: values count: %ld, names count: %ld", values.size(), names.size());
            return false;
        }

        std::vector<std::string> namesVec{};
        namesVec.reserve(names.size());
        for (auto i :names) {
            namesVec.push_back(i->ToString());
        }
        GKCamera::bodyPartsEnum = Misc::CSEnum(namesVec, values);
        GKCamera::bodyPartsEnum.SetIndex(GKCamera::bodyPartsEnum.GetValueByName("Head"));
        isInit = true;
        return true;
    }

    void HideHead(UnityResolve::UnityType::GameObject* obj, const bool isFace) {
        static UnityResolve::UnityType::GameObject* lastFaceObj = nullptr;
        static UnityResolve::UnityType::GameObject* lastHairObj = nullptr;

#define lastHidedObj (isFace ? lastFaceObj : lastHairObj)

       static auto get_activeInHierarchy = reinterpret_cast<bool (*)(void*)>(
                Il2cppUtils::il2cpp_resolve_icall("UnityEngine.GameObject::get_activeInHierarchy()"));

        const auto isFirstPerson = GKCamera::GetCameraMode() == GKCamera::CameraMode::FIRST_PERSON;

        if (isFirstPerson && obj) {
            if (obj == lastHidedObj) return;
            if (lastHidedObj && IsNativeObjectAlive(lastHidedObj) && get_activeInHierarchy(lastHidedObj)) {
                lastHidedObj->SetActive(true);
            }
            if (IsNativeObjectAlive(obj)) {
                obj->SetActive(false);
                lastHidedObj = obj;
            }
        }
        else {
            if (lastHidedObj && IsNativeObjectAlive(lastHidedObj)) {
                lastHidedObj->SetActive(true);
                lastHidedObj = nullptr;
            }
        }
    }

    DEFINE_HOOK(void, CampusActorController_LateUpdate, (void* self, void* mtd)) {
        static auto CampusActorController_klass = Il2cppUtils::GetClass("campus-submodule.Runtime.dll",
                                                                        "Campus.Common", "CampusActorController");
        static auto rootBody_field = CampusActorController_klass->Get<UnityResolve::Field>("_rootBody");
        static auto parentKlass = UnityResolve::Invoke<void*>("il2cpp_class_get_parent", CampusActorController_klass->address);

        if (!Config::enableFreeCamera || (GKCamera::GetCameraMode() == GKCamera::CameraMode::FREE)) {
            if (needRestoreHides) {
                needRestoreHides = false;
                HideHead(nullptr, false);
                HideHead(nullptr, true);
            }
            return CampusActorController_LateUpdate_Orig(self, mtd);
        }

        static auto GetHumanBodyBoneTransform_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(parentKlass, "GetHumanBodyBoneTransform", 1);
        static auto GetHumanBodyBoneTransform = reinterpret_cast<UnityResolve::UnityType::Transform* (*)(void*, int)>(
                GetHumanBodyBoneTransform_mtd->methodPointer
                );
        static auto get_index_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(CampusActorController_klass->address, "get_index", 0);
        static auto get_Index = get_index_mtd ? reinterpret_cast<int (*)(void*)>(
                get_index_mtd->methodPointer) : [](void*){return 0;};

        const auto currIndex = get_Index(self);
        if (currIndex == GKCamera::followCharaIndex) {
            static auto initPartsSuccess = InitBodyParts();
            static auto headBodyId = initPartsSuccess ? GKCamera::bodyPartsEnum.GetValueByName("Head") : 0xA;
            const auto isFirstPerson = GKCamera::GetCameraMode() == GKCamera::CameraMode::FIRST_PERSON;

            auto targetTrans = GetHumanBodyBoneTransform(self,
                                                         isFirstPerson ? headBodyId : GKCamera::bodyPartsEnum.GetCurrent().second);

            if (targetTrans) {
                cacheTrans = targetTrans;
                cacheRotation = cacheTrans->GetRotation();
                cachePosition = cacheTrans->GetPosition();
                cacheForward = cacheTrans->GetForward();
                cacheLookAt = cacheTrans->GetPosition() + cacheTrans->GetForward() * 3;

                auto rootBody = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(self, rootBody_field);
                auto rootModel = rootBody->GetParent();
                auto rootModelChildCount = rootModel->GetChildCount();
                for (int i = 0; i < rootModelChildCount; i++) {
                    auto rootChild = rootModel->GetChild(i);
                    const auto childName = rootChild->GetName();
                    if (childName == "Root_Face") {
                        for (int n = 0; n < rootChild->GetChildCount(); n++) {
                            auto vLSkinningRenderer = rootChild->GetChild(n);
                            if (vLSkinningRenderer->GetName() == "VLSkinningRenderer") {
                                HideHead(vLSkinningRenderer->GetGameObject(), true);
                                needRestoreHides = true;
                            }
                        }
                    }
                    else if (childName == "Root_Hair") {
                        HideHead(rootChild->GetGameObject(), false);
                        needRestoreHides = true;
                    }
                }
            }
            else {
                cacheTrans = nullptr;
            }

        }

        CampusActorController_LateUpdate_Orig(self, mtd);
    }

    DEFINE_HOOK(bool, PlatformInformation_get_IsAndroid, ()) {
        if (Config::loginAsIOS) {
            return false;
        }
        // Log::DebugFmt("PlatformInformation_get_IsAndroid: 0x%x", ret);
        return PlatformInformation_get_IsAndroid_Orig();
    }

    DEFINE_HOOK(bool, PlatformInformation_get_IsIOS, ()) {
        if (Config::loginAsIOS) {
            return true;
        }
        // Log::DebugFmt("PlatformInformation_get_IsIOS: 0x%x", ret);
        return PlatformInformation_get_IsIOS_Orig();
    }

    DEFINE_HOOK(Il2cppString*, ApiBase_GetPlatformString, (void* self, void* mtd)) {
        if (Config::loginAsIOS) {
            return Il2cppString::New("iOS");
        }
        // Log::DebugFmt("ApiBase_GetPlatformString: %s", ret->ToString().c_str());
        return ApiBase_GetPlatformString_Orig(self, mtd);
    }

    void ProcessApiBase(void* self) {
        static void* processedIOS = nullptr;

        if (Config::loginAsIOS) {
            if (self == processedIOS) return;

            static auto ApiBase_klass = Il2cppUtils::get_class_from_instance(self);
            static auto platform_field = UnityResolve::Invoke<Il2cppUtils::FieldInfo*>("il2cpp_class_get_field_from_name", ApiBase_klass, "_platform");
             auto platform = Il2cppUtils::ClassGetFieldValue<Il2cppString*>(self, platform_field);
             Log::DebugFmt("ProcessApiBase platform: %s", platform ? platform->ToString().c_str() : "null");
             if (platform) {
                 const auto origPlatform = platform->ToString();
                 if (origPlatform != "iOS") {
                     Il2cppUtils::ClassSetFieldValue(self, platform_field, Il2cppString::New("iOS"));
                     processedIOS = self;
                 }
             }
             else {
                 Il2cppUtils::ClassSetFieldValue(self, platform_field, Il2cppString::New("iOS"));
                 processedIOS = self;
             }
        }
        else {
            if (processedIOS) {
                Log::DebugFmt("Restore API");
                static auto ApiBase_klass = Il2cppUtils::get_class_from_instance(self);
                static auto platform_field = UnityResolve::Invoke<Il2cppUtils::FieldInfo*>("il2cpp_class_get_field_from_name", ApiBase_klass, "_platform");
#ifdef GKMS_WINDOWS
                Il2cppUtils::ClassSetFieldValue(self, platform_field, Il2cppString::New("dmm"));
#else
                Il2cppUtils::ClassSetFieldValue(self, platform_field, Il2cppString::New("Android"));
#endif
                processedIOS = nullptr;
            }
        }
    }

    DEFINE_HOOK(void, ApiBase_ctor, (void* self, void* mtd)) {
        ApiBase_ctor_Orig(self, mtd);
        ProcessApiBase(self);
    }

    DEFINE_HOOK(void*, ApiBase_get_Instance, (void* mtd)) {
        auto ret = ApiBase_get_Instance_Orig(mtd);
        if (ret) {
            ProcessApiBase(ret);
        }
        return ret;
    }

#ifdef GKMS_WINDOWS
    // DMM Only
    DEFINE_HOOK(void*, WindowHandle_SetWindowLong, (int32_t nIndex, intptr_t dwNewLong, void* mtd)) {
        if (GakumasLocal::Config::dmmUnlockSize) {
            // Log::DebugFmt("WindowHandle_SetWindowLong: %d, %p\n", nIndex, dwNewLong);

            if (nIndex == GWLP_WNDPROC) {
                return 0;
            }
        }

		return WindowHandle_SetWindowLong_Orig(nIndex, dwNewLong, mtd);
    }

    // DMM Only
	void SetResolution(int width, int height, bool fullscreen) {
		static auto Screen_SetResolution = reinterpret_cast<void (*)(UINT, UINT, UINT, void*)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Screen::SetResolution_Injected(System.Int32,System.Int32,UnityEngine.FullScreenMode,UnityEngine.RefreshRate&)"));

        int64_t v8[3];
        v8[0] = 0x100000000LL;
		Screen_SetResolution(width, height, 2 * !fullscreen + 1, v8);
	}

    // DMM Only
    DEFINE_HOOK(void, WindowManager_ApplyOrientationSettings, (int orientation, void* method)) {
        if (!GakumasLocal::Config::dmmUnlockSize) return WindowManager_ApplyOrientationSettings_Orig(orientation, method);

        static auto get_Height = reinterpret_cast<int (*)()>(Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Screen::get_height()"));
        static auto get_Width = reinterpret_cast<int (*)()>(Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Screen::get_width()"));

		static auto lastWidth = -1;
		static auto lastHeight = -1;

		const auto currWidth = get_Width();
		const auto currHeight = get_Height();

        if (lastWidth == -1) {
			lastWidth = currWidth;
			lastHeight = currHeight;
            return;
		}

		const bool lastIsPortrait = lastWidth < lastHeight;
		const bool currIsPortrait = currWidth < currHeight;
        if (lastIsPortrait == currIsPortrait) {
            lastWidth = currWidth;
            lastHeight = currHeight;
            return;
        }

		SetResolution(lastWidth, lastHeight, false);
		lastWidth = currWidth;
		lastHeight = currHeight;

		Log::DebugFmt("WindowManager_ApplyOrientationSettings: %d (%d, %d)\n", orientation, get_Width(), get_Height());
    }

    // DMM Only
    DEFINE_HOOK(void, AspectRatioHandler_NudgeWindow, (void* method)) {
		if (!GakumasLocal::Config::dmmUnlockSize) return AspectRatioHandler_NudgeWindow_Orig(method);
		// printf("AspectRatioHandler_NudgeWindow\n");
    }
#endif

    void UpdateSwingBreastBonesData(void* initializeData) {
        if (!Config::enableBreastParam) return;
        static auto CampusActorAnimationInitializeData_klass = Il2cppUtils::GetClass("campus-submodule.Runtime.dll", "ActorAnimation",
                                                                                     "CampusActorAnimationInitializeData");
        static auto ActorSwingBreastBone_klass = Il2cppUtils::GetClass("ActorAnimation.Runtime.dll", "ActorAnimation",
                                                                       "ActorSwingBreastBone");
        static auto LimitInfo_klass = Il2cppUtils::GetClass("ActorAnimation.Runtime.dll", "ActorAnimation",
                                                            "LimitInfo");

        static auto Data_swingBreastBones_field = CampusActorAnimationInitializeData_klass->Get<UnityResolve::Field>("swingBreastBones");
        static auto damping_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("damping");
        static auto stiffness_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("stiffness");
        static auto spring_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("spring");
        static auto pendulum_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("pendulum");
        static auto pendulumRange_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("pendulumRange");
        static auto average_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("average");
        static auto rootWeight_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("rootWeight");
        static auto useArmCorrection_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("useArmCorrection");
        static auto isDirty_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("<isDirty>k__BackingField");
        static auto leftBreast_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("leftBreast");
        static auto rightBreast_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("rightBreast");
        static auto leftBreastEnd_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("leftBreastEnd");
        static auto rightBreastEnd_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("rightBreastEnd");
        static auto limitInfo_field = ActorSwingBreastBone_klass->Get<UnityResolve::Field>("limitInfo");

        static auto limitInfo_useLimit_field = LimitInfo_klass->Get<UnityResolve::Field>("useLimit");
        static auto limitInfo_axisX_field = LimitInfo_klass->Get<UnityResolve::Field>("axisX");
        static auto limitInfo_axisY_field = LimitInfo_klass->Get<UnityResolve::Field>("axisY");
        static auto limitInfo_axisZ_field = LimitInfo_klass->Get<UnityResolve::Field>("axisZ");

        auto swingBreastBones = Il2cppUtils::ClassGetFieldValue
                <UnityResolve::UnityType::List<UnityResolve::UnityType::MonoBehaviour*>*>(initializeData, Data_swingBreastBones_field);

        auto boneArr = swingBreastBones->ToArray();
        for (int i = 0; i < boneArr->max_length; i++) {
            auto bone = boneArr->At(i);
            if (!bone) continue;

            auto damping = Il2cppUtils::ClassGetFieldValue<float>(bone, damping_field);
            auto stiffness = Il2cppUtils::ClassGetFieldValue<float>(bone, stiffness_field);
            auto spring = Il2cppUtils::ClassGetFieldValue<float>(bone, spring_field);
            auto pendulum = Il2cppUtils::ClassGetFieldValue<float>(bone, pendulum_field);
            auto pendulumRange = Il2cppUtils::ClassGetFieldValue<float>(bone, pendulumRange_field);
            auto average = Il2cppUtils::ClassGetFieldValue<float>(bone, average_field);
            auto rootWeight = Il2cppUtils::ClassGetFieldValue<float>(bone, rootWeight_field);
            auto useArmCorrection = Il2cppUtils::ClassGetFieldValue<bool>(bone, useArmCorrection_field);
            auto isDirty = Il2cppUtils::ClassGetFieldValue<bool>(bone, isDirty_field);

            auto limitInfo = Il2cppUtils::ClassGetFieldValue<void*>(bone, limitInfo_field);
            auto useLimit = Il2cppUtils::ClassGetFieldValue<int>(limitInfo, limitInfo_useLimit_field);

            if (Config::bUseScale) {
                auto leftBreast = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(bone, leftBreast_field);
                auto rightBreast = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(bone, rightBreast_field);
                auto leftBreastEnd = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(bone, leftBreastEnd_field);
                auto rightBreastEnd = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Transform*>(bone, rightBreastEnd_field);

                const auto setScale = UnityResolve::UnityType::Vector3(Config::bScale, Config::bScale, Config::bScale);
                leftBreast->SetLocalScale(setScale);
                rightBreast->SetLocalScale(setScale);
                leftBreastEnd->SetLocalScale(setScale);
                rightBreastEnd->SetLocalScale(setScale);
            }

            Log::DebugFmt("orig bone: damping: %f, stiffness: %f, spring: %f, pendulum: %f, "
                          "pendulumRange: %f, average: %f, rootWeight: %f, useLimit: %d, useArmCorrection: %d, isDirty: %d",
                          damping, stiffness, spring, pendulum, pendulumRange, average, rootWeight, useLimit, useArmCorrection, isDirty);
            if (!Config::bUseLimit) {
                Il2cppUtils::ClassSetFieldValue(limitInfo, limitInfo_useLimit_field, 0);
            }
            else {
                Il2cppUtils::ClassSetFieldValue(limitInfo, limitInfo_useLimit_field, 1);
                auto axisX = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Vector2Int>(limitInfo, limitInfo_axisX_field);
                auto axisY = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Vector2Int>(limitInfo, limitInfo_axisY_field);
                auto axisZ = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Vector2Int>(limitInfo, limitInfo_axisZ_field);
                axisX.m_X *= Config::bLimitXx;
                axisX.m_Y *= Config::bLimitXy;
                axisY.m_X *= Config::bLimitYx;
                axisY.m_Y *= Config::bLimitYy;
                axisZ.m_X *= Config::bLimitZx;
                axisZ.m_Y *= Config::bLimitZy;
                Il2cppUtils::ClassSetFieldValue(limitInfo, limitInfo_axisX_field, axisX);
                Il2cppUtils::ClassSetFieldValue(limitInfo, limitInfo_axisY_field, axisY);
                Il2cppUtils::ClassSetFieldValue(limitInfo, limitInfo_axisZ_field, axisZ);

            }

            Il2cppUtils::ClassSetFieldValue(bone, damping_field, Config::bDamping);
            Il2cppUtils::ClassSetFieldValue(bone, stiffness_field, Config::bStiffness);
            Il2cppUtils::ClassSetFieldValue(bone, spring_field, Config::bSpring);
            Il2cppUtils::ClassSetFieldValue(bone, pendulum_field, Config::bPendulum);
            Il2cppUtils::ClassSetFieldValue(bone, pendulumRange_field, Config::bPendulumRange);
            Il2cppUtils::ClassSetFieldValue(bone, average_field, Config::bAverage);
            Il2cppUtils::ClassSetFieldValue(bone, rootWeight_field, Config::bRootWeight);
            Il2cppUtils::ClassSetFieldValue(bone, useArmCorrection_field, Config::bUseArmCorrection);
            // Il2cppUtils::ClassSetFieldValue(bone, isDirty_field, Config::bIsDirty);
        }
        // Log::DebugFmt("\n");
    }

    DEFINE_HOOK(void, CampusActorAnimation_Setup, (void* self, void* rootTrans, void* initializeData)) {
        UpdateSwingBreastBonesData(initializeData);
        return CampusActorAnimation_Setup_Orig(self, rootTrans, initializeData);
    }

/*
    std::map<std::string, std::pair<uintptr_t, void*>> findByKeyHookAddress{};
    void* FindByKeyHooks(void* self, void* key, void* mtd) {
        auto self_klass = Il2cppUtils::get_class_from_instance(self);

        if (auto it = findByKeyHookAddress.find(self_klass->name); it != findByKeyHookAddress.end()) {
            Log::DebugFmt("FindByKeyHooks Call cache: %s, %p, %p", self_klass->name, it->second.first, it->second.second);
            return reinterpret_cast<decltype(FindByKeyHooks)*>(it->second.second)(self, key, mtd);
        }
        Log::DebugFmt("FindByKeyHooks not in cache: %s", self_klass->name);

        auto FindByKey_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(self_klass, "FindByKey", 1);
        for (auto& [k, v] : findByKeyHookAddress) {
            if (FindByKey_mtd->methodPointer == v.first) {
                findByKeyHookAddress.emplace(self_klass->name, std::make_pair(FindByKey_mtd->methodPointer, v.second));
                Log::DebugFmt("FindByKeyHooks add to cache: %s", self_klass->name);
                return reinterpret_cast<decltype(FindByKeyHooks)*>(v.second)(self, key, mtd);
            }
        }

        Log::ErrorFmt("FindByKeyHooks not found hook: %s", self_klass->name);
        return SHADOWHOOK_CALL_PREV(FindByKeyHooks, self, key, mtd);
    }

    static inline std::vector<void(*)(HookInstaller* hookInstaller)> g_registerMasterFindByKeyHookFuncs;

#define DEF_AND_ADD_MASTER_FINDBYKEY_HOOK(name)                                \
    using name##_FindByKey_Type = void* (*)(void* self, void* key, void* idx, void* mtd); \
    inline name##_FindByKey_Type name##_FindByKey_Addr = nullptr;              \
    inline void* name##_FindByKey_Orig = nullptr;                              \
    inline void* name##_FindByKey_Hook(void* self, void* key, void* idx, void* mtd) {     \
        auto result = reinterpret_cast<decltype(name##_FindByKey_Hook)*>(      \
            name##_FindByKey_Orig)(self, key, idx, mtd);                            \
        LocalizeFindByKey(result, self);                                       \
        return result;                                                         \
    }                                                                          \
    inline void name##_RegisterHook(HookInstaller* hookInstaller) {            \
        auto klass = Il2cppUtils::GetClass(                                    \
            "Assembly-CSharp.dll", "Campus.Common.Master", #name);             \
        auto mtd = Il2cppUtils::il2cpp_class_get_method_from_name(             \
            klass->address, "GetData", 2);                                   \
        ADD_HOOK(name##_FindByKey, mtd->methodPointer);                        \
    }                                                                          \
    struct name##_RegisterHookPusher {                                         \
        name##_RegisterHookPusher() {                                          \
            g_registerMasterFindByKeyHookFuncs.push_back(&name##_RegisterHook);\
        }                                                                      \
    } g_##name##_RegisterHookPusherInst;

    DEF_AND_ADD_MASTER_FINDBYKEY_HOOK(AchievementMaster)
    DEF_AND_ADD_MASTER_FINDBYKEY_HOOK(ProduceSkillMaster)
    DEF_AND_ADD_MASTER_FINDBYKEY_HOOK(FeatureLockMaster)
    DEF_AND_ADD_MASTER_FINDBYKEY_HOOK(ProduceCardMaster)

    // 安装 DEF_AND_ADD_MASTER_FINDBYKEY_HOOK 的 hook
    void InitMasterHooks(HookInstaller* hookInstaller) {
        for (auto& func : g_registerMasterFindByKeyHookFuncs) {
            func(hookInstaller);
        }
    }
*/

    void StartInjectFunctions() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

#ifdef GKMS_WINDOWS
        auto il2cpp_module = GetModuleHandle("GameAssembly.dll");
        if (!il2cpp_module) {
            Log::ErrorFmt("GameAssembly.dll not loaded.");
        }
        UnityResolve::Init(il2cpp_module, UnityResolve::Mode::Il2Cpp, Config::lazyInit);
        GakumasLocal::WinHooks::Keyboard::InstallWndProcHook();
#else
        UnityResolve::Init(xdl_open(hookInstaller->m_il2cppLibraryPath.c_str(), RTLD_NOW),
            UnityResolve::Mode::Il2Cpp, Config::lazyInit);
#endif

        ADD_HOOK(AssetBundle_LoadAssetAsync, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.AssetBundle::LoadAssetAsync_Internal(System.String,System.Type)"));
        ADD_HOOK(AssetBundleRequest_GetResult, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.AssetBundleRequest::GetResult()"));
        ADD_HOOK(Resources_Load, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.ResourcesAPIInternal::Load(System.String,System.Type)"));

        ADD_HOOK(I18nHelper_SetUpI18n, Il2cppUtils::GetMethodPointer("quaunity-ui.Runtime.dll", "Qua.UI",
                                                                     "I18nHelper", "SetUpI18n"));
        ADD_HOOK(I18nHelper_SetValue, Il2cppUtils::GetMethodPointer("quaunity-ui.Runtime.dll", "Qua.UI",
                                                                     "I18n", "SetValue"));

        //ADD_HOOK(UI_I18n_GetOrDefault, Il2cppUtils::GetMethodPointer("quaunity-ui.Runtime.dll", "Qua.UI",
        //                                                             "I18n", "GetOrDefault"));

        ADD_HOOK(TextMeshProUGUI_Awake, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
                                                                      "TextMeshProUGUI", "Awake"));

        ADD_HOOK(TMP_Text_PopulateTextBackingArray, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
                                                                  "TMP_Text", "PopulateTextBackingArray",
                                                                  {"System.String", "System.Int32", "System.Int32"}));
        ADD_HOOK(TMP_Text_SetText_2, Il2cppUtils::GetMethodPointer("Unity.TextMeshPro.dll", "TMPro",
            "TMP_Text", "SetText",
            { "System.String", "System.Boolean" }));

        ADD_HOOK(TextField_set_value, Il2cppUtils::GetMethodPointer("UnityEngine.UIElementsModule.dll", "UnityEngine.UIElements",
                                                                  "TextField", "set_value"));
        /* SQL 查询相关函数，不好用
        // 下面是 byte[] u8 string 转 std::string 的例子
        auto query = reinterpret_cast<UnityResolve::UnityType::Array<UnityResolve::UnityType::Byte>*>(mtd);
        auto data_ptr = reinterpret_cast<std::uint8_t*>(query->GetData());
        std::string qS(data_ptr, data_ptr + lastLength);

        ADD_HOOK(PreparedStatement_ExecuteQuery, Il2cppUtils::GetMethodPointer("quaunity-master-manager.Runtime.dll", "Qua.Master.SQLite",
                                                                               "PreparedStatement", "ExecuteQuery", {"System.String"}));
        ADD_HOOK(PreparedStatement_ExecuteQuery_u8, Il2cppUtils::GetMethodPointer("quaunity-master-manager.Runtime.dll", "Qua.Master.SQLite",
                                                                                  "PreparedStatement", "ExecuteQuery", {"*", "*"}));
        ADD_HOOK(PreparedStatement_FinalizeStatement, Il2cppUtils::GetMethodPointer("quaunity-master-manager.Runtime.dll", "Qua.Master.SQLite",
                                                                                  "PreparedStatement", "FinalizeStatement"));
       */

        // ADD_HOOK(EffectGroup_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.Proto.Client.Master",
        //                                                          "EffectGroup", ".ctor"));

        ADD_HOOK(MessageExtensions_MergeFrom, Il2cppUtils::GetMethodPointer("Google.Protobuf.dll", "Google.Protobuf",
                                                                            "MessageExtensions", "MergeFrom", {"Google.Protobuf.IMessage", "System.ReadOnlySpan<System.Byte>"}));

        /* // 此 block 为 MasterBase 相关的 hook，后来发现它们最后都会调用 MessageExtensions.MergeFrom 进行构造，遂停用。现留档以备用
        // ADD_HOOK(MasterBase_GetAll, Il2cppUtils::GetMethodPointer("quaunity-master-manager.Runtime.dll", "Qua.Master",
        //                                                          "MasterBase`2", "GetAll", {"*", "*", "*", "*", "*"}));

        // 安装 DEF_AND_ADD_MASTER_FINDBYKEY_HOOK 的 hook
        InitMasterHooks(hookInstaller);

        auto AchievementMaster_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.Common.Master", "AchievementMaster");
        auto AchievementMaster_GetAll_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(AchievementMaster_klass->address, "GetAll", 5);
        // auto AchievementMaster_FindByKey_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(AchievementMaster_klass->address, "FindByKey", 1);
        // Log::DebugFmt("AchievementMaster_GetAll_mtd at %p", AchievementMaster_GetAll_mtd);
        ADD_HOOK(MasterBase_GetAll, AchievementMaster_GetAll_mtd->methodPointer);
        */

        ADD_HOOK(OctoCaching_GetResourceFileName, Il2cppUtils::GetMethodPointer("Octo.dll", "Octo.Caching",
                                                                     "OctoCaching", "GetResourceFileName"));

        ADD_HOOK(OctoResourceLoader_LoadFromCacheOrDownload,
                 Il2cppUtils::GetMethodPointer("Octo.dll", "Octo.Loader",
                                               "OctoResourceLoader", "LoadFromCacheOrDownload",
                                               {"System.String", "System.Action<System.String,Octo.LoadError>", "Octo.OnDownloadProgress"}));

        ADD_HOOK(OnDownloadProgress_Invoke,
                 Il2cppUtils::GetMethodPointer("Octo.dll", "Octo",
                                               "OnDownloadProgress", "Invoke"));

        /*
        auto UserDataManager_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.Common.User",
                                                           "UserDataManager");
        if (UserDataManager_klass) {
            auto UserDataManagerBase_klass = UnityResolve::Invoke<Il2cppUtils::Il2CppClassHead*>("il2cpp_class_get_parent", UserDataManager_klass->address);
            if (UserDataManagerBase_klass) {
                auto get_userIdolCardSkinList_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(UserDataManagerBase_klass, "get__userIdolCardSkinList", 0);
                if (get_userIdolCardSkinList_mtd) {
                    ADD_HOOK(UserDataManagerBase_get__userIdolCardSkinList, get_userIdolCardSkinList_mtd->methodPointer);
                }
                auto get_userCostumeList_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(UserDataManagerBase_klass, "get__userCostumeList", 0);
                if (get_userCostumeList_mtd) {
                    ADD_HOOK(UserDataManagerBase_get__userCostumeList, get_userCostumeList_mtd->methodPointer);
                }
                auto get_userCostumeHeadList_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(UserDataManagerBase_klass, "get__userCostumeHeadList", 0);
                if (get_userCostumeHeadList_mtd) {
                    ADD_HOOK(UserDataManagerBase_get__userCostumeHeadList, get_userCostumeHeadList_mtd->methodPointer);
                }
            }
        }*/

        auto UserIdolCardSkinCollection_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.Common.User",
                                                                      "UserIdolCardSkinCollection");
        auto UserIdolCardSkinCollection_Exists_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(UserIdolCardSkinCollection_klass->address, "Exists", 1);
        if (UserIdolCardSkinCollection_Exists_mtd) {
            ADD_HOOK(UserIdolCardSkinCollection_Exists, UserIdolCardSkinCollection_Exists_mtd->methodPointer);
        }

        auto UserCostumeCollection_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.Common.User",
                                                                      "UserCostumeCollection");
        auto UserCostumeCollection_FindBy_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(
                UserCostumeCollection_klass->address, "FindBy", 1);
        if (UserCostumeCollection_FindBy_mtd) {
            ADD_HOOK(UserCostumeCollection_FindBy, UserCostumeCollection_FindBy_mtd->methodPointer);
        }

        // 双端
        ADD_HOOK(PictureBookLiveThumbnailView_SetDataAsync,
            Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.OutGame.PictureBook",
                "PictureBookLiveThumbnailView", "SetDataAsync", { "*", "*", "*", "*", "*" }));

        ADD_HOOK(PictureBookWindowPresenter_GetLiveMusics,
                 Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.OutGame",
                                               "PictureBookWindowPresenter", "GetLiveMusics"));

#ifdef GKMS_WINDOWS
        // 跳过切歌Loading，安卓端会崩溃
        ADD_HOOK(Produce_ViewPictureBookLiveAsync,
            Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "",
                "Produce", "ViewPictureBookLiveAsync"));
#endif
        ADD_HOOK(PictureBookLiveSelectScreenModel_ctor,
                 Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.OutGame",
                                               "PictureBookLiveSelectScreenModel", ".ctor"));

        ADD_HOOK(PictureBookLiveSelectScreenPresenter_MoveLiveScene,
                 Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.OutGame",
                                               "PictureBookLiveSelectScreenPresenter", "MoveLiveScene"));

        // 双端
        ADD_HOOK(PictureBookLiveSelectScreenPresenter_OnSelectMusic,
            Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.OutGame",
                "PictureBookLiveSelectScreenPresenter", "OnSelectMusicAsync"));

        ADD_HOOK(VLDOF_IsActive,
                 Il2cppUtils::GetMethodPointer("Unity.RenderPipelines.Universal.Runtime.dll", "VL.Rendering",
                                               "VLDOF", "IsActive"));

        ADD_HOOK(CampusQualityManager_ApplySetting,
                 Il2cppUtils::GetMethodPointer("campus-submodule.Runtime.dll", "Campus.Common",
                                               "CampusQualityManager", "ApplySetting"));

        ADD_HOOK(UIManager_UpdateRenderTarget,
                 Il2cppUtils::GetMethodPointer("ADV.Runtime.dll", "Campus.ADV",
                                               "UIManager", "UpdateRenderTarget"));
        ADD_HOOK(VLSRPCameraController_UpdateRenderTarget,
                 Il2cppUtils::GetMethodPointer("vl-unity.Runtime.dll", "VL.Rendering",
                                               "VLSRPCameraController", "UpdateRenderTarget",
                                               {"*", "*", "*"}));

        ADD_HOOK(VLUtility_GetLimitedResolution,
                 Il2cppUtils::GetMethodPointer("vl-unity.Runtime.dll", "VL",
                                               "VLUtility", "GetLimitedResolution",
                                               {"*", "*", "*", "*", "*", "*"}));

        ADD_HOOK(CampusActorModelParts_OnRegisterBone,
                 Il2cppUtils::GetMethodPointer("campus-submodule.Runtime.dll", "Campus.Common",
                                               "CampusActorModelParts", "OnRegisterBone"));
        ADD_HOOK(CampusActorController_LateUpdate,
                 Il2cppUtils::GetMethodPointer("campus-submodule.Runtime.dll", "Campus.Common",
                                               "CampusActorController", "LateUpdate"));

        ADD_HOOK(PlatformInformation_get_IsAndroid, Il2cppUtils::GetMethodPointer("Firebase.Platform.dll", "Firebase.Platform",
                                                                         "PlatformInformation", "get_IsAndroid"));
        ADD_HOOK(PlatformInformation_get_IsIOS, Il2cppUtils::GetMethodPointer("Firebase.Platform.dll", "Firebase.Platform",
                                                                                  "PlatformInformation", "get_IsIOS"));

        auto api_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Campus.Common.Network", "Api");
        if (api_klass) {
            // Qua.Network.ApiBase
            auto api_parent = UnityResolve::Invoke<Il2cppUtils::Il2CppClassHead*>("il2cpp_class_get_parent", api_klass->address);
            if (api_parent) {
                // Log::DebugFmt("api_parent at %p, name: %s::%s", api_parent, api_parent->namespaze, api_parent->name);
                ADD_HOOK(ApiBase_GetPlatformString, Il2cppUtils::il2cpp_class_get_method_pointer_from_name(api_parent, "GetPlatformString", 0));
                ADD_HOOK(ApiBase_ctor, Il2cppUtils::il2cpp_class_get_method_pointer_from_name(api_parent, ".ctor", 0));
                ADD_HOOK(ApiBase_get_Instance, Il2cppUtils::il2cpp_class_get_method_pointer_from_name(api_parent, "get_Instance", 0));
            }
        }

        /*
        static auto CampusActorController_klass = Il2cppUtils::GetClass("campus-submodule.Runtime.dll",
                                                                        "Campus.Common", "CampusActorController");
        for (const auto& i : CampusActorController_klass->methods) {
            Log::DebugFmt("CampusActorController.%s at %p", i->name.c_str(), i->function);
        }*/

        ADD_HOOK(CampusActorAnimation_Setup,
                 Il2cppUtils::GetMethodPointer("campus-submodule.Runtime.dll", "Campus.Common",
                                               "CampusActorAnimation", "Setup"));

        ADD_HOOK(CampusQualityManager_set_TargetFrameRate,
                 Il2cppUtils::GetMethodPointer("campus-submodule.Runtime.dll", "Campus.Common",
                                               "CampusQualityManager", "set_TargetFrameRate"));

        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));

        // 双端
        ADD_HOOK(InternalSetOrientationAsync,
            Il2cppUtils::GetMethodPointer("campus-submodule.Runtime.dll", "Campus.Common",
                "ScreenOrientationControllerBase", "InternalSetOrientationAsync"));

        ADD_HOOK(Unity_set_position_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_position_Injected(UnityEngine.Vector3&)"));
        ADD_HOOK(Unity_set_rotation_Injected, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Transform::set_rotation_Injected(UnityEngine.Quaternion&)"));
        ADD_HOOK(Unity_get_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "get_fieldOfView"));
        ADD_HOOK(Unity_set_fieldOfView, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                      "Camera", "set_fieldOfView"));
        ADD_HOOK(Unity_set_targetFrameRate, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.Application::set_targetFrameRate(System.Int32)"));
        ADD_HOOK(EndCameraRendering, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine.Rendering",
                                                                     "RenderPipeline", "EndCameraRendering"));

#ifdef GKMS_WINDOWS
        ADD_HOOK(WindowHandle_SetWindowLong, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.StandAloneWindow",
            "WindowHandle", "SetWindowLong"));
        //ADD_HOOK(WindowHandle_SetWindowLong32, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.StandAloneWindow",
        //    "WindowHandle", "SetWindowLong32"));
        //ADD_HOOK(WindowHandle_SetWindowLongPtr64, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.StandAloneWindow",
        //    "WindowHandle", "SetWindowLongPtr64"));
        //ADD_HOOK(WindowSizeUtility_RestoreWindowSize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.StandAloneWindow",
        //    "WindowSizeUtility", "RestoreWindowSize"));
        ADD_HOOK(WindowManager_ApplyOrientationSettings, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.StandAloneWindow",
            "WindowManager", "ApplyOrientationSettings"));
        ADD_HOOK(AspectRatioHandler_NudgeWindow, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.StandAloneWindow",
            "AspectRatioHandler", "NudgeWindow"));
        //ADD_HOOK(AspectRatioHandler_WindowProc, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Campus.Common.StandAloneWindow",
        //    "AspectRatioHandler", "WindowProc"));

        if (GakumasLocal::Config::dmmUnlockSize) {
			std::thread([]() {
				std::this_thread::sleep_for(std::chrono::seconds(3));
                    auto hWnd = FindWindowW(L"UnityWndClass", L"gakumas");
                    // 添加可调整大小的边框和最大化按钮
                    LONG style = GetWindowLong(hWnd, GWL_STYLE);
                    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
                    SetWindowLong(hWnd, GWL_STYLE, style);
				}).detach();
        }

        g_extra_assetbundle_paths.push_back((gakumasLocalPath / "local-files/gakumasassets").string());
		LoadExtraAssetBundle();
		GkmsResourceUpdate::CheckUpdateFromAPI(false);
#endif // GKMS_WINDOWS

    }
    // 77 2640 5000

    DEFINE_HOOK(int, il2cpp_init, (const char* domain_name)) {
#ifndef GKMS_WINDOWS
        const auto ret = il2cpp_init_Orig(domain_name);
#else
        const auto ret = 0;
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
        GKCamera::initCameraSettings();

        if (Config::lazyInit) {
            UnityResolveProgress::assembliesProgress.current = 2;
            UnityResolveProgress::classProgress.total = 1;
            UnityResolveProgress::classProgress.current = 0;
        }

        Local::LoadData();
        MasterLocal::LoadData();

        UnityResolveProgress::startInit = false;

        Log::Info("Plugin init finished.");
        return ret;
    }
}


namespace GakumasLocal::Hook {
    void Install() {
        const auto hookInstaller = Plugin::GetInstance().GetHookInstaller();

        Log::Info("Installing hook");

#ifndef GKMS_WINDOWS
        ADD_HOOK(HookMain::il2cpp_init,
            Plugin::GetInstance().GetHookInstaller()->LookupSymbol("il2cpp_init"));
#else
        HookMain::il2cpp_init_Hook(nullptr);
#endif


        Log::Info("Hook installed");
    }
}
