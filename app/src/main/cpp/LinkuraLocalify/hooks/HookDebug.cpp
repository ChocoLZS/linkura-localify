#include "../HookMain.h"
#include <chrono>
#include <thread>
#include <set>
#include <map>
#include <vector>
#include <future>
#include <atomic>

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

    DEFINE_HOOK(void, FootShadowManipulator_OnInstantiate, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("FootShadowManipulator_OnInstantiate HOOKED");
        if (Config::hideCharacterShadow) return;
        FootShadowManipulator_OnInstantiate_Orig(self, method);
    }

    // character　item
    DEFINE_HOOK(void, ItemManipulator_OnInstantiate, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("ItemManipulator_OnInstantiate HOOKED");
        if (Config::hideLiveStreamCharacterItems) return;
        ItemManipulator_OnInstantiate_Orig(self, method);
    }

    enum HideLiveStreamSceneItemMode {
        None,
        /**
         * Simply hide live scene item, useful for with meets.\n
         */
        Lite,
        /**
         * Simply hide live scene item, useful for with meets.\n
         * Will remove static items for fes live.
         * And will try to hide static scene object
         */
        Normal,
        /**
         * Will try to remove dynamic items for fes live.
         */
        Strong,
        /**
         * Will remove timeline for fes live, light render, position control, dynamic camera will be useless.
         */
        Ultimate
    };

    // For example: Whiteboard, photo in with meets
    DEFINE_HOOK(void, ScenePropManipulator_OnInstantiate, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("ScenePropManipulator_OnInstantiate HOOKED");
        if (Config::hideLiveStreamSceneItemsLevel >= HideLiveStreamSceneItemMode::Lite) return;
        ScenePropManipulator_OnInstantiate_Orig(self, method);
    }

    static void hideGameObjectRecursive(UnityResolve::UnityType::GameObject* gameObject, int current_level, int max_level,
                                 const std::string& prefix = "", std::set<void*>* visited = nullptr, bool debug = true) {
        if (!gameObject || current_level > max_level) {
            return;
        }

        // 防止无限递归
        std::set<void*> local_visited;
        if (!visited) {
            visited = &local_visited;
        }

        void* gameObjectPtr = static_cast<void*>(gameObject);
        if (visited->find(gameObjectPtr) != visited->end()) {
            if (debug) Log::VerboseFmt("%s[L%d] GameObject: %s (ALREADY_VISITED - skipping)", prefix.c_str(), current_level, gameObject->GetName().c_str());
            return;
        }
        visited->insert(gameObjectPtr);

        auto objName = gameObject->GetName();
        auto transform = gameObject->GetTransform();

        // 递归处理子对象 - 深度优先，先隐藏子节点
        if (current_level < max_level && transform) {
            const auto childCount = transform->GetChildCount();
            if (childCount > 0) {
                for (int i = 0; i < childCount; i++) {
                    auto childTransform = transform->GetChild(i);
                    if (childTransform) {
                        auto childGameObject = childTransform->GetGameObject();
                        if (childGameObject && childGameObject != gameObject) { // 避免自引用
                            std::string newPrefix = prefix + "  ";
                            hideGameObjectRecursive(childGameObject, current_level + 1, max_level, newPrefix, visited, debug);
                        }
                    }
                }
            }
        }

        // 隐藏当前GameObject (在隐藏子节点之后)
        if (transform) {
            if (debug) Log::VerboseFmt("%s[L%d] Hiding GameObject: %s", prefix.c_str(), current_level, objName.c_str());
            Il2cppUtils::SetTransformRenderActive(transform, false, objName, debug);
        }
    }

    // return struct value, should use &scene as ptr.
    DEFINE_HOOK(void*, SceneManager_GetSceneByName, (Il2cppString * sceneName, void* method)) {
        Log::DebugFmt("SceneManager_GetSceneByName HOOKED: %s", sceneName->ToString().c_str());
        auto scene = SceneManager_GetSceneByName_Orig(sceneName, method);
        Log::DebugFmt("SceneManager_GetSceneByName HOOKED: %s Finished", sceneName->ToString().c_str());
        static auto Scene_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine.SceneManagement", "Scene");
        static auto Scene_getRootGameObjects = Scene_klass->Get<UnityResolve::Method>("GetRootGameObjects", {});
        static auto Transform_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
        if (sceneName->ToString().starts_with("3d_stage")) {
//            Log::DebugFmt("Scene_getRootGameObjects is at %p", Scene_getRootGameObjects);
            if (Scene_getRootGameObjects) {
                auto gameObjects = Scene_getRootGameObjects->Invoke<UnityResolve::UnityType::Array<UnityResolve::UnityType::GameObject*>*>(&scene);
//                auto gameObjects = Scene_getRootGameObjects(scene);
//                Log::DebugFmt("gameObjects is at %p", gameObjects);
                auto gameObjectsVector = gameObjects->ToVector();
                for (auto object : gameObjectsVector) {
                    auto name = object->GetName();
                    Log::DebugFmt("SceneManager_GetSceneByName game object: %s", name.c_str());
                    switch ((HideLiveStreamSceneItemMode) Config::hideLiveStreamSceneItemsLevel) {
                        case HideLiveStreamSceneItemMode::Normal:
                            if (name.starts_with("Sc")) {
                                hideGameObjectRecursive(object, 0, 12);
                            }
                            break;
                        case HideLiveStreamSceneItemMode::Strong:
                        case HideLiveStreamSceneItemMode::Ultimate:
                            hideGameObjectRecursive(object, 0, 12);
                            break;
                        default:
                            break;
                    }
                }
            }



        }
        return scene;
    }

    DEFINE_HOOK(void, TimelineCommandReceiver_Awake, (void* self, void* method)) {
        Log::DebugFmt("TimelineCommandReceiver_Awake HOOKED");
        if (Config::hideLiveStreamSceneItemsLevel == HideLiveStreamSceneItemMode::Ultimate) return;
        TimelineCommandReceiver_Awake_Orig(self, method);
    }
#pragma region draft
    DEFINE_HOOK(void, LiveSceneController_InitializeSceneAsync, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("LiveSceneController_InitializeSceneAsync HOOKED");
        LiveSceneController_InitializeSceneAsync_Orig(self, method);
//        Il2cppUtils::GetClass
//        static auto SceneControllerBase_klass = Il2cppUtils::GetClass("Core.dll", "", "SceneControllerBase`1");
//        static auto view_field = SceneControllerBase_klass->Get<UnityResolve::Field>("_view");
////        Log::DebugFmt("view_field is at: %p", view_field);
//        auto liveSceneView = Il2cppUtils::ClassGetFieldValue<UnityResolve::UnityType::Component*>(self, view_field);
//        Log::DebugFmt("liveSceneView is at: %p", liveSceneView);
    }

    DEFINE_HOOK(void*, SceneControllerBase_GetSceneView, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("SceneControllerBase_GetSceneView HOOKED");
        return SceneControllerBase_GetSceneView_Orig(self, method);
    }

    static bool is_first_load = true;

    DEFINE_HOOK(UnityResolve::UnityType::Array<Il2cppString*>*, LiveSceneControllerLogic_FindAssetPaths, (Il2cppUtils::Il2CppObject* self, void* locationsRecord, void* timelinesRecords, void* charactersRecords, void* costumesRecords, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_FindAssetPaths HOOKED");
        auto result = LiveSceneControllerLogic_FindAssetPaths_Orig(self, locationsRecord, timelinesRecords, charactersRecords, costumesRecords, method);
        if (is_first_load) {
            is_first_load = false;
            return result;
        }
        // Convert to vector for safer filtering
//        auto originalVector = result->ToVector();
//        std::vector<Il2cppString*> filteredVector;
//
//        // Filter out stage assets
//        for (auto str : originalVector) {
//            if (str) {
//                Log::DebugFmt("Load asset path: %s", str->ToString().c_str());
//                if (!str->ToString().contains("prop")) {
//                    filteredVector.push_back(str);
//                } else {
//                    Log::DebugFmt("Filtering out prop asset path: %s", str->ToString().c_str());
//                    filteredVector.push_back(str);
//                }
//            }
//        }
//
//        // Create new Array with filtered data
//        static auto Il2cppString_klass = Il2cppUtils::GetClass("mscorlib.dll", "System", "String");
//        if (Il2cppString_klass && !filteredVector.empty()) {
//            auto newResult = UnityResolve::UnityType::Array<Il2cppString*>::New(Il2cppString_klass, filteredVector.size());
//            if (newResult) {
//                newResult->Insert(filteredVector.data(), filteredVector.size());
//                return newResult;
//            }
//        }
//
        // Fallback: return original if filtering failed
        return result;
    }

    DEFINE_HOOK(void*, LiveSceneControllerLogic_LoadLocationAssets, (Il2cppUtils::Il2CppObject* self, void* locationsRecord, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_LoadLocationAssets HOOKED");
        return LiveSceneControllerLogic_LoadLocationAssets_Orig(self, locationsRecord, method);
    }


    UnityResolve::UnityType::GameObject* viewObjectCache = nullptr;
    std::atomic<bool> analysisScheduled{false};
    std::atomic<bool> analysisRunning{false};
    std::future<void> analysisFuture;
    
    DEFINE_HOOK(void, LiveSceneControllerLogic_ctor, (Il2cppUtils::Il2CppObject* self, void* param, void* loader, UnityResolve::UnityType::GameObject* view, void* addSceneProvider, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_ctor HOOKED");
        LiveSceneControllerLogic_ctor_Orig(self, param, loader, view, addSceneProvider, method);
        Log::DebugFmt("LiveSceneControllerLogic_ctor HOOKED Finished, ready for following handlers");
        viewObjectCache = view;
    }

    DEFINE_HOOK(void*, SceneChanger_AddSceneAsync, (Il2cppUtils::Il2CppObject* self, Il2cppString* sceneName, bool ignoreEditorSceneManager, void* method)) {
        Log::DebugFmt("SceneChanger_AddSceneAsync HOOKED");
        return SceneChanger_AddSceneAsync_Orig(self, sceneName, ignoreEditorSceneManager, method);
    }

    // lag
    DEFINE_HOOK(void*, LiveSceneController_PrepareChangeSceneAsync, (Il2cppUtils::Il2CppObject* self,void* token, void* method)) {
        Log::DebugFmt("LiveSceneController_PrepareChangeSceneAsync HOOKED");
        return LiveSceneController_PrepareChangeSceneAsync_Orig(self, token, method);
    }

    void printChildren(UnityResolve::UnityType::Transform* obj, const std::string& obj_name) {
        const auto childCount = obj->GetChildCount();
        for (int i = 0;i < childCount; i++) {
            auto child = obj->GetChild(i);
            const auto childName = child->GetName();
            Log::VerboseFmt("%s child: %s", obj_name.c_str(), childName.c_str());
        }
    }

    void printGameObjectComponentsRecursive(UnityResolve::UnityType::GameObject* gameObject, int current_level, int max_level, 
                                           UnityResolve::Class* component_klass = nullptr, const std::string& prefix = "",
                                           std::set<void*>* visited = nullptr) {
        if (!gameObject || current_level > max_level) {
            return;
        }
        
        // 防止无限递归
        std::set<void*> local_visited;
        if (!visited) {
            visited = &local_visited;
        }
        
        void* gameObjectPtr = static_cast<void*>(gameObject);
        if (visited->find(gameObjectPtr) != visited->end()) {
            Log::DebugFmt("%s[L%d] GameObject: %s (ALREADY_VISITED - skipping)", prefix.c_str(), current_level, gameObject->GetName().c_str());
            return;
        }
        visited->insert(gameObjectPtr);
        
        auto objName = gameObject->GetName();
        Log::DebugFmt("%s[L%d] GameObject: %s", prefix.c_str(), current_level, objName.c_str());

        // 使用自定义类型或默认Component类型
        static auto defaultComponent_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Component");
        auto target_klass = component_klass ? component_klass : defaultComponent_klass;
        
        if (target_klass) {
            // 获取指定类型的组件
            auto components = gameObject->GetComponents<UnityResolve::UnityType::Component*>(target_klass);
            Log::DebugFmt("%s  ├─ Components[%s]: %d found", prefix.c_str(), 
                         component_klass ? component_klass->name.c_str() : "Component", components.size());
            
            for (size_t i = 0; i < components.size(); i++) {
                auto component = components[i];
                if (component) {
                    auto componentName = component->GetName();
                    // 获取组件类型信息 - 但不递归进入组件的GameObject
                    Log::DebugFmt("%s  │  ├─ [%d] %s (ptr: %p)", prefix.c_str(), i, componentName.c_str(), component);
                }
            }
        }
        
        // 递归处理子对象 - 优化子对象获取逻辑
        if (current_level < max_level) {
            auto transform = gameObject->GetTransform();
            std::vector<UnityResolve::UnityType::GameObject*> childGameObjects;
            
            // 方法1：直接通过Transform获取子对象
            if (transform) {
                const auto childCount = transform->GetChildCount();
                if (childCount > 0) {
                    Log::DebugFmt("%s  └─ Direct children: %d", prefix.c_str(), childCount);
                    for (int i = 0; i < childCount; i++) {
                        auto childTransform = transform->GetChild(i);
                        if (childTransform) {
                            auto childGameObject = childTransform->GetGameObject();
                            if (childGameObject && childGameObject != gameObject) { // 避免自引用
                                childGameObjects.push_back(childGameObject);
                            }
                        }
                    }
                }
            }
            
            // 方法2：如果没有直接子对象，使用GetComponentsInChildren (但限制层数)
            if (childGameObjects.empty() && current_level < 2) {
                static auto GameObject_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "GameObject");
                if (GameObject_klass) {
                    auto allChildGameObjects = gameObject->GetComponentsInChildren<UnityResolve::UnityType::GameObject*>(GameObject_klass, true);
                    if (allChildGameObjects.size() > 1) { // 大于1因为包含自己
                        Log::DebugFmt("%s  └─ Children via GetComponentsInChildren: %d", prefix.c_str(), std::min(5, (int)(allChildGameObjects.size() - 1)));
                        // 跳过第一个(自己)，添加其余子对象，但限制数量
                        for (size_t i = 1; i < allChildGameObjects.size() && i <= 5; i++) {
                            if (allChildGameObjects[i] && allChildGameObjects[i] != gameObject) {
                                childGameObjects.push_back(allChildGameObjects[i]);
                            }
                        }
                    }
                }
            }
            
            // 递归处理所有找到的子对象
            for (size_t i = 0; i < childGameObjects.size(); i++) {
                auto childGameObject = childGameObjects[i];
                if (childGameObject) {
                    std::string newPrefix = prefix + "  ";
                    printGameObjectComponentsRecursive(childGameObject, current_level + 1, max_level, component_klass, newPrefix, visited);
                }
            }
        }
    }
    void analyzeLiveSceneViewInDepth(UnityResolve::UnityType::Component* liveSceneView, const std::string& prefix = "") {
        if (!liveSceneView) return;

        Log::DebugFmt("%s=== Deep Analysis of LiveSceneView Component ===", prefix.c_str());
        Log::DebugFmt("%sLiveSceneView ptr: %p", prefix.c_str(), liveSceneView);

        // 1. 基本信息
        auto gameObject = liveSceneView->GetGameObject();
        auto transform = liveSceneView->GetTransform();
        Log::DebugFmt("%sGameObject: %s (ptr: %p)", prefix.c_str(),
                      gameObject ? gameObject->GetName().c_str() : "NULL", gameObject);
        Log::DebugFmt("%sTransform: %p", prefix.c_str(), transform);

        // 2. 尝试获取LiveSceneView的类定义和字段
        static auto LiveSceneView_klass = Il2cppUtils::GetClass("Core.dll", "Inspix", "LiveSceneView");
        if (LiveSceneView_klass) {
            Log::DebugFmt("%sLiveSceneView class found: %p", prefix.c_str(), LiveSceneView_klass);

            // 尝试获取一些可能的字段
            std::vector<std::string> possibleFields = {
                    "_view", "view", "sceneView", "mainView",
                    "_transform", "_gameObject", "_renderer",
                    "_camera", "camera", "_canvas", "canvas",
                    "_components", "components", "_children", "children"
            };

            for (const auto& fieldName : possibleFields) {
                auto field = LiveSceneView_klass->Get<UnityResolve::Field>(fieldName);
                if (field) {
                    Log::DebugFmt("%s  Field found: %s (offset: 0x%X, static: %s)", prefix.c_str(),
                                  fieldName.c_str(), field->offset, field->static_field ? "yes" : "no");

                    if (!field->static_field && field->offset > 0) {
                        // 尝试读取字段值
                        try {
                            void* fieldValue = *reinterpret_cast<void**>(
                                    reinterpret_cast<uintptr_t>(liveSceneView) + field->offset
                            );
                            Log::DebugFmt("%s    └─ Value: %p", prefix.c_str(), fieldValue);

                            // 如果字段值看起来像是GameObject或Component
                            if (fieldValue) {
                                auto possibleGameObject = reinterpret_cast<UnityResolve::UnityType::GameObject*>(fieldValue);
                                try {
                                    auto objName = possibleGameObject->GetName();
                                    if (!objName.empty()) {
                                        Log::DebugFmt("%s      └─ GameObject name: %s", prefix.c_str(), objName.c_str());
                                    }
                                } catch (...) {
                                    // 可能不是GameObject，尝试Component
                                    try {
                                        auto possibleComponent = reinterpret_cast<UnityResolve::UnityType::Component*>(fieldValue);
                                        auto compGameObj = possibleComponent->GetGameObject();
                                        if (compGameObj) {
                                            auto compObjName = compGameObj->GetName();
                                            Log::DebugFmt("%s      └─ Component on GameObject: %s", prefix.c_str(), compObjName.c_str());
                                        }
                                    } catch (...) {
                                        // 不是Component
                                    }
                                }
                            }
                        } catch (...) {
                            Log::DebugFmt("%s    └─ Failed to read field value", prefix.c_str());
                        }
                    }
                }
            }
        }

        // 3. 分析LiveSceneView能访问的所有组件
        Log::DebugFmt("%s--- Components accessible from LiveSceneView ---", prefix.c_str());
        auto allComponents = liveSceneView->GetComponentsInChildren<UnityResolve::UnityType::Component*>();
        Log::DebugFmt("%sTotal accessible components: %d", prefix.c_str(), allComponents.size());

        std::map<std::string, int> componentTypeCount;
        std::map<std::string, std::vector<UnityResolve::UnityType::Component*>> componentsByType;

        for (size_t i = 0; i < allComponents.size() && i < 50; i++) { // 限制数量
            auto comp = allComponents[i];
            if (comp) {
                std::string kompName = "Unknown";
                if (comp->Il2CppClass.klass) {
                    kompName = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", comp->Il2CppClass.klass);
                }
                componentTypeCount[kompName]++;
                componentsByType[kompName].push_back(comp);
            }
        }

        // 按类型分组显示组件
        for (const auto& pair : componentTypeCount) {
            const auto& typeName = pair.first;
            const auto& count = pair.second;
            Log::DebugFmt("%s  %s: %d instances", prefix.c_str(), typeName.c_str(), count);

            // 显示每个实例所在的GameObject
            for (size_t i = 0; i < componentsByType[typeName].size() && i < 3; i++) {
                auto comp = componentsByType[typeName][i];
                auto compGameObj = comp->GetGameObject();
                auto gameObjName = compGameObj ? compGameObj->GetName() : "NULL";
                Log::DebugFmt("%s    ├─ [%d] on GameObject: %s", prefix.c_str(), i, gameObjName.c_str());
            }
            if (componentsByType[typeName].size() > 3) {
                Log::DebugFmt("%s    └─ ... and %d more", prefix.c_str(),
                              componentsByType[typeName].size() - 3);
            }
        }

        // 4. 分析Transform层级
        if (transform) {
            Log::DebugFmt("%s--- Transform Hierarchy from LiveSceneView ---", prefix.c_str());
            const auto childCount = transform->GetChildCount();
            auto parent = transform->GetParent();

            Log::DebugFmt("%sTransform parent: %s", prefix.c_str(),
                          parent ? (parent->GetGameObject() ? parent->GetGameObject()->GetName().c_str() : "NULL") : "ROOT");
            Log::DebugFmt("%sTransform children: %d", prefix.c_str(), childCount);

            for (int i = 0; i < childCount && i < 10; i++) {
                auto childTransform = transform->GetChild(i);
                if (childTransform) {
                    auto childGameObj = childTransform->GetGameObject();
                    auto childName = childGameObj ? childGameObj->GetName() : "NULL";
                    Log::DebugFmt("%s  Child[%d]: %s", prefix.c_str(), i, childName.c_str());

                    // 显示子对象的组件
                    if (childGameObj) {
                        static auto Component_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Component");
                        if (Component_klass) {
                            auto childComponents = childGameObj->GetComponents<UnityResolve::UnityType::Component*>(Component_klass);
                            Log::DebugFmt("%s    └─ Components: %d", prefix.c_str(), childComponents.size());
                            for (size_t j = 0; j < childComponents.size() && j < 3; j++) {
                                auto childComp = childComponents[j];
                                std::string childCompType = "Unknown";
                                if (childComp && childComp->Il2CppClass.klass) {
                                    childCompType = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", childComp->Il2CppClass.klass);
                                }
                                Log::DebugFmt("%s      ├─ [%d] %s", prefix.c_str(), j, childCompType.c_str());
                            }
                        }
                    }
                }
            }
        }

        Log::DebugFmt("%s=== End Deep Analysis of LiveSceneView ===", prefix.c_str());
    }

    void exploreComponentsInDetail(UnityResolve::UnityType::GameObject* gameObject, const std::string& prefix = "") {
        if (!gameObject) return;
        
        auto objName = gameObject->GetName();
        Log::DebugFmt("%s=== Detailed Component Analysis for: %s ===", prefix.c_str(), objName.c_str());
        
        // 获取所有组件
        static auto Component_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Component");
        if (Component_klass) {
            auto components = gameObject->GetComponents<UnityResolve::UnityType::Component*>(Component_klass);
            Log::DebugFmt("%sTotal components: %d", prefix.c_str(), components.size());
            
            // 检查是否有重复的指针
            std::set<void*> componentPtrs;
            int duplicateCount = 0;
            for (size_t i = 0; i < components.size(); i++) {
                if (componentPtrs.find(components[i]) != componentPtrs.end()) {
                    duplicateCount++;
                } else {
                    componentPtrs.insert(components[i]);
                }
            }
            Log::DebugFmt("%sDuplicate components: %d, Unique: %d", prefix.c_str(), duplicateCount, componentPtrs.size());
            
            for (size_t i = 0; i < components.size(); i++) {
                auto component = components[i];
                if (component) {
                    auto componentName = component->GetName();
                    
                    // 尝试多种方式获取组件的实际类型信息
                    auto componentType = component->GetType();
                    std::string typeName = "Unknown";
                    std::string className = "Unknown";
                    
                    if (componentType) {
                        typeName = componentType->GetFullName();
                        className = componentType->FormatTypeName();
                    }
                    
                    // 尝试从Il2Cpp对象直接获取类型信息
                    void* klass = nullptr;
                    std::string klassName = "Unknown";
                    if (component->Il2CppClass.klass) {
                        klass = component->Il2CppClass.klass;
                        klassName = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", klass);
                    }
                    
                    // 检查这个组件是否属于不同的GameObject
                    auto compGameObject = component->GetGameObject();
                    auto compGameObjectName = compGameObject ? compGameObject->GetName() : "NULL";
                    bool isDifferentGameObject = (compGameObject != gameObject);
                    
                    Log::DebugFmt("%s  Component[%d]: %s (ptr: %p)", prefix.c_str(), i, componentName.c_str(), component);
                    Log::DebugFmt("%s    ├─ FullName: %s", prefix.c_str(), typeName.c_str());
                    Log::DebugFmt("%s    ├─ ClassName: %s", prefix.c_str(), className.c_str());
                    Log::DebugFmt("%s    ├─ Il2CppClass: %s (ptr: %p)", prefix.c_str(), klassName.c_str(), klass);
                    Log::DebugFmt("%s    ├─ GameObject: %s %s", prefix.c_str(), compGameObjectName.c_str(), 
                                 isDifferentGameObject ? "(DIFFERENT!)" : "(same)");
                    Log::DebugFmt("%s    ├─ Component==GameObject: %s", prefix.c_str(), 
                                 ((void*)component == gameObject) ? "YES!" : "no");
                    
                    // 如果是LiveSceneView，进行深度分析
                    if (klassName == "LiveSceneView") {
                        Log::DebugFmt("%s    ├─ *** DETECTED LiveSceneView - Starting Deep Analysis ***", prefix.c_str());
                        analyzeLiveSceneViewInDepth(component, prefix + "    ");
                    }
                    
                    // 尝试获取Transform组件信息
                    auto transform = component->GetTransform();
                    if (transform && transform != component) {
                        auto transformGameObj = transform->GetGameObject();
                        auto transformGameObjName = transformGameObj ? transformGameObj->GetName() : "NULL";
                        Log::DebugFmt("%s    ├─ Transform GameObject: %s", prefix.c_str(), transformGameObjName.c_str());
                    }
                    
                    // 尝试获取这个组件的子组件
                    Log::DebugFmt("%s    ├─ Checking for child components...", prefix.c_str());
                    auto childComponents = component->GetComponentsInChildren<UnityResolve::UnityType::Component*>();
                    if (childComponents.size() > 1) { // 大于1因为包含自己
                        Log::DebugFmt("%s    ├─ Child components from %s: %d", prefix.c_str(), klassName.c_str(), childComponents.size() - 1);
                        for (size_t j = 1; j < childComponents.size() && j <= 5; j++) { // 限制显示数量，跳过自己
                            auto childComp = childComponents[j];
                            if (childComp && childComp != component) {
                                auto childCompName = childComp->GetName();
                                auto childCompGameObject = childComp->GetGameObject();
                                auto childCompGameObjectName = childCompGameObject ? childCompGameObject->GetName() : "NULL";
                                
                                // 获取子组件的类型
                                std::string childKlassName = "Unknown";
                                if (childComp->Il2CppClass.klass) {
                                    childKlassName = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", childComp->Il2CppClass.klass);
                                }
                                
                                Log::DebugFmt("%s      ├─ [%d] %s (%s) on GameObject: %s", prefix.c_str(), 
                                            j-1, childKlassName.c_str(), childCompName.c_str(), childCompGameObjectName.c_str());
                            }
                        }
                    } else {
                        Log::DebugFmt("%s    ├─ No child components found", prefix.c_str());
                    }
                    
                    // 如果是Transform组件，显示层级信息
                    if (klassName == "Transform") {
                        auto transformComp = reinterpret_cast<UnityResolve::UnityType::Transform*>(component);
                        if (transformComp) {
                            const auto childCount = transformComp->GetChildCount();
                            auto parent = transformComp->GetParent();
                            auto parentName = parent ? (parent->GetGameObject() ? parent->GetGameObject()->GetName() : "NULL") : "ROOT";
                            
                            Log::DebugFmt("%s    ├─ Transform Parent: %s", prefix.c_str(), parentName.c_str());
                            Log::DebugFmt("%s    └─ Transform Children: %d", prefix.c_str(), childCount);
                            
                            for (int k = 0; k < childCount && k < 5; k++) {
                                auto childTransform = transformComp->GetChild(k);
                                if (childTransform) {
                                    auto childGameObj = childTransform->GetGameObject();
                                    auto childObjName = childGameObj ? childGameObj->GetName() : "NULL";
                                    Log::DebugFmt("%s      ├─ Transform Child[%d]: %s", prefix.c_str(), k, childObjName.c_str());
                                }
                            }
                        }
                    }
                    
                    Log::DebugFmt("%s", prefix.c_str()); // 空行分隔
                }
            }
        }
        
        // 尝试获取所有子GameObject
        auto transform = gameObject->GetTransform();
        if (transform) {
            const auto childCount = transform->GetChildCount();
            if (childCount > 0) {
                Log::DebugFmt("%s=== Direct Child GameObjects: %d ===", prefix.c_str(), childCount);
                for (int i = 0; i < childCount && i < 5; i++) {
                    auto childTransform = transform->GetChild(i);
                    if (childTransform) {
                        auto childGameObject = childTransform->GetGameObject();
                        if (childGameObject) {
                            auto childName = childGameObject->GetName();
                            Log::DebugFmt("%s  Child GameObject[%d]: %s", prefix.c_str(), i, childName.c_str());
                            
                            // 递归分析第一级子对象的组件
                            if (i < 2) { // 只分析前两个子对象
                                exploreComponentsInDetail(childGameObject, prefix + "    ");
                            }
                        }
                    }
                }
            }
        }
        
        Log::DebugFmt("%s=== End Detailed Analysis ===", prefix.c_str());
    }

    // 异步执行分析的函数
    void performAsyncAnalysis() {
        if (viewObjectCache) {
            auto gameObject = viewObjectCache;
            auto gameObjectName = gameObject->GetName();
            Log::DebugFmt("Live scene view game object is %s", gameObjectName.c_str());

            // 打印所有组件的递归结构
            Log::DebugFmt("=== Recursive GameObject Components Structure (max 3 levels) ===");
            printGameObjectComponentsRecursive(gameObject, 0, 6);
            Log::DebugFmt("=== End Recursive Structure ===");

            // 详细分析每个组件
            Log::DebugFmt("=== Detailed Component Analysis ===");
            exploreComponentsInDetail(gameObject, "  ");
            Log::DebugFmt("=== End Detailed Component Analysis ===");

            // 可选：打印特定类型的组件
            static auto Transform_klass = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
            if (Transform_klass) {
                Log::DebugFmt("=== Recursive Transform Components Only ===");
                printGameObjectComponentsRecursive(gameObject, 0, 6, Transform_klass);
                Log::DebugFmt("=== End Transform Structure ===");
            }
        }
        analysisScheduled = false;
        analysisRunning = false;
    }

    // 启动异步分析的包装函数
    void startAsyncAnalysis() {
        if (analysisRunning.exchange(true)) {
            return; // 已经在运行中
        }

        try {
            // 使用 std::async 创建异步任务
            analysisFuture = std::async(std::launch::async, []() {
                // 延迟5秒
                std::this_thread::sleep_for(std::chrono::seconds(20));

                // 检查Unity线程状态
                UnityResolve::ThreadAttach();

                Log::DebugFmt("Starting delayed async analysis");
                performAsyncAnalysis();

                UnityResolve::ThreadDetach();
            });
        } catch (const std::exception& e) {
            Log::ErrorFmt("Failed to start async analysis: %s", e.what());
            analysisRunning = false;
        }
    }

    static int hooked_count = 0;
    DEFINE_HOOK(void, LiveSceneController_InitializeSceneAsync_MoveNext, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("LiveSceneController_InitializeSceneAsync_MoveNext HOOKED");
        LiveSceneController_InitializeSceneAsync_MoveNext_Orig(self, method);
        Log::DebugFmt("LiveSceneController_InitializeSceneAsync_MoveNext HOOKED Finished");
//        hooked_count = hooked_count+1;
//        if (hooked_count <= 1) return;
//        // 启动异步非阻塞分析（5秒后执行）
//        if (!analysisScheduled.exchange(true)) {
//            Log::DebugFmt("Starting async analysis in 5 seconds...");
//            startAsyncAnalysis();
//        }
    }

    DEFINE_HOOK(bool, LiveSceneControllerLogic_Initialize_MoveNext, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("LiveSceneControllerLogic_Initialize_MoveNext HOOKED");
        auto result = LiveSceneControllerLogic_Initialize_MoveNext_Orig(self, method);
        Log::DebugFmt("LiveSceneControllerLogic_Initialize_MoveNext HOOKED Finished");
        return result;
    }

    DEFINE_HOOK(void*, Scene_GetRootGameObjects_Injected, (Il2cppUtils::Il2CppObject* self, void* method)) {
        Log::DebugFmt("Scene_GetRootGameObjects_Injected HOOKED");
        auto result = Scene_GetRootGameObjects_Injected_Orig(self, method);
        Log::DebugFmt("Scene_GetRootGameObjects_Injected HOOKED Finished");
        return result;
    }

#pragma endregion

    void Install(HookInstaller* hookInstaller) {
        ADD_HOOK(Internal_LogException, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_LogException(System.Exception,UnityEngine.Object)"));
        ADD_HOOK(Internal_Log, Il2cppUtils::il2cpp_resolve_icall(
                "UnityEngine.DebugLogHandler::Internal_Log(UnityEngine.LogType,UnityEngine.LogOption,System.String,UnityEngine.Object)"));
        
        // 👀
        ADD_HOOK(CoverImageCommandReceiver_Awake, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "CoverImageCommandReceiver", "Awake"));
        ADD_HOOK(CharacterVisibleReceiver_SetupExistCharacter, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character", "CharacterVisibleReceiver", "SetupExistCharacter"));

        // 👀 old
        ADD_HOOK(MRS_AppsCoverScreen_SetActiveCoverImage, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.LiveMain", "AppsCoverScreen", "SetActiveCoverImage"));
        ADD_HOOK(CharacterVisibleReceiver_UpdateAvatarVisibility, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "Inspix.Character", "CharacterVisibleReceiver", "UpdateAvatarVisibility"));
        ADD_HOOK(Hailstorm_AssetDownloadJob_get_UrlBase, Il2cppUtils::GetMethodPointer("Core.dll", "Hailstorm", "AssetDownloadJob", "get_UrlBase"));

        ADD_HOOK(FootShadowManipulator_OnInstantiate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.FootShadow", "FootShadowManipulator", "OnInstantiate"));
        ADD_HOOK(ItemManipulator_OnInstantiate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Character.Item", "ItemManipulator", "OnInstantiate"));
        ADD_HOOK(ScenePropManipulator_OnInstantiate, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix.Operator", "ScenePropManipulator", "OnInstantiate"));
        ADD_HOOK(SceneManager_GetSceneByName, Il2cppUtils::GetMethodPointer("UnityEngine.CoreModule.dll", "UnityEngine.SceneManagement", "SceneManager", "GetSceneByName"));

        ADD_HOOK(TimelineCommandReceiver_Awake, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "TimelineCommandReceiver", "Awake"));
#pragma region draft_hook
//        ADD_HOOK(LiveSceneController_InitializeSceneAsync, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneController", "InitializeSceneAsync"));
//        ADD_HOOK(SceneControllerBase_GetSceneView, Il2cppUtils::GetMethodPointer("Core.dll", "", "SceneControllerBase`1", "SetSceneParam"));
//        ADD_HOOK(LiveSceneControllerLogic_FindAssetPaths, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneControllerLogic", "FindAssetPaths"));
//        ADD_HOOK(LiveSceneControllerLogic_LoadLocationAssets, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneControllerLogic", "LoadLocationAssets"));
//        ADD_HOOK(LiveSceneControllerLogic_ctor, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneControllerLogic", ".ctor"));
//        ADD_HOOK(SceneChanger_AddSceneAsync, Il2cppUtils::GetMethodPointer("Assembly-CSharp.dll", "", "SceneChanger", "AddSceneAsync"));
        //        ADD_HOOK(LiveSceneController_PrepareChangeSceneAsync, Il2cppUtils::GetMethodPointer("Core.dll", "Inspix", "LiveSceneController", "PrepareChangeSceneAsync"));

//        Il2cppUtils::MethodInfo* method = nullptr;
//        auto LiveSceneController_klass = Il2cppUtils::GetClassIl2cpp("Core.dll", "Inspix", "LiveSceneController");
//        if (LiveSceneController_klass) {
//            auto InitializeSceneAsync_klass = Il2cppUtils::find_nested_class_from_name(
//                    LiveSceneController_klass, "<InitializeSceneAsync>d__3");
//            method = Il2cppUtils::GetMethodIl2cpp(InitializeSceneAsync_klass, "MoveNext", 0);
//            if (method) {
//                ADD_HOOK(LiveSceneController_InitializeSceneAsync_MoveNext, method->methodPointer);
//            }
//        }
//        auto LiveSceneControllerLogic_klass = Il2cppUtils::GetClassIl2cpp("Core.dll", "Inspix", "LiveSceneControllerLogic");
//        if (LiveSceneControllerLogic_klass) {
//            auto Initialize_klass = Il2cppUtils::find_nested_class_from_name(
//                    LiveSceneControllerLogic_klass, "<Initialize>d__12");
//            method = Il2cppUtils::GetMethodIl2cpp(Initialize_klass, "MoveNext", 0);
//            if (method) {
//                ADD_HOOK(LiveSceneControllerLogic_Initialize_MoveNext, method->methodPointer);
//            }
//        }
#pragma endregion

    }
}