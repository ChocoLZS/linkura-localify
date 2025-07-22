#ifndef LINKURA_LOCALIFY_LOG_H
#define LINKURA_LOCALIFY_LOG_H

#include "../platformDefine.hpp"

#include <string>

#ifndef GKMS_WINDOWS
    #include <jni.h>
#endif


namespace LinkuraLocal::Log {
    std::string StringFormat(const char* fmt, ...);
    void LogUnityLog(int prio, const char* fmt, ...);
    void LogFmt(int prio, const char* fmt, ...);
    void Info(const char* msg);
    void InfoFmt(const char* fmt, ...);
    void Error(const char* msg);
    void ErrorFmt(const char* fmt, ...);
    void Debug(const char* msg);
    void DebugFmt(const char* fmt, ...);
    void Verbose(const char* msg);
    void VerboseFmt(const char* fmt, ...);

    void ShowToast(const char* text);
    void ShowToastFmt(const char* fmt, ...);

#ifndef GKMS_WINDOWS
    void ToastLoop(JNIEnv *env, jclass clazz);
#endif
}

#endif //LINKURA_LOCALIFY_LOG_H
