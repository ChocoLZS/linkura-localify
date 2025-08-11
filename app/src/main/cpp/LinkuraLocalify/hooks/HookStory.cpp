#include "../HookMain.h"
#include "../config/Config.hpp"
#include <re2/re2.h>
#include <string>
#include "../camera/camera.hpp"

namespace LinkuraLocal::HookStory {
    // 简洁的 RE2 替换函数
    auto regex_replace = [](std::string input, const char* pattern, const char* replacement) -> std::string {
        RE2 re(pattern);
        if (!re.ok()) {
            LinkuraLocal::Log::ErrorFmt("RE2 compile failed for pattern: %s, error: %s", pattern, re.error().c_str());
            return input;
        }
        
        int count = RE2::GlobalReplace(&input, re, replacement);
        LinkuraLocal::Log::VerboseFmt("RE2 replace success for pattern: %s, replacements: %d", pattern, count);
        return input;
    };

    DEFINE_HOOK(Il2cppUtils::Il2CppString*, StoryScene_LoadStoryData, (Il2cppUtils::Il2CppString* fileName, void* mtd) ) {
        Log::DebugFmt("StoryScene_LoadStoryData HOOKED, %s", fileName->ToString().c_str());
        auto content = StoryScene_LoadStoryData_Orig(fileName, mtd);
        auto content_str = content->ToString();
        if (Config::storyHideBackground) {
            content_str = regex_replace(content_str, "#?\\[?背景(表示|移動|回転)[^\\n]*\\n", "");
            content_str = regex_replace(content_str, "[^\\n]*runbg[^\\n]*\\n", "");
        }

        if (Config::storyHideTransition) {
            content_str = regex_replace(content_str, "[^\\n]*暗転_イン[^\\n]*\\n", "");
            content_str = regex_replace(content_str, "[^\\n]*###[^\\n]*\\n", "");
        }

        if (Config::storyHideNonCharacter3d) {
            content_str = regex_replace(content_str, "#[^#]*3Dオブジェクト[^\\n]*\\n", "");
        }

        if (Config::storyHideDof) {
            content_str = regex_replace(content_str, "\\[?被写界深度[^\\n]*\\n", "");
        }

        if (Config::storyHideEffect) {
            content_str = regex_replace(content_str, "\\[?プリセットポストエフェクト[^\\n]*\\n", "");
        }

        content = Il2cppUtils::Il2CppString::New(content_str);
        return content;
    }

    DEFINE_HOOK(void, StoryScene_SetStory, (void* self, void* story, void* mtd)) {
        Log::DebugFmt("StoryScene_SetStory HOOKED");
        L4Camera::clearRenderSet();
        StoryScene_SetStory_Orig(self, story, mtd);
    }

    DEFINE_HOOK(void*, StoryNovelView_AddTextAsync, (void* self, Il2cppUtils::Il2CppString* text, void* rubis, float durationSec, bool shouldTapWait, bool addNewLine, void* mtd)) {
        return StoryNovelView_AddTextAsync_Orig(self, text, rubis, durationSec, shouldTapWait, addNewLine, mtd);
    }

    DEFINE_HOOK(int32_t, AddNovelTextCommand_DoExecute, (void* self,void* mnemonic, bool isFirstClock, bool skip, float commandPlayedTime, bool seekbar, void* mtd)) {
        return AddNovelTextCommand_DoExecute_Orig(self, mnemonic, isFirstClock, skip, commandPlayedTime, seekbar, mtd);
    }

    // works
    DEFINE_HOOK(float, AddNovelTextCommand_GetDisplayTime, (void* mnemonic, void* mtd)) {
        static auto AddNovelTextCommand_klass = Il2cppUtils::GetClass("Assembly-CSharp.dll", "Tecotec", "AddNovelTextCommand");
        static auto AddNovelTextCommand_GetText = AddNovelTextCommand_klass->Get<UnityResolve::Method>("GetText");
        static auto AddNovelTextCommand_HasVoice = AddNovelTextCommand_klass->Get<UnityResolve::Method>("HasVoice");
        auto durationSec = AddNovelTextCommand_GetDisplayTime_Orig(mnemonic, mtd);
        auto textTuple = AddNovelTextCommand_GetText->Invoke<UnityResolve::UnityType::ValueTuple<Il2cppUtils::Il2CppString *, void*>>(mnemonic);

        auto text = textTuple.Item1;

        if (text) {
            auto originDurationSec = durationSec;
            auto text_str = text->ToString();
            bool hasVoice = AddNovelTextCommand_HasVoice->Invoke<bool>(mnemonic);
            if (hasVoice) {
                Log::VerboseFmt("Vocal text is %s", text_str.c_str());
                durationSec = durationSec * Config::storyNovelVocalTextDurationRate;
            } else {
                Log::VerboseFmt("Text is %s", text_str.c_str());
                durationSec = durationSec * Config::storyNovelNonVocalTextDurationRate;
            }
            Log::DebugFmt("GetDisplayTime: text = %s, origin duration = %f, duration = %f", text->ToString().c_str(), originDurationSec, durationSec);
        }
        
        return durationSec;
    }


    void Install(HookInstaller* hookInstaller) {
#pragma region Story
//        ADD_HOOK(StoryModelSpace_GetCamera, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryModelSpace", "get_StoryCamera"));
        ADD_HOOK(StoryScene_LoadStoryData, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "LoadStoryData"));
//        ADD_HOOK(StoryScene_SetCameraColor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "SetCameraColor"));
        ADD_HOOK(StoryScene_SetStory, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "SetStory"));
//        ADD_HOOK(StoryScriptConverter_AdvScriptFileToMnemonics, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScriptConverter", "AdvScriptFileToMnemonics"));
//        ADD_HOOK(StorySystem_OnInitialize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StorySystem", "OnInitialize"));
//        ADD_HOOK(StoryScene_SetCameraColor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryScene", "SetCameraColor"));
//        ADD_HOOK(StoryModelSpaceManager_get_modelSpace, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StoryModelSpaceManager", "get_modelSpace"));
//        ADD_HOOK(StoryNovelView_ChangeSpeed, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.Story", "NovelView", "ChangeSpeed"));
//        ADD_HOOK(StoryNovelText_SetText, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School", "NovelText", "SetText", {"System.String", "School.Ruby[]"}));
//        ADD_HOOK(StoryNovelView_ShowAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.Story", "NovelView", "ShowAsync"));
//        ADD_HOOK(StoryNovelView_SetNextTimeScale, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.Story", "NovelView", "SetNextTimeScale"));
//        ADD_HOOK(AnimatableText_set_Text, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School", "AnimatableText", "set_Text"));
//        ADD_HOOK(StoryNovelView_PauseTextAnimation, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.Story", "NovelView", "PauseTextAnimation"));
//        ADD_HOOK(StoryNovelView_ctor, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.Story", "NovelView", ".ctor"));
        ADD_HOOK(StoryNovelView_AddTextAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "School.Story", "NovelView", "AddTextAsync"));
        ADD_HOOK(AddNovelTextCommand_DoExecute, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "AddNovelTextCommand", "DoExecute"));
        ADD_HOOK(AddNovelTextCommand_GetDisplayTime, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "AddNovelTextCommand", "GetDisplayTime"));
//        ADD_HOOK(Tween_UpdateDelay, Il2cppUtils::GetMethodPointer("DOTween.dll", "DG.Tweening", "Tween", "UpdateDelay"));
//        ADD_HOOK(TweeningDOTween_To, Il2cppUtils::GetMethodPointer("DOTween.dll", "DG.Tweening", "DOTween", "To", {"DG.Tweening.Core.DOGetter", "DG.Tweening.Core.DOSetter", "System.Single", "System.Single"}));
//        ADD_HOOK(StorySystem_Pause, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Tecotec", "StorySystem", "Pause"));
#pragma endregion
    }
}