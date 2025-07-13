#include "shadowhook.h"
#include <android/log.h>

#define ADD_HOOK(name, addr)                                                                       \
	name##_Addr = reinterpret_cast<name##_Type>(addr);                                             \
	if (addr) {                                                                                    \
    	auto stub = hookInstaller->InstallHook(reinterpret_cast<void*>(addr),                      \
                                               reinterpret_cast<void*>(name##_Hook),               \
                                               reinterpret_cast<void**>(&name##_Orig));            \
        if (stub == NULL) {                                                                        \
            int error_num = shadowhook_get_errno();                                                \
            const char *error_msg = shadowhook_to_errmsg(error_num);                               \
            Log::ErrorFmt("ADD_HOOK: %s at %p failed: %s", #name, addr, error_msg);                \
        }                                                                                          \
        else {                                                                                     \
            hookedStubs.emplace(stub);                                                             \
            LinkuraLocal::Log::InfoFmt("ADD_HOOK: %s at %p", #name, addr);                         \
        }                                                                                          \
    }                                                                                              \
    else LinkuraLocal::Log::ErrorFmt("Hook failed: %s is NULL", #name, addr);                      \
    if (Config::lazyInit) UnityResolveProgress::classProgress.current++

#define DEFINE_HOOK(returnType, name, params)                                                      \
	using name##_Type = returnType(*) params;                                                      \
	name##_Type name##_Addr = nullptr;                                                             \
	name##_Type name##_Orig = nullptr;                                                             \
	returnType name##_Hook params

#include <cstdint> // For uintptr_t
// Assuming you have a logging header like this:
// #include "YourLogHeader.h"
// For example, if you have a class Log with a static method DebugFmt:
// namespace YourNamespace {
//     class Log {
//     public:
//         static void DebugFmt(const char* fmt, ...);
//     };
// }
// If your Log namespace/class is different, adjust LOG_DEBUG_FMT below.

#define IF_CALLER_WITHIN(func_start_addr, caller_actual_addr, size_range) \
    /* The actual 'if' for the user's code block */ \
    if (\
        (reinterpret_cast<uintptr_t>(caller_actual_addr) > reinterpret_cast<uintptr_t>(func_start_addr)) && \
        (reinterpret_cast<uintptr_t>(caller_actual_addr) - reinterpret_cast<uintptr_t>(func_start_addr)) < static_cast<uintptr_t>(size_range))

//#define GET_ASYNC_MOVENEXT_ADDR(assemblyName, nameSpaceName, className, nestedClass, ptr) \
//    auto className##_klass = Il2cppUtils::GetClass(assemblyName, nameSpaceName, className); \
//    if (className##_klass) { \
//        auto className##_method##_klass = Il2cppUtils::find_nested_class_from_name(ArchiveApi_klass, nestedClass); \
//        Log::DebugFmt("%s name is: %s", #nestedClass, static_cast<Il2cppUtils::Il2CppClassHead*>(className##_method##_klass)->name); \
//        auto method = Il2cppUtils::find_method_from_name(className##_method##_klass, "MoveNext", 0);               \
//        if (method) {                                                                      \
//            ptr = method->methodPointer;                                                              \
//        }                                                                                       \
//    }