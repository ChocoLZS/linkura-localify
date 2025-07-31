#include "../HookMain.h"
#include "../config/Config.hpp"
#include <re2/re2.h>
#include <string>

namespace LinkuraLocal::HookStory {
    // 简洁的 RE2 替换函数
    auto regex_replace = [](std::string input, const char* pattern, const char* replacement) -> std::string {
        RE2 re(pattern);
        if (!re.ok()) {
            LinkuraLocal::Log::DebugFmt("RE2 compile failed for pattern: %s, error: %s", pattern, re.error().c_str());
            return input;
        }
        
        int count = RE2::GlobalReplace(&input, re, replacement);
        LinkuraLocal::Log::DebugFmt("RE2 replace success for pattern: %s, replacements: %d", pattern, count);
        return input;
    };

    DEFINE_HOOK(Il2cppUtils::Il2CppString*, StoryScene_LoadStoryData, (Il2cppUtils::Il2CppString* fileName, void* mtd) ) {
        Log::DebugFmt("StoryScene_LoadStoryData HOOKED, %s", fileName->ToString().c_str());
        auto content = StoryScene_LoadStoryData_Orig(fileName, mtd);
        auto content_str = content->ToString();
        if (Config::storyHideBackground) {
            content_str = regex_replace(content_str, "#?\\[?背景(表示|移動|回転)[^\\n]*\\n", ""); // 隐藏背景
            content_str = regex_replace(content_str, "[^\\n]*runbg[^\\n]*\\n", ""); // 隐藏背景
        }

        if (Config::storyHideTransition) {
            content_str = regex_replace(content_str, "[^\\n]*暗転_イン[^\\n]*\\n", ""); // 隐藏过渡
            content_str = regex_replace(content_str, "[^\\n]*###[^\\n]*\\n", ""); // 隐藏过渡
        }

        if (Config::storyHideNonCharacter3d) {
            content_str = regex_replace(content_str, "#[^#]*3Dオブジェクト表示[^\\n]*\\n", "");         // 隐藏非角色3d
        }

        if (Config::storyHideDof) {
            content_str = regex_replace(content_str, "\\[?被写界深度[^\\n]*\\n", ""); // 隐藏景深
        }
        content = Il2cppUtils::Il2CppString::New(content_str);
        return content;
    }

    DEFINE_HOOK(void, StoryScene_SetStory, (void* self, void* story, void* mtd)) {
        Log::DebugFmt("StoryScene_SetStory HOOKED");
        StoryScene_SetStory_Orig(self, story, mtd);
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
#pragma endregion
    }
}