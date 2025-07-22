#pragma once
#include "../HookMain.h"

namespace LinkuraLocal::HookShare {

#pragma region HttpRequests
    uintptr_t ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = 0;
    DEFINE_HOOK(void* , ApiClient_Deserialize, (void* self, void* response, void* type, void* method_info)) {
        auto result = ApiClient_Deserialize_Orig(self, response, type, method_info);
        auto json = nlohmann::json::parse(Il2cppUtils::ToJsonStr(result)->ToString());
        auto caller = __builtin_return_address(0);
        IF_CALLER_WITHIN(ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr, caller, 3000) {
            if (Config::fesArchiveUnlockTicket) {
                Log::DebugFmt(
                        "ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext HOOKED");
                json["selectable_camera_types"] = {1,2,3,4};
                json["ticket_rank"] = 6;
                json["has_extra_admission"] = "true";
                result = Il2cppUtils::FromJsonStr(json.dump(), type);
            }
        }

        return result;
    }

#pragma endregion

    void Install(HookInstaller* hookInstaller) {

        // GetHttpAsyncAddr
        ADD_HOOK(ApiClient_Deserialize, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Org.OpenAPITools.Client","ApiClient", "Deserialize"));
        auto ArchiveApi_klass = Il2cppUtils::GetClassIl2cpp("Assembly-CSharp.dll", "Org.OpenAPITools.Api", "ArchiveApi");
        if (ArchiveApi_klass) {
            // hook /v1/archive/get_fes_archive_data response
            auto ArchiveGetFesArchiveDataWithHttpInfoAsync_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, "<ArchiveGetFesArchiveDataWithHttpInfoAsync>d__30");
            auto method = Il2cppUtils::GetMethodIl2cpp(ArchiveGetFesArchiveDataWithHttpInfoAsync_klass, "MoveNext", 0);
            if (method) {
                ArchiveApi_ArchiveGetFesArchiveDataWithHttpInfoAsync_MoveNext_Addr = method->methodPointer;
            }
        }
    }
}
