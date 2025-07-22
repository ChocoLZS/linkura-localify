#include "LinkuraLocalify/Plugin.h"
#include "LinkuraLocalify/Log.h"
#include "LinkuraLocalify/Local.h"
#include "LinkuraLocalify/Hook.h"

#include <jni.h>
#include <android/log.h>
#include "string"
#include "shadowhook.h"
#include "xdl.h"
#include "LinkuraLocalify/camera/camera.hpp"
#include "LinkuraLocalify/config/Config.hpp"
#include "Joystick/JoystickEvent.h"
#include "build/linkura_messages.pb.h"

JavaVM* g_javaVM = nullptr;
jclass g_linkuraHookMainClass = nullptr;
jmethodID showToastMethodId = nullptr;

bool UnityResolveProgress::startInit = false;
UnityResolveProgress::Progress UnityResolveProgress::assembliesProgress{};
UnityResolveProgress::Progress UnityResolveProgress::classProgress{};

namespace
{
    class AndroidHookInstaller : public LinkuraLocal::HookInstaller
    {
    public:
        explicit AndroidHookInstaller(const std::string& il2cppLibraryPath, const std::string& localizationFilesDir)
                : m_Il2CppLibrary(xdl_open(il2cppLibraryPath.c_str(), RTLD_LAZY))
        {
            this->m_il2cppLibraryPath = il2cppLibraryPath;
            this->localizationFilesDir = localizationFilesDir;
        }

        ~AndroidHookInstaller() override {
            xdl_close(m_Il2CppLibrary);
        }

        void* InstallHook(void* addr, void* hook, void** orig) override
        {
            return shadowhook_hook_func_addr(addr, hook, orig);
        }

        LinkuraLocal::OpaqueFunctionPointer LookupSymbol(const char* name) override
        {
            return reinterpret_cast<LinkuraLocal::OpaqueFunctionPointer>(xdl_sym(m_Il2CppLibrary, name, NULL));
        }

    private:
        void* m_Il2CppLibrary;
    };
}

extern "C"
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_javaVM = vm;
    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_initHook(JNIEnv *env, jclass clazz, jstring targetLibraryPath,
                                                                 jstring localizationFilesDir) {
    g_linkuraHookMainClass = clazz;
    showToastMethodId = env->GetStaticMethodID(clazz, "showToast", "(Ljava/lang/String;)V");

    const auto targetLibraryPathChars = env->GetStringUTFChars(targetLibraryPath, nullptr);
    const std::string targetLibraryPathStr = targetLibraryPathChars;

    const auto localizationFilesDirChars = env->GetStringUTFChars(localizationFilesDir, nullptr);
    const std::string localizationFilesDirCharsStr = localizationFilesDirChars;

    auto& plugin = LinkuraLocal::Plugin::GetInstance();
    plugin.InstallHook(std::make_unique<AndroidHookInstaller>(targetLibraryPathStr, localizationFilesDirCharsStr));
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_keyboardEvent(JNIEnv *env, jclass clazz, jint key_code, jint action) {
    L4Camera::on_cam_rawinput_keyboard(action, key_code);
    const auto msg = LinkuraLocal::Local::OnKeyDown(action, key_code);
    if (!msg.empty()) {
        g_linkuraHookMainClass = clazz;
        showToastMethodId = env->GetStaticMethodID(clazz, "showToast", "(Ljava/lang/String;)V");

        if (env && clazz && showToastMethodId) {
            jstring param = env->NewStringUTF(msg.c_str());
            env->CallStaticVoidMethod(clazz, showToastMethodId, param);
            env->DeleteLocalRef(param);
        }
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_joystickEvent(JNIEnv *env, jclass clazz,
                                                                      jint action,
                                                                      jfloat leftStickX,
                                                                      jfloat leftStickY,
                                                                      jfloat rightStickX,
                                                                      jfloat rightStickY,
                                                                      jfloat leftTrigger,
                                                                      jfloat rightTrigger,
                                                                      jfloat hatX,
                                                                      jfloat hatY) {
    JoystickEvent event(action, leftStickX, leftStickY, rightStickX, rightStickY, leftTrigger, rightTrigger, hatX, hatY);
    L4Camera::on_cam_rawinput_joystick(event);
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_loadConfig(JNIEnv *env, jclass clazz,
                                                                   jstring config_json_str) {
    const auto configJsonStrChars = env->GetStringUTFChars(config_json_str, nullptr);
    const std::string configJson = configJsonStrChars;
    LinkuraLocal::Config::LoadConfig(configJson);
}

extern "C"
JNIEXPORT jint JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_pluginCallbackLooper(JNIEnv *env,
                                                                             jclass clazz) {
    LinkuraLocal::Log::ToastLoop(env, clazz);

    if (UnityResolveProgress::startInit) {
        return 9;
    }
    return 0;
}


extern "C"
JNIEXPORT void JNICALL
Java_io_github_chocolzs_linkura_localify_models_NativeInitProgress_pluginInitProgressLooper(
        JNIEnv *env, jclass clazz, jobject progress) {

    // jclass progressClass = env->GetObjectClass(progress);

    static jfieldID startInitFieldID = env->GetStaticFieldID(clazz, "startInit", "Z");

    static jmethodID setAssembliesProgressDataMethodID = env->GetMethodID(clazz, "setAssembliesProgressData", "(JJ)V");
    static jmethodID setClassProgressDataMethodID = env->GetMethodID(clazz, "setClassProgressData", "(JJ)V");

    // jboolean startInit = env->GetStaticBooleanField(clazz, startInitFieldID);

    env->SetStaticBooleanField(clazz, startInitFieldID, UnityResolveProgress::startInit);

    env->CallVoidMethod(progress, setAssembliesProgressDataMethodID,
                        UnityResolveProgress::assembliesProgress.current, UnityResolveProgress::assembliesProgress.total);
    env->CallVoidMethod(progress, setClassProgressDataMethodID,
                        UnityResolveProgress::classProgress.current, UnityResolveProgress::classProgress.total);

}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_getCameraInfoProtobuf(JNIEnv *env, jclass clazz) {
    try {
        std::vector<uint8_t> protobufData = LinkuraLocal::HookCamera::getCameraInfoProtobuf();
        
        jbyteArray result = env->NewByteArray(protobufData.size());
        env->SetByteArrayRegion(result, 0, protobufData.size(), 
                               reinterpret_cast<const jbyte*>(protobufData.data()));
        return result;
    } catch (const std::exception& e) {
        // Return empty byte array if something goes wrong
        jbyteArray result = env->NewByteArray(0);
        return result;
    } catch (...) {
        jbyteArray result = env->NewByteArray(0);
        return result;
    }
}

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_getCurrentArchiveInfo(JNIEnv *env, jclass clazz) {
    try {
        LinkuraLocal::Log::DebugFmt("getCurrentArchiveInfo jni called");
        std::vector<uint8_t> protobufData = LinkuraLocal::HookLiveRender::getCurrentArchiveInfo();

        jbyteArray result = env->NewByteArray(protobufData.size());
        env->SetByteArrayRegion(result, 0, protobufData.size(),
                                reinterpret_cast<const jbyte*>(protobufData.data()));
        return result;
    } catch (const std::exception& e) {
        jbyteArray result = env->NewByteArray(0);
        return result;
    } catch (...) {
        jbyteArray result = env->NewByteArray(0);
        return result;
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_setArchivePosition(JNIEnv *env, jclass clazz,
                                                                                   jfloat seconds) {
    try {
        LinkuraLocal::Log::DebugFmt("setArchivePosition: Received request to set position to %f seconds", seconds);

        LinkuraLocal::HookLiveRender::setArchivePosition(seconds);
        
        LinkuraLocal::Log::Info("Archive position set successfully");
        
    } catch (const std::exception& e) {
        LinkuraLocal::Log::ErrorFmt("Error in setArchivePosition: %s", e.what());
    } catch (...) {
        LinkuraLocal::Log::Error("Unknown error in setArchivePosition");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_io_github_chocolzs_linkura_localify_LinkuraHookMain_updateConfig(JNIEnv *env, jclass clazz,
                                                                     jbyteArray config_update_data) {
    try {
        jsize dataSize = env->GetArrayLength(config_update_data);
        if (dataSize == 0) {
            return;
        }
        
        jbyte* dataElements = env->GetByteArrayElements(config_update_data, nullptr);
        if (dataElements == nullptr) {
            return;
        }
        
        // Parse protobuf data
        linkura::ipc::ConfigUpdate configUpdate;
        if (!configUpdate.ParseFromArray(dataElements, dataSize)) {
            env->ReleaseByteArrayElements(config_update_data, dataElements, JNI_ABORT);
            LinkuraLocal::Log::Error("Failed to parse config update protobuf");
            return;
        }
        
        env->ReleaseByteArrayElements(config_update_data, dataElements, JNI_ABORT);
        
        // Apply configuration updates
        LinkuraLocal::Config::UpdateConfig(configUpdate);
        
        LinkuraLocal::Log::Info("Config hot-reload applied successfully");
        
    } catch (const std::exception& e) {
        LinkuraLocal::Log::ErrorFmt("Error in updateConfig: %s", e.what());
    } catch (...) {
        LinkuraLocal::Log::Error("Unknown error in updateConfig");
    }
}