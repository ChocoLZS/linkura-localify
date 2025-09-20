#pragma once
#include "../deps/UnityResolve/UnityResolve.hpp"
#include "Log.h"
#include <memory>
#include "functional"

namespace Il2cppUtils {
    using namespace LinkuraLocal;
    using Il2CppString = UnityResolve::UnityType::String;
    template <typename T>
    using Il2CppArray = UnityResolve::UnityType::Array<T>;
    struct Il2CppClassHead {
        // The following fields are always valid for a Il2CppClass structure
        const void* image;
        void* gc_desc;
        const char* name;
        const char* namespaze;
    };

    struct Il2CppObject
    {
        union
        {
            void* klass;
            void* vtable;
        };
        void* monitor;
    };

    enum Il2CppTypeEnum
    {
        IL2CPP_TYPE_END = 0x00,       /* End of List */
        IL2CPP_TYPE_VOID = 0x01,
        IL2CPP_TYPE_BOOLEAN = 0x02,
        IL2CPP_TYPE_CHAR = 0x03,
        IL2CPP_TYPE_I1 = 0x04,
        IL2CPP_TYPE_U1 = 0x05,
        IL2CPP_TYPE_I2 = 0x06,
        IL2CPP_TYPE_U2 = 0x07,
        IL2CPP_TYPE_I4 = 0x08,
        IL2CPP_TYPE_U4 = 0x09,
        IL2CPP_TYPE_I8 = 0x0a,
        IL2CPP_TYPE_U8 = 0x0b,
        IL2CPP_TYPE_R4 = 0x0c,
        IL2CPP_TYPE_R8 = 0x0d,
        IL2CPP_TYPE_STRING = 0x0e,
        IL2CPP_TYPE_PTR = 0x0f,
        IL2CPP_TYPE_BYREF = 0x10,
        IL2CPP_TYPE_VALUETYPE = 0x11,
        IL2CPP_TYPE_CLASS = 0x12,
        IL2CPP_TYPE_VAR = 0x13,
        IL2CPP_TYPE_ARRAY = 0x14,
        IL2CPP_TYPE_GENERICINST = 0x15,
        IL2CPP_TYPE_TYPEDBYREF = 0x16,
        IL2CPP_TYPE_I = 0x18,
        IL2CPP_TYPE_U = 0x19,
        IL2CPP_TYPE_FNPTR = 0x1b,
        IL2CPP_TYPE_OBJECT = 0x1c,
        IL2CPP_TYPE_SZARRAY = 0x1d,
        IL2CPP_TYPE_MVAR = 0x1e,
        IL2CPP_TYPE_CMOD_REQD = 0x1f,
        IL2CPP_TYPE_CMOD_OPT = 0x20,
        IL2CPP_TYPE_INTERNAL = 0x21,

        IL2CPP_TYPE_MODIFIER = 0x40,
        IL2CPP_TYPE_SENTINEL = 0x41,
        IL2CPP_TYPE_PINNED = 0x45,

        IL2CPP_TYPE_ENUM = 0x55
    };

    typedef struct Il2CppType
    {
        void* dummy;
        unsigned int attrs : 16;
        Il2CppTypeEnum type : 8;
        unsigned int num_mods : 6;
        unsigned int byref : 1;
        unsigned int pinned : 1;
    } Il2CppType;

    struct Il2CppReflectionType
    {
        Il2CppObject object;
        const Il2CppType* type;
    };

    struct Resolution_t {
        int width;
        int height;
        int herz;
    };

    struct FieldInfo {
        const char* name;
        const Il2CppType* type;
        uintptr_t parent;
        int32_t offset;
        uint32_t token;
    };

    struct MethodInfo {
        uintptr_t methodPointer;
        uintptr_t invoker_method;
        const char* name;
        uintptr_t klass;
        const Il2CppType* return_type;
        //const ParameterInfo* parameters;
        // const void* return_type;
        const void* parameters;
        uintptr_t methodDefinition;
        uintptr_t genericContainer;
        uint32_t token;
        uint16_t flags;
        uint16_t iflags;
        uint16_t slot;
        uint8_t parameters_count;
        uint8_t is_generic : 1;
        uint8_t is_inflated : 1;
        uint8_t wrapper_type : 1;
        uint8_t is_marshaled_from_native : 1;
    };

    static UnityResolve::Class* GetClass(const std::string& assemblyName, const std::string& nameSpaceName,
                   const std::string& className) {
        const auto assembly = UnityResolve::Get(assemblyName);
        if (!assembly) {
            Log::ErrorFmt("GetClass error: assembly %s not found.", assemblyName.c_str());
            return nullptr;
        }
        const auto pClass = assembly->Get(className, nameSpaceName);
        if (!pClass) {
            Log::ErrorFmt("GetClass error: Class %s::%s not found.", nameSpaceName.c_str(), className.c_str());
            return nullptr;
        }
        return pClass;
    }

    // UnityResolve.hpp static auto ForeachFields il2cpp only
    static auto ForeachFields(UnityResolve::Class* klass, void* pKlass) -> void {
        void* iter = nullptr;
        void* field;
        do {
            if ((field = UnityResolve::Invoke<void*>("il2cpp_class_get_fields", pKlass, &iter))) {
                const auto pField = new UnityResolve::Field{ .address = field, .name = UnityResolve::Invoke<const char*>("il2cpp_field_get_name", field), .type = new UnityResolve::Type{.address = UnityResolve::Invoke<void*>("il2cpp_field_get_type", field)}, .klass = klass, .offset = UnityResolve::Invoke<int>("il2cpp_field_get_offset", field), .static_field = false, .vTable = nullptr };
                int        tSize{};
                pField->static_field = pField->offset <= 0;
                pField->type->name = UnityResolve::Invoke<const char*>("il2cpp_type_get_name", pField->type->address);
                pField->type->size = -1;
                klass->fields.push_back(pField);
            }
        } while (field);
    }

    static auto ForeachMethod(UnityResolve::Class* klass, void* pKlass) -> void {
        // 遍历方法
        void* iter = nullptr;
        void* method;
        do {
            if ((method = UnityResolve::Invoke<void*>("il2cpp_class_get_methods", pKlass, &iter))) {
                int        fFlags{};
                const auto pMethod = new UnityResolve::Method{};
                pMethod->address = method;
                pMethod->name = UnityResolve::Invoke<const char*>("il2cpp_method_get_name", method);
                pMethod->klass = klass;
                pMethod->return_type = new UnityResolve::Type{ .address = UnityResolve::Invoke<void*>("il2cpp_method_get_return_type", method), };
                pMethod->flags = UnityResolve::Invoke<int>("il2cpp_method_get_flags", method, &fFlags);

                int tSize{};
                pMethod->static_function = pMethod->flags & 0x10;
                pMethod->return_type->name = UnityResolve::Invoke<const char*>("il2cpp_type_get_name", pMethod->return_type->address);
                pMethod->return_type->size = -1;
                pMethod->function = *static_cast<void**>(method);
                klass->methods.push_back(pMethod);
                const auto argCount = UnityResolve::Invoke<int>("il2cpp_method_get_param_count", method);
                for (auto index = 0; index < argCount; index++) pMethod->args.push_back(new UnityResolve::Method::Arg{ UnityResolve::Invoke<const char*>("il2cpp_method_get_param_name", method, index), new UnityResolve::Type{.address = UnityResolve::Invoke<void*>("il2cpp_method_get_param", method, index), .name = UnityResolve::Invoke<const char*>("il2cpp_type_get_name", UnityResolve::Invoke<void*>("il2cpp_method_get_param", method, index)), .size = -1} });
            }
        } while (method);
    }


    static UnityResolve::Class* ToUnityResolveClass(void* pClass) {
        const auto pAClass = new UnityResolve::Class();
        pAClass->address = pClass;
        pAClass->name = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", pClass);
        if (const auto pPClass = UnityResolve::Invoke<void*>("il2cpp_class_get_parent", pClass)) pAClass->parent = UnityResolve::Invoke<const char*>("il2cpp_class_get_name", pPClass);
        pAClass->namespaze = "";
        ForeachFields(pAClass, pClass);
        ForeachMethod(pAClass, pClass);
        void* i_class{};
        void* iter{};
        do {
            if ((i_class = UnityResolve::Invoke<void*>("il2cpp_class_get_interfaces", pClass, &iter))) {
                Log::DebugFmt("Starting get foreach");
                ForeachFields(pAClass, i_class);
                ForeachMethod(pAClass, i_class);
            }
        } while (i_class);
        return pAClass;
    }
    /*
    UnityResolve::Method* GetMethodIl2cpp(const char* assemblyName, const char* nameSpaceName,
                                    const char* className, const char* methodName, const int argsCount) {
        auto domain = UnityResolve::Invoke<void*>("il2cpp_domain_get");
        UnityResolve::Invoke<void*>("il2cpp_thread_attach", domain);
        auto image = UnityResolve::Invoke<void*>("il2cpp_assembly_get_image", domain);
        if (!image) {
            Log::ErrorFmt("GetMethodIl2cpp error: assembly %s not found.", assemblyName);
            return nullptr;
        }
        Log::Debug("GetMethodIl2cpp 1");
        auto klass = UnityResolve::Invoke<void*>("il2cpp_class_from_name", image, nameSpaceName, className);
        if (!klass) {
            Log::ErrorFmt("GetMethodIl2cpp error: Class %s::%s not found.", nameSpaceName, className);
            return nullptr;
        }
        Log::Debug("GetMethodIl2cpp 2");
        auto ret = UnityResolve::Invoke<UnityResolve::Method*>("il2cpp_class_get_method_from_name", klass, methodName, argsCount);
        if (!ret) {
            Log::ErrorFmt("GetMethodIl2cpp error: method %s::%s.%s not found.", nameSpaceName, className, methodName);
            return nullptr;
        }
        return ret;
    }*/

    static UnityResolve::Method* GetMethod(const std::string& assemblyName, const std::string& nameSpaceName,
                           const std::string& className, const std::string& methodName, const std::vector<std::string>& args = {}) {
        const auto assembly = UnityResolve::Get(assemblyName);
        if (!assembly) {
            Log::ErrorFmt("GetMethod error: assembly %s not found.", assemblyName.c_str());
            return nullptr;
        }
        const auto pClass = assembly->Get(className, nameSpaceName);
        if (!pClass) {
            Log::ErrorFmt("GetMethod error: Class %s::%s not found.", nameSpaceName.c_str(), className.c_str());
            return nullptr;
        }
        auto method = pClass->Get<UnityResolve::Method>(methodName, args);
        if (!method) {
            /*
            method = GetMethodIl2cpp(assemblyName.c_str(), nameSpaceName.c_str(), className.c_str(),
                                     methodName.c_str(), args.size() == 0 ? -1 : args.size());
            if (!method) {
                Log::ErrorFmt("GetMethod error: method %s::%s.%s not found.", nameSpaceName.c_str(), className.c_str(), methodName.c_str());
                return nullptr;
            }*/
            Log::ErrorFmt("GetMethod error: method %s::%s.%s not found.", nameSpaceName.c_str(), className.c_str(), methodName.c_str());
            return nullptr;
        }
        return method;
    }

    static void* GetMethodPointer(const std::string& assemblyName, const std::string& nameSpaceName,
                           const std::string& className, const std::string& methodName, const std::vector<std::string>& args = {}) {
        auto method = GetMethod(assemblyName, nameSpaceName, className, methodName, args);
        if (method) {
            return method->function;
        }
        return nullptr;
    }

    static void* il2cpp_resolve_icall(const char* s) {
        return UnityResolve::Invoke<void*>("il2cpp_resolve_icall", s);
    }


    static Il2CppClassHead* get_class_from_instance(const void* instance) {
        return static_cast<Il2CppClassHead*>(*static_cast<void* const*>(std::assume_aligned<alignof(void*)>(instance)));
    }

    static void* get_system_type_from_instance(const void* instance) {
        auto klass = Il2cppUtils::get_class_from_instance(instance);
        auto type = UnityResolve::Invoke<void*>("il2cpp_class_get_type", klass);
        return UnityResolve::Invoke<Il2cppUtils::Il2CppReflectionType*>("il2cpp_type_get_object", type);
    }

    static MethodInfo* il2cpp_class_get_method_from_name(void* klass, const char* name, int argsCount) {
        return UnityResolve::Invoke<MethodInfo*>("il2cpp_class_get_method_from_name", klass, name, argsCount);
    }

    static Il2cppUtils::FieldInfo* il2cpp_class_get_field_from_name(void* klass, const char* filedName) {
        return UnityResolve::Invoke<Il2cppUtils::FieldInfo*>("il2cpp_class_get_field_from_name", klass, filedName);
    }

    static uintptr_t il2cpp_class_get_method_pointer_from_name(void* klass, const char* name, int argsCount) {
        auto findKlass = il2cpp_class_get_method_from_name(klass, name, argsCount);
        if (findKlass) {
            return findKlass->methodPointer;
        }
        Log::ErrorFmt("method: %s not found", name);
        return 0;
    }

    static void* GetClassIl2cpp(const std::string& assemblyName, const std::string& nameSpaceName,
                                const std::string& className) {
        const auto assembly = UnityResolve::Get(assemblyName);
        if (!assembly) {
            Log::ErrorFmt("GetClassIl2cpp error: assembly %s not found.", assemblyName.c_str());
            return nullptr;
        }
        const auto image = UnityResolve::Invoke<void*>("il2cpp_assembly_get_image", assembly->address);
        if (!image) {
            Log::ErrorFmt("GetClassIl2cpp error: assembly %s not found.", assemblyName.c_str());
            return nullptr;
        }
        return UnityResolve::Invoke<void*>("il2cpp_class_from_name", image, nameSpaceName.c_str(), className.c_str());
    }
    static MethodInfo* GetMethodIl2cpp(const char* assemblyName, const char* nameSpaceName,
                                                 const char* className, const char* methodName, const int argsCount) {
        auto klass = GetClassIl2cpp(assemblyName, nameSpaceName, className);
        if (!klass) {
            Log::ErrorFmt("GetMethodIl2cpp error: Class %s::%s not found.", nameSpaceName, className);
            return nullptr;
        }
        return il2cpp_class_get_method_from_name(klass, methodName, argsCount);
    }
    static MethodInfo* GetMethodIl2cpp(void* klass, const char* methodName, const int argsCount) {
        if (!klass) return nullptr;
        return il2cpp_class_get_method_from_name(klass, methodName, argsCount);
    }

    static void* find_nested_class(void* klass, std::predicate<void*> auto&& predicate) {
        void* iter{};
        while (const auto curNestedClass = UnityResolve::Invoke<void*>("il2cpp_class_get_nested_types", klass, &iter))
        {
            if (static_cast<decltype(predicate)>(predicate)(curNestedClass))
            {
                return curNestedClass;
            }
        }

        return nullptr;
    }

    static void* find_nested_class_from_name(void* klass, const char* name) {
        if (!klass) return nullptr;
        return find_nested_class(klass, [name = std::string_view(name)](void* nestedClass) {
//            Log::DebugFmt("%s", static_cast<Il2CppClassHead*>(nestedClass)->name);
            // TODO, Regex or match
            return static_cast<Il2CppClassHead*>(nestedClass)->name == name;
        });
    }

//    static void* find_nested_class_from_name(void* klass, ()) {
//
//    }

    template <typename RType>
    static auto ClassGetFieldValue(void* obj, UnityResolve::Field* field) -> RType {
        return *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(obj) + field->offset);
    }

    template <typename RType>
    static auto ClassGetFieldValue(void* obj, FieldInfo* field) -> RType {
        return *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(obj) + field->offset);
    }

    template <typename T>
    static auto ClassSetFieldValue(void* obj, UnityResolve::Field* field, T value) -> void {
        const auto fieldPtr = static_cast<std::byte*>(obj) + field->offset;
        std::memcpy(fieldPtr, std::addressof(value), sizeof(T));
    }

    template <typename RType>
    static auto ClassSetFieldValue(void* obj, FieldInfo* field, RType value) -> void {
        *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(obj) + field->offset) = value;
    }

    static void* get_system_class_from_reflection_type_str(const char* typeStr, const char* assemblyName = "mscorlib") {

        static auto assemblyLoad = reinterpret_cast<void* (*)(Il2CppString*)>(
                GetMethodPointer("mscorlib.dll", "System.Reflection",
                                 "Assembly", "Load", {"*"})
        );
        static auto assemblyGetType = reinterpret_cast<Il2CppReflectionType * (*)(void*, Il2CppString*)>(
                GetMethodPointer("mscorlib.dll", "System.Reflection",
                                 "Assembly", "GetType", {"*"})
        );

        static auto reflectionAssembly = assemblyLoad(Il2CppString::New(assemblyName));
        auto reflectionType = assemblyGetType(reflectionAssembly, Il2CppString::New(typeStr));
        return UnityResolve::Invoke<void*>("il2cpp_class_from_system_type", reflectionType);
    }

    static std::unordered_map<std::string, std::unordered_map<int, std::string>> enumToValueMapCache{};
    static std::unordered_map<int, std::string> EnumToValueMap(Il2CppClassHead* enumClass, bool useCache) {
        std::unordered_map<int, std::string> ret{};
        auto isEnum = UnityResolve::Invoke<bool>("il2cpp_class_is_enum", enumClass);

        if (isEnum) {
            Il2cppUtils::FieldInfo* field = nullptr;
            void* iter = nullptr;

            std::string cacheName = std::string(enumClass->namespaze) + "::" + enumClass->name;
            if (useCache) {
                if (auto it = enumToValueMapCache.find(cacheName); it != enumToValueMapCache.end()) {
                    return it->second;
                }
            }

            while ((field = UnityResolve::Invoke<Il2cppUtils::FieldInfo*>("il2cpp_class_get_fields", enumClass, &iter))) {
                // Log::DebugFmt("field: %s, off: %d", field->name, field->offset);
                if (field->offset > 0) continue;  // 非 static
                if (strcmp(field->name, "value__") == 0) {
                    continue;
                }

                int value;
                UnityResolve::Invoke<void>("il2cpp_field_static_get_value", field, &value);
                // Log::DebugFmt("returnClass: %s - %s: 0x%x", enumClass->name, field->name, value);
                std::string itemName = std::string(enumClass->name) + "_" + field->name;
                ret.emplace(value, std::move(itemName));
            }

            if (useCache) {
                enumToValueMapCache.emplace(std::move(cacheName), ret);
            }
        }
        return ret;
    }

    template <typename T = void*>
    static void iterate_IEnumerable(const void* obj, std::invocable<T> auto&& receiver)
    {
        const auto klass = get_class_from_instance(obj);
        const auto getEnumeratorMethod = reinterpret_cast<void* (*)(const void*)>(il2cpp_class_get_method_from_name(klass, "GetEnumerator", 0)->methodPointer);
        const auto enumerator = getEnumeratorMethod(obj);
        const auto enumeratorClass = get_class_from_instance(enumerator);
        const auto getCurrentMethod = reinterpret_cast<T(*)(void*)>(il2cpp_class_get_method_from_name(enumeratorClass, "get_Current", 0)->methodPointer);
        const auto moveNextMethod = reinterpret_cast<bool(*)(void*)>(il2cpp_class_get_method_from_name(enumeratorClass, "MoveNext", 0)->methodPointer);

        while (moveNextMethod(enumerator))
        {
            static_cast<decltype(receiver)>(receiver)(getCurrentMethod(enumerator));
        }
    }

    static Il2CppString* ToJsonStr(void* object) {
        static Il2CppString* (*toJsonStr)(void*) = nullptr;
        if (!toJsonStr) {
            toJsonStr = reinterpret_cast<Il2CppString * (*)(void*)>(Il2cppUtils::GetMethodPointer("Newtonsoft.Json.dll", "Newtonsoft.Json",
                                                                                                  "JsonConvert", "SerializeObject", { "*" }));
        }
        if (!toJsonStr) {
            return nullptr;
        }
        return toJsonStr(object);
    }

    static void* FromJsonStr(Il2CppString* jsonString, void* targetType) {
        static void* (*fromJsonStr)(Il2CppString*, void*) = nullptr;
        if (!fromJsonStr) {
            fromJsonStr = reinterpret_cast<void* (*)(Il2CppString*, void*)>(Il2cppUtils::GetMethodPointer("Newtonsoft.Json.dll", "Newtonsoft.Json",
                                                                                                          "JsonConvert", "DeserializeObject", { "System.String", "System.Type" }));
        }
        if (!fromJsonStr) {
            return nullptr;
        }
        return fromJsonStr(jsonString, targetType);
    }

    static void* FromJsonStr(const std::string& jsonString, void* targetType) {
        auto il2cppJsonString = Il2CppString::New(jsonString);
        return FromJsonStr(il2cppJsonString, targetType);
    }

    static std::vector<UnityResolve::UnityType::Transform*> GetNestedTransformChildren(
            UnityResolve::UnityType::Transform* parent,
            const std::vector<std::function<bool(const std::string&)>>& predicates
            ) {
        std::vector<UnityResolve::UnityType::Transform*> currentLevel;

        if (parent) {
            currentLevel.push_back(parent);
        }

        for (const auto& predicate : predicates) {
            std::vector<UnityResolve::UnityType::Transform*> nextLevel;

            for (auto* transform : currentLevel) {
                if (!transform) continue;

                for (int i = 0; i < transform->GetChildCount(); i++) {
                    auto child = transform->GetChild(i);
                    if (!child) continue;

                    const auto childName = child->GetName();
                    if (predicate(childName)) {
                        nextLevel.push_back(child);
                    }
                }
            }

            currentLevel = std::move(nextLevel);

            // 如果某一层没有找到任何匹配的子对象，提前退出
            if (currentLevel.empty()) {
                break;
            }
        }

        return currentLevel;
    }

    static void il2cpp_SetGameObjectActive(UnityResolve::UnityType::GameObject* gameObject, bool active) {
        static auto il2cpp_GameObject_SetActive_Injected = reinterpret_cast<void (*)(UnityResolve::UnityType::GameObject*, bool)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.GameObject::SetActive(System.Boolean)"));
        
        if (il2cpp_GameObject_SetActive_Injected && gameObject) {
            il2cpp_GameObject_SetActive_Injected(gameObject, active);
        }
    }

    static bool il2cpp_Renderer_get_enabled(UnityResolve::UnityType::Renderer* renderer) {
        static auto il2cpp_Renderer_get_enabled_Injected = reinterpret_cast<bool (*)(UnityResolve::UnityType::Renderer*)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::get_enabled()"));

        if (il2cpp_Renderer_get_enabled_Injected && renderer) {
            return il2cpp_Renderer_get_enabled_Injected(renderer);
        }
        return false;
    }

    static void il2cpp_Renderer_set_enabled(UnityResolve::UnityType::Renderer* renderer, bool enabled) {
        static auto il2cpp_Renderer_set_enabled_Injected = reinterpret_cast<void (*)(UnityResolve::UnityType::Renderer*, bool)>(
            Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::set_enabled(System.Boolean)"));

        if (il2cpp_Renderer_set_enabled_Injected && renderer) {
            il2cpp_Renderer_set_enabled_Injected(renderer, enabled);
        }
    }

    static bool IsNativeObjectAlive(void* obj) {
        if (!obj) return false;
        static UnityResolve::Method* IsNativeObjectAliveMtd = nullptr;
        if (!IsNativeObjectAliveMtd) IsNativeObjectAliveMtd = Il2cppUtils::GetMethod("UnityEngine.CoreModule.dll", "UnityEngine",
                                                                                     "Object", "IsNativeObjectAlive");
        return IsNativeObjectAliveMtd->Invoke<bool>(obj);
    }

    static UnityResolve::UnityType::Component* GetMeshRenderer(UnityResolve::UnityType::Transform* transform) {
        static auto get_component = reinterpret_cast<UnityResolve::UnityType::Component* (*)(UnityResolve::UnityType::GameObject*, void*)>(
                Il2cppUtils::il2cpp_resolve_icall("UnityEngine.GameObject::GetComponent(System.Type)"));
        static auto get_enabled = reinterpret_cast<bool (*)(UnityResolve::UnityType::Component*)>(
                Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::get_enabled()"));
        static auto rendererType = Il2cppUtils::GetClass("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer");
        if (!(transform && IsNativeObjectAlive(transform))) return nullptr;
        auto gameObject = transform->GetGameObject();
        if (!(gameObject && IsNativeObjectAlive(gameObject))) return nullptr;
        return gameObject->GetComponent<UnityResolve::UnityType::Component*>(rendererType);
    }

    static void SetTransformRenderActive(UnityResolve::UnityType::Transform* transform, bool active, std::string debugName = "", bool debug = true) {
        static auto set_enabled = reinterpret_cast<void (*)(UnityResolve::UnityType::Component*, bool)>(
                Il2cppUtils::il2cpp_resolve_icall("UnityEngine.Renderer::set_enabled(System.Boolean)"));
        auto renderer =GetMeshRenderer(transform);
        if (renderer && IsNativeObjectAlive(renderer)) {
            set_enabled(renderer, active);
        } else if (debug) {
            LinkuraLocal::Log::VerboseFmt("No renderer found for %s", debugName.c_str());
        }
    }

    namespace Tools {

        template <typename T = void*>
        class CSListEditor {
        public:
            CSListEditor(void* list) {
                list_klass = get_class_from_instance(list);
                lst = list;

                lst_get_Count_method = il2cpp_class_get_method_from_name(list_klass, "get_Count", 0);
                lst_get_Item_method = il2cpp_class_get_method_from_name(list_klass, "get_Item", 1);
                lst_set_Item_method = il2cpp_class_get_method_from_name(list_klass, "set_Item", 2);
                lst_Add_method = il2cpp_class_get_method_from_name(list_klass, "Add", 1);
                lst_Contains_method = il2cpp_class_get_method_from_name(list_klass, "Contains", 1);

                lst_get_Count = reinterpret_cast<lst_get_Count_t>(lst_get_Count_method->methodPointer);
                lst_get_Item = reinterpret_cast<lst_get_Item_t>(lst_get_Item_method->methodPointer);
                lst_set_Item = reinterpret_cast<lst_set_Item_t>(lst_set_Item_method->methodPointer);
                lst_Add = reinterpret_cast<lst_Add_t>(lst_Add_method->methodPointer);
                lst_Contains = reinterpret_cast<lst_Contains_t>(lst_Contains_method->methodPointer);
            }

            void Add(T value) {
                lst_Add(lst, value, lst_Add_method);
            }

            bool Contains(T value) {
                return lst_Contains(lst, value, lst_Contains_method);
            }

            T get_Item(int index) {
                return lst_get_Item(lst, index, lst_get_Item_method);
            }

            void set_Item(int index, T value) {
                return lst_set_Item(lst, index, value, lst_set_Item_method);
            }

            int get_Count() {
                return lst_get_Count(lst, lst_get_Count_method);
            }

            T operator[] (int key) {
                return get_Item(key);
            }

            class Iterator {
            public:
                Iterator(CSListEditor<T>* editor, int index) : editor(editor), index(index) {}

                T operator*() const {
                    return editor->get_Item(index);
                }

                Iterator& operator++() {
                    ++index;
                    return *this;
                }

                bool operator!=(const Iterator& other) const {
                    return index != other.index;
                }

            private:
                CSListEditor<T>* editor;
                int index;
            };

            Iterator begin() {
                return Iterator(this, 0);
            }

            Iterator end() {
                return Iterator(this, get_Count());
            }

            void* lst;
            void* list_klass;
        private:
            typedef T(*lst_get_Item_t)(void*, int, void* mtd);
            typedef void(*lst_Add_t)(void*, T, void* mtd);
            typedef void(*lst_set_Item_t)(void*, int, T, void* mtd);
            typedef int(*lst_get_Count_t)(void*, void* mtd);
            typedef bool(*lst_Contains_t)(void*, T, void* mtd);

            MethodInfo* lst_get_Item_method;
            MethodInfo* lst_Add_method;
            MethodInfo* lst_get_Count_method;
            MethodInfo* lst_set_Item_method;
            MethodInfo* lst_Contains_method;

            lst_get_Item_t lst_get_Item;
            lst_set_Item_t lst_set_Item;
            lst_Add_t lst_Add;
            lst_get_Count_t lst_get_Count;
            lst_Contains_t lst_Contains;
        };


        template <typename KT = void*, typename VT = void*>
        class CSDictEditor {
        public:
            // @param dict: Dictionary instance.
            // @param dictTypeStr: Reflection type. eg: "System.Collections.Generic.Dictionary`2[System.Int32, System.Int32]"
            CSDictEditor(void* dict, const char* dictTypeStr) {
                dic_klass = Il2cppUtils::get_system_class_from_reflection_type_str(dictTypeStr);
				initDict(dict);
            }

            CSDictEditor(void* dict) {
				dic_klass = get_class_from_instance(dict);
				initDict(dict);
            }

            CSDictEditor(void* dict, void* dicClass) {
                dic_klass = dicClass;
				initDict(dict);
            }

            void Add(KT key, VT value) {
                dic_Add(dict, key, value, Add_method);
            }

            bool ContainsKey(KT key) {
                return dic_containsKey(dict, key, ContainsKey_method);
            }

            VT get_Item(KT key) {
                return dic_get_Item(dict, key, get_Item_method);
            }

            VT operator[] (KT key) {
                return get_Item(key);
            }

            void* dict;
            void* dic_klass;

        private:
            void initDict(void* dict) {
				// dic_klass = dicClass;
                this->dict = dict;

                get_Item_method = il2cpp_class_get_method_from_name(dic_klass, "get_Item", 1);
                Add_method = il2cpp_class_get_method_from_name(dic_klass, "Add", 2);
                ContainsKey_method = il2cpp_class_get_method_from_name(dic_klass, "ContainsKey", 1);

                dic_get_Item = (dic_get_Item_t)get_Item_method->methodPointer;
                dic_Add = (dic_Add_t)Add_method->methodPointer;
                dic_containsKey = (dic_containsKey_t)ContainsKey_method->methodPointer;
            }

            typedef VT(*dic_get_Item_t)(void*, KT, void* mtd);
            typedef VT(*dic_Add_t)(void*, KT, VT, void* mtd);
            typedef VT(*dic_containsKey_t)(void*, KT, void* mtd);

            CSDictEditor();
            MethodInfo* get_Item_method;
            MethodInfo* Add_method;
            MethodInfo* ContainsKey_method;
            dic_get_Item_t dic_get_Item;
            dic_Add_t dic_Add;
            dic_containsKey_t dic_containsKey;
        };

    }
}
