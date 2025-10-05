//
// Created by choco on 2025/7/25.
//

#include "../HookMain.h"
#include "../Local.h"
#include <re2/re2.h>

namespace LinkuraLocal::HookTranslation {
    using Il2cppString = UnityResolve::UnityType::String;

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
            if (Il2cppUtils::IsNativeObjectAlive(fontCache)) {
                return fontCache;
            }
        }

        const auto newFont = Font_klass->New<void*>();
        Font_ctor->Invoke<void>(newFont);

        CreateFontFromPath(newFont, Il2cppString::New(fontName.string()));
        fontCache = newFont;
        return newFont;
    }
    std::unordered_set<void*> updatedFontPtrs{};
    void UpdateTMPFont(void* TMP_Textself) {
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
        // if (!Config::enableLocale) return TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length);
        if (!text) TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length);

        static auto Substring = Il2cppUtils::GetMethod("mscorlib.dll", "System", "String", "Substring",
                                                       {"System.Int32", "System.Int32"});

        const std::string origText = Substring->Invoke<Il2cppString*>(text, start, length)->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            UpdateTMPFont(self);
            return TMP_Text_PopulateTextBackingArray_Orig(self, newText, 0, newText->length);
        }

        if (Config::textTest) {
            Log::VerboseFmt("[TP] %s", text->ToString().c_str());
            TMP_Text_PopulateTextBackingArray_Orig(self, UnityResolve::UnityType::String::New("[TP]" + text->ToString()), start, length + 4);
        } else {
            TMP_Text_PopulateTextBackingArray_Orig(self, text, start, length);
        }
        UpdateTMPFont(self);
    }

    DEFINE_HOOK(void, TMP_Text_SetText_2, (void* self, Il2cppString* sourceText, bool syncTextInputBox, void* mtd)) {
        // if (!Config::enableLocale) return TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
        if (!sourceText) {
            return TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
        }
        const std::string origText = sourceText->ToString();
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            UpdateTMPFont(self);
            return TMP_Text_SetText_2_Orig(self, newText, syncTextInputBox, mtd);
        }
        if (Config::textTest) {
            Log::VerboseFmt("[TS] %s", sourceText->ToString().c_str());
            TMP_Text_SetText_2_Orig(self, UnityResolve::UnityType::String::New("[TS]" + sourceText->ToString()), syncTextInputBox, mtd);
        } else {
            TMP_Text_SetText_2_Orig(self, sourceText, syncTextInputBox, mtd);
        }
        UpdateTMPFont(self);
    }

    DEFINE_HOOK(void, TextMeshProUGUI_Awake, (void* self, void* method)) {
        // if (!Config::enableLocale) return TextMeshProUGUI_Awake_Orig(self, method);
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
                set_Text_method->Invoke<void>(self, UnityResolve::UnityType::String::New(transText));
                UpdateTMPFont(self);
                TextMeshProUGUI_Awake_Orig(self, method);
                return;
            }
            if (Config::textTest) {
                Log::VerboseFmt("[TA] %s", currText->ToString().c_str());
                set_Text_method->Invoke<void>(self, UnityResolve::UnityType::String::New("[TA]" + currText->ToString()));
            }
            else {
                set_Text_method->Invoke<void>(self, UnityResolve::UnityType::String::New(currText->ToString()));
            }
        }

        // set_font->Invoke<void>(self, font);
        UpdateTMPFont(self);
        TextMeshProUGUI_Awake_Orig(self, method);
    }

    DEFINE_HOOK(void, Text_set_text, (void* self, Il2cppString* sourceText, void* mtd)) {
        // if (!Config::enableLocale) return Text_set_text_Orig(self, sourceText, mtd);
        if (!sourceText) return Text_set_text_Orig(self, sourceText, mtd);
        Log::DebugFmt("Text_set_text Hooked, %s", sourceText->ToString().c_str());
        // 特判时间
        std::string origText = sourceText->ToString();
        RE2 time(R"((\d{1,2}:\d{1,2})|\d+)");
        Log::DebugFmt("Ready to match");
        if (RE2::FullMatch(origText, time)) return Text_set_text_Orig(self, sourceText, mtd);
        Log::DebugFmt("Trying to get translation");
        std::string transText;
        if (Local::GetGenericText(origText, &transText)) {
            const auto newText = UnityResolve::UnityType::String::New(transText);
            return Text_set_text_Orig(self, newText, mtd);
        }
        if (Config::textTest) {
            Log::VerboseFmt("[TU] %s", sourceText->ToString().c_str());
            Text_set_text_Orig(self,  UnityResolve::UnityType::String::New("[TU]" + sourceText->ToString()), mtd);
        } else {
            Text_set_text_Orig(self, sourceText, mtd);
        }
//        UpdateFont(self);
    }

    // TODO 文本未hook完整
    DEFINE_HOOK(void, TextField_set_value, (void* self, Il2cppString* value)) {
        Log::DebugFmt("TextField_set_value: %s", value->ToString().c_str());
        TextField_set_value_Orig(self, value);
    }
    // maybe we find a better way to resolve master database
//    DEFINE_HOOK(void*, CardSkillsMaster_Fetch , (void* self, int64_t id)) {
//        Log::DebugFmt("CardSkillsMaster_Fetch Hooked");
//        auto result = CardSkillsMaster_Fetch_Orig(self, id);
//        return result;
//    }
//
//    DEFINE_HOOK(void, CardSkillsMaster_FetchAll, (void* self, void* mtd)) {
//        Log::DebugFmt("CardSkillsMaster_FetchAll Hooked");
//        CardSkillsMaster_FetchAll_Orig(self, mtd);
//        static auto Masterbase_klass = Il2cppUtils::GetClass("Core.dll", "Silverflame.SFL", "CardSkillsMaster");
//        static auto allValues_field = Masterbase_klass->Get<UnityResolve::Field>("allValues"); // List
//        static auto cache_field = Masterbase_klass->Get<UnityResolve::Field>("cache"); // Dictionary
//        auto cache = Il2cppUtils::ClassGetFieldValue<void*>(self, cache_field);
//        Log::DebugFmt("cache is at %p", cache);
//    }


    void Install(HookInstaller* hookInstaller) {
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
        ADD_HOOK(Text_set_text, Il2cppUtils::GetMethodPointer("UnityEngine.UI.dll", "UnityEngine.UI", "Text", "set_text"));
//        ADD_HOOK(CardSkillsMaster_Fetch, Il2cppUtils::GetMethodPointer("Core.dll", "Silverflame.SFL",
//                                                                            "CardSkillsMaster", "Fetch"));
//        ADD_HOOK(CardSkillsMaster_FetchAll, Il2cppUtils::GetMethodPointer("Core.dll", "Silverflame.SFL",
//                                                                          "CardSkillsMaster", "FetchAll"));
    }
} // LinkuraLocal