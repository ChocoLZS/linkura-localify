#include "Log.h"
#include "Misc.hpp"
#include <sstream>
#include <string>
#include <thread>
#include <queue>
#include <cstdarg>

#ifndef GKMS_WINDOWS
    #include <android/log.h>

    extern JavaVM* g_javaVM;
    extern jclass g_linkuraHookMainClass;
    extern jmethodID showToastMethodId;
#endif // GKMS_WINDOWS


#define GetParamStringResult(name)\
    va_list args;\
    va_start(args, fmt);\
    va_list args_copy;\
    va_copy(args_copy, args);\
    int size = vsnprintf(nullptr, 0, fmt, args_copy) + 1;\
    va_end(args_copy);\
    char* buffer = new char[size];\
    vsnprintf(buffer, size, fmt, args);\
    va_end(args);\
    std::string name(buffer);\
    delete[] buffer


namespace LinkuraLocal::Log {
    namespace {
        std::queue<std::string> showingToasts{};
    }

    std::string StringFormat(const char* fmt, ...) {
        GetParamStringResult(result);
        return result;
    }

    void Log(int prio, const char* msg) {
        __android_log_write(prio, "LinkuraLocal-Native", msg);
    }

    void LogFmt(int prio, const char* fmt, ...) {
        GetParamStringResult(result);
        Log(prio, result.c_str());
    }

    void Info(const char* msg) {
        Log(ANDROID_LOG_INFO, msg);
    }

    void InfoFmt(const char* fmt, ...) {
        GetParamStringResult(result);
        Info(result.c_str());
    }

    void Error(const char* msg) {
        Log(ANDROID_LOG_ERROR, msg);
    }

    void ErrorFmt(const char* fmt, ...) {
        GetParamStringResult(result);
        Error(result.c_str());
    }

    void Debug(const char* msg) {
        Log(ANDROID_LOG_DEBUG, msg);
    }

    void DebugFmt(const char* fmt, ...) {
        GetParamStringResult(result);
        Debug(result.c_str());
    }

    void Verbose(const char* msg) {
        Log(ANDROID_LOG_VERBOSE, msg);
    }

    void VerboseFmt(const char* fmt, ...) {
        GetParamStringResult(result);
        Verbose(result.c_str());
    }

    void LogUnityLog(int prio, const char* fmt, ...) {
        GetParamStringResult(result);
        __android_log_write(prio, "LinkuraLog", result.c_str());
    }

    /*
    void ShowToastJNI(const char* text) {
        DebugFmt("Toast: %s", text);

        std::thread([text](){
            auto env = Misc::GetJNIEnv();
            if (!env) {
                return;
            }

            jclass& kotlinClass = g_linkuraHookMainClass;
            if (!kotlinClass) {
                g_javaVM->DetachCurrentThread();
                return;
            }
            jmethodID& methodId = showToastMethodId;
            if (!methodId) {
                g_javaVM->DetachCurrentThread();
                return;
            }
            jstring param = env->NewStringUTF(text);
            env->CallStaticVoidMethod(kotlinClass, methodId, param);

            g_javaVM->DetachCurrentThread();
        }).detach();
    }*/


    void ShowToast(const std::string& text) {
#ifndef GKMS_WINDOWS
        showingToasts.push(text);
#else
		InfoFmt("Toast: %s", text.c_str());
#endif
    }

    void ShowToast(const char* text) {
        // DebugFmt("Toast: %s", text);
        return ShowToast(std::string(text));
    }

    void ShowToastFmt(const char* fmt, ...) {
        GetParamStringResult(result);
        ShowToast(result);
    }

    std::string GetQueuedToast() {
        if (showingToasts.empty()) {
            return "";
        }
        const auto ret = showingToasts.front();
        showingToasts.pop();
        return ret;
    }

#ifndef GKMS_WINDOWS
    void ToastLoop(JNIEnv *env, jclass clazz) {
        const auto toastString = GetQueuedToast();
        if (toastString.empty()) return;

        static auto _showToastMethodId = env->GetStaticMethodID(clazz, "showToast", "(Ljava/lang/String;)V");

        if (env && clazz && _showToastMethodId) {
            jstring param = env->NewStringUTF(toastString.c_str());
            env->CallStaticVoidMethod(clazz, _showToastMethodId, param);
            env->DeleteLocalRef(param);
        }
        else {
            _showToastMethodId = env->GetStaticMethodID(clazz, "showToast", "(Ljava/lang/String;)V");
        }
    }
#endif

}
