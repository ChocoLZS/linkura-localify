#include "MasterLocal.h"
#include "Local.h"
#include "Il2cppUtils.hpp"
#include "config/Config.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <regex>
#include <nlohmann/json.hpp>

namespace LinkuraLocal::MasterLocal {
    using Il2cppString = UnityResolve::UnityType::String;

    static std::unordered_map<std::string, Il2cppUtils::MethodInfo*> fieldSetCache;
    static std::unordered_map<std::string, Il2cppUtils::MethodInfo*> fieldGetCache;

    enum class JsonValueType {
        JVT_String,
        JVT_Int,
        JVT_Object,
        JVT_ArrayObject,
        JVT_ArrayString,
        JVT_Unsupported,
        JVT_NeedMore_EmptyArray
    };

    struct ItemRule {
        std::vector<std::string> mainPrimaryKey;
        std::map<std::string, std::vector<std::string>> subPrimaryKey;

        std::vector<std::string> mainLocalKey;
        std::map<std::string, std::vector<std::string>> subLocalKey;
    };

    struct TableLocalData {
        ItemRule itemRule;

        std::unordered_map<std::string, JsonValueType> mainKeyType;
        std::unordered_map<std::string, std::unordered_map<std::string, JsonValueType>> subKeyType;

        std::unordered_map<std::string, std::string> transData;
        std::unordered_map<std::string, std::vector<std::string>> transStrListData;

        [[nodiscard]] JsonValueType GetMainKeyType(const std::string& mainKey) const {
            if (auto it = mainKeyType.find(mainKey); it != mainKeyType.end()) {
                return it->second;
            }
            return JsonValueType::JVT_Unsupported;
        }

        [[nodiscard]] JsonValueType GetSubKeyType(const std::string& parentKey, const std::string& subKey) const {
            if (auto it = subKeyType.find(parentKey); it != subKeyType.end()) {
                if (auto subIt = it->second.find(subKey); subIt != it->second.end()) {
                    return subIt->second;
                }
            }
            return JsonValueType::JVT_Unsupported;
        }
    };

    static std::unordered_map<std::string, TableLocalData> masterLocalData;

    class FieldController {
        void* self;
        std::string self_klass_name;

        static std::string capitalizeFirstLetter(const std::string& input) {
            if (input.empty()) return input;
            std::string result = input;
            result[0] = static_cast<char>(std::toupper(result[0]));
            return result;
        }

        Il2cppUtils::MethodInfo* GetGetSetMethodFromCache(const std::string& fieldName, int argsCount,
                                                          std::unordered_map<std::string, Il2cppUtils::MethodInfo*>& fromCache, const std::string& prefix = "set_") {
            const std::string methodName = prefix + capitalizeFirstLetter(fieldName);
            const std::string searchName = self_klass_name + "." + methodName;

            if (auto it = fromCache.find(searchName); it != fromCache.end()) {
                return it->second;
            }
            auto set_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(
                    self_klass,
                    methodName.c_str(),
                    argsCount
            );
            fromCache.emplace(searchName, set_mtd);
            return set_mtd;
        }

    public:
        Il2cppUtils::Il2CppClassHead* self_klass;

        explicit FieldController(void* from) {
            if (!from) {
                self = nullptr;
                return;
            }
            self = from;
            self_klass = Il2cppUtils::get_class_from_instance(self);
            if (self_klass) {
                self_klass_name = self_klass->name;
            }
        }

        template<typename T>
        T ReadField(const std::string& fieldName) {
            if (!self) return T();
            auto get_mtd = GetGetSetMethodFromCache(fieldName, 0, fieldGetCache, "get_");
            if (get_mtd) {
                return reinterpret_cast<T (*)(void*, void*)>(get_mtd->methodPointer)(self, get_mtd);
            }

            auto field = UnityResolve::Invoke<Il2cppUtils::FieldInfo*>(
                    "il2cpp_class_get_field_from_name",
                    self_klass,
                    (fieldName + '_').c_str()
            );
            if (!field) {
                return T();
            }
            return Il2cppUtils::ClassGetFieldValue<T>(self, field);
        }

        template<typename T>
        void SetField(const std::string& fieldName, T value) {
            if (!self) return;
            auto set_mtd = GetGetSetMethodFromCache(fieldName, 1, fieldSetCache, "set_");
            if (set_mtd) {
                reinterpret_cast<void (*)(void*, T, void*)>(
                        set_mtd->methodPointer
                )(self, value, set_mtd);
                return;
            }
            auto field = UnityResolve::Invoke<Il2cppUtils::FieldInfo*>(
                    "il2cpp_class_get_field_from_name",
                    self_klass,
                    (fieldName + '_').c_str()
            );
            if (!field) return;
            Il2cppUtils::ClassSetFieldValue(self, field, value);
        }

        int ReadIntField(const std::string& fieldName) {
            return ReadField<int>(fieldName);
        }

        Il2cppString* ReadStringField(const std::string& fieldName) {
            if (!self) return nullptr;
            auto get_mtd = GetGetSetMethodFromCache(fieldName, 0, fieldGetCache, "get_");
            if (!get_mtd) {
                return ReadField<Il2cppString*>(fieldName);
            }
            auto returnClass = UnityResolve::Invoke<Il2cppUtils::Il2CppClassHead*>(
                    "il2cpp_class_from_type",
                    UnityResolve::Invoke<void*>("il2cpp_method_get_return_type", get_mtd)
            );
            if (!returnClass) {
                return reinterpret_cast<Il2cppString* (*)(void*, void*)>(
                        get_mtd->methodPointer
                )(self, get_mtd);
            }
            auto isEnum = UnityResolve::Invoke<bool>("il2cpp_class_is_enum", returnClass);
            if (!isEnum) {
                return reinterpret_cast<Il2cppString* (*)(void*, void*)>(
                        get_mtd->methodPointer
                )(self, get_mtd);
            }
            auto enumMap = Il2cppUtils::EnumToValueMap(returnClass, true);
            auto enumValue = reinterpret_cast<int (*)(void*, void*)>(
                    get_mtd->methodPointer
            )(self, get_mtd);
            if (auto it = enumMap.find(enumValue); it != enumMap.end()) {
                return Il2cppString::New(it->second);
            }
            return nullptr;
        }

        void SetStringField(const std::string& fieldName, const std::string& value) {
            if (!self) return;
            auto newString = Il2cppString::New(value);
            SetField(fieldName, newString);
        }

        void SetStringListField(const std::string& fieldName, const std::vector<std::string>& data) {
            if (!self) return;
            static auto List_String_klass = Il2cppUtils::get_system_class_from_reflection_type_str(
                    "System.Collections.Generic.List`1[System.String]"
            );
            static auto List_String_ctor_mtd = Il2cppUtils::il2cpp_class_get_method_from_name(
                    List_String_klass, ".ctor", 0
            );
            static auto List_String_ctor = reinterpret_cast<void (*)(void*, void*)>(
                    List_String_ctor_mtd->methodPointer
            );

            auto newList = UnityResolve::Invoke<void*>("il2cpp_object_new", List_String_klass);
            List_String_ctor(newList, List_String_ctor_mtd);

            Il2cppUtils::Tools::CSListEditor<Il2cppString*> newListEditor(newList);
            for (auto& s : data) {
                newListEditor.Add(Il2cppString::New(s));
            }
            SetField(fieldName, newList);
        }

        void* ReadObjectField(const std::string& fieldName) {
            if (!self) return nullptr;
            return ReadField<void*>(fieldName);
        }

        void* ReadObjectListField(const std::string& fieldName) {
            if (!self) return nullptr;
            return ReadField<void*>(fieldName);
        }

        static FieldController CreateSubFieldController(void* subObj) {
            return FieldController(subObj);
        }

        FieldController CreateSubFieldController(const std::string& subObjName) {
            auto field = ReadObjectField(subObjName);
            return FieldController(field);
        }
    };


    JsonValueType checkJsonValueType(const nlohmann::json& j) {
        if (j.is_string())  return JsonValueType::JVT_String;
        if (j.is_number_integer()) return JsonValueType::JVT_Int;
        if (j.is_object())  return JsonValueType::JVT_Object;
        if (j.is_array()) {
            if (!j.empty()) {
                if (j.begin()->is_object()) {
                    return JsonValueType::JVT_ArrayObject;
                }
                else if (j.begin()->is_string()) {
                    return JsonValueType::JVT_ArrayString;
                }
            }
            else {
                return JsonValueType::JVT_NeedMore_EmptyArray;
            }
        }
        return JsonValueType::JVT_Unsupported;
    }


    std::string ReadFileToString(const std::filesystem::path& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return {};
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        return buffer.str();
    }

    namespace Load {
        std::vector<std::string> ArrayStrJsonToVec(nlohmann::json& data) {
            return data;
        }

        bool BuildObjectItemLocalRule(nlohmann::json& transData, ItemRule& itemRule) {
            // transData: data[]
            bool hasSuccess = false;
            for (auto& data : transData) {
                // data: {"id": "xxx", "produceDescriptions": [{"k", "v"}], "descriptions": {"k2", "v2"}}
                if (!data.is_object()) continue;
                for (auto& [key, value] : data.items()) {
                    // key: "id", value: "xxx"
                    // key: "produceDescriptions", value: [{"k", "v"}]
                    const auto valueType = checkJsonValueType(value);
                    switch (valueType) {
                        case JsonValueType::JVT_String:
                            // case JsonValueType::JVT_Int:
                        case JsonValueType::JVT_ArrayString: {
                            if (std::find(itemRule.mainPrimaryKey.begin(), itemRule.mainPrimaryKey.end(), key) != itemRule.mainPrimaryKey.end()) {
                                continue;
                            }
                            if (auto it = std::find(itemRule.mainLocalKey.begin(), itemRule.mainLocalKey.end(), key); it == itemRule.mainLocalKey.end()) {
                                itemRule.mainLocalKey.emplace_back(key);
                            }
                            hasSuccess = true;
                        } break;

                        case JsonValueType::JVT_Object: {
                            ItemRule currRule{ .mainPrimaryKey = itemRule.subPrimaryKey[key] };

                            auto vJson = nlohmann::json::array();
                            vJson.push_back(value);

                            if (BuildObjectItemLocalRule(vJson, currRule)) {
                                itemRule.subLocalKey.emplace(key, currRule.mainLocalKey);
                                hasSuccess = true;
                            }
                        } break;

                        case JsonValueType::JVT_ArrayObject: {
                            for (auto& obj : value) {
                                // obj: {"k", "v"}
                                ItemRule currRule{ .mainPrimaryKey = itemRule.subPrimaryKey[key] };
                                if (BuildObjectItemLocalRule(value, currRule)) {
                                    itemRule.subLocalKey.emplace(key, currRule.mainLocalKey);
                                    hasSuccess = true;
                                    break;
                                }
                            }
                        } break;

                        case JsonValueType::JVT_Unsupported:
                        default:
                            break;
                    }
                }
                if (hasSuccess) break;
            }
            return hasSuccess;
        }

        bool GetItemRule(nlohmann::json& fullData, ItemRule& itemRule) {
            auto& primaryKeys = fullData["rules"]["primaryKeys"];
            auto& transData = fullData["data"];
            if (!primaryKeys.is_array()) return false;
            if (!transData.is_array()) return false;

            // 首先构造 mainPrimaryKey 规则
            for (auto& pkItem : primaryKeys) {
                if (!pkItem.is_string()) {
                    return false;
                }
                std::string pk = pkItem;
                auto dotCount = std::ranges::count(pk, '.');
                if (dotCount == 0) {
                    itemRule.mainPrimaryKey.emplace_back(pk);
                }
                else if (dotCount == 1) {
                    auto [parentKey, subKey] = Misc::StringFormat::split_once(pk, ".");
                    if (itemRule.subPrimaryKey.contains(parentKey)) {
                        itemRule.subPrimaryKey[parentKey].emplace_back(subKey);
                    }
                    else {
                        itemRule.subPrimaryKey.emplace(parentKey, std::vector<std::string>{subKey});
                    }
                }
                else {
                    Log::ErrorFmt("Unsupported depth: %d", dotCount);
                    continue;
                }
            }
            return BuildObjectItemLocalRule(transData, itemRule);
        }

        std::string BuildBaseMainUniqueKey(nlohmann::json& data, TableLocalData& tableLocalData) {
            try {
                std::string mainBaseUniqueKey;
                for (auto& mainPrimaryKey : tableLocalData.itemRule.mainPrimaryKey) {
                    if (!data.contains(mainPrimaryKey)) {
                        return "";
                    }
                    auto& value = data[mainPrimaryKey];
                    if (value.is_number_integer()) {
                        mainBaseUniqueKey.append(std::to_string(value.get<int>()));
                    }
                    else {
                        mainBaseUniqueKey.append(value);
                    }
                    mainBaseUniqueKey.push_back('|');
                }
                return mainBaseUniqueKey;
            }
            catch (std::exception& e) {
                Log::ErrorFmt("LoadData - BuildBaseMainUniqueKey failed: %s", e.what());
                throw e;
            }
        }

        void BuildBaseObjectSubUniqueKey(nlohmann::json& value, JsonValueType valueType, std::string& currLocalKey) {
            switch (valueType) {
                case JsonValueType::JVT_String:
                    currLocalKey.append(value.get<std::string>());  // p_card-00-acc-0_002|0|produceDescriptions|ProduceDescriptionType_Exam|
                    currLocalKey.push_back('|');
                    break;
                case JsonValueType::JVT_Int:
                    currLocalKey.append(std::to_string(value.get<int>()));
                    currLocalKey.push_back('|');
                    break;
                default:
                    break;
            }
        }

        bool BuildUniqueKeyValue(nlohmann::json& data, TableLocalData& tableLocalData) {
            // 首先处理 main 部分
            const std::string mainBaseUniqueKey = BuildBaseMainUniqueKey(data, tableLocalData);  // p_card-00-acc-0_002|0|
            if (mainBaseUniqueKey.empty()) return false;
            for (auto& mainLocalKey : tableLocalData.itemRule.mainLocalKey) {
                if (!data.contains(mainLocalKey)) continue;
                auto& currLocalValue = data[mainLocalKey];
                auto currUniqueKey = mainBaseUniqueKey + mainLocalKey;  // p_card-00-acc-0_002|0|name
                if (tableLocalData.GetMainKeyType(mainLocalKey) == JsonValueType::JVT_ArrayString) {
                    tableLocalData.transStrListData.emplace(currUniqueKey, ArrayStrJsonToVec(currLocalValue));
                }
                else {
                    tableLocalData.transData.emplace(currUniqueKey, currLocalValue);
                }
            }
            // 然后处理 sub 部分
            /*
            for (const auto& [subPrimaryParentKey, subPrimarySubKeys] : tableLocalData.itemRule.subPrimaryKey) {
                if (!data.contains(subPrimaryParentKey)) continue;

                const std::string subBaseUniqueKey = mainBaseUniqueKey + subPrimaryParentKey + '|';  // p_card-00-acc-0_002|0|produceDescriptions|

                auto subValueType = checkJsonValueType(data[subPrimaryParentKey]);
                std::string currLocalKey = subBaseUniqueKey;  // p_card-00-acc-0_002|0|produceDescriptions|
                switch (subValueType) {
                    case JsonValueType::JVT_Object: {
                        for (auto& subPrimarySubKey : subPrimarySubKeys) {
                            if (!data[subPrimaryParentKey].contains(subPrimarySubKey)) continue;
                            auto& value = data[subPrimaryParentKey][subPrimarySubKey];
                            auto valueType = tableLocalData.GetSubKeyType(subPrimaryParentKey, subPrimarySubKey);
                            BuildBaseObjectSubUniqueKey(value, valueType, currLocalKey);  // p_card-00-acc-0_002|0|produceDescriptions|ProduceDescriptionType_Exam|
                        }
                    } break;
                    case JsonValueType::JVT_ArrayObject: {
                        int currIndex = 0;
                        for (auto& obj : data[subPrimaryParentKey]) {
                            for (auto& subPrimarySubKey : subPrimarySubKeys) {

                            }
                            currIndex++;
                        }
                    } break;
                    default:
                        break;
                }
            }*/

            for (const auto& [subLocalParentKey, subLocalSubKeys] : tableLocalData.itemRule.subLocalKey) {
                if (!data.contains(subLocalParentKey)) continue;

                const std::string subBaseUniqueKey = mainBaseUniqueKey + subLocalParentKey + '|';  // p_card-00-acc-0_002|0|produceDescriptions|
                auto subValueType = checkJsonValueType(data[subLocalParentKey]);
                if (subValueType != JsonValueType::JVT_NeedMore_EmptyArray) {
                    tableLocalData.mainKeyType.emplace(subLocalParentKey, subValueType);  // 在这里插入 subParent 的类型
                }
                switch (subValueType) {
                    case JsonValueType::JVT_Object: {
                        for (auto& localSubKey : subLocalSubKeys) {
                            const std::string currLocalUniqueKey = subBaseUniqueKey + localSubKey;  // p_card-00-acc-0_002|0|produceDescriptions|text
                            if (tableLocalData.GetSubKeyType(subLocalParentKey, localSubKey) == JsonValueType::JVT_ArrayString) {
                                tableLocalData.transStrListData.emplace(currLocalUniqueKey, ArrayStrJsonToVec(data[subLocalParentKey][localSubKey]));
                            }
                            else {
                                tableLocalData.transData.emplace(currLocalUniqueKey, data[subLocalParentKey][localSubKey]);
                            }
                        }
                    } break;
                    case JsonValueType::JVT_ArrayObject: {
                        int currIndex = 0;
                        for (auto& obj : data[subLocalParentKey]) {
                            for (auto& localSubKey : subLocalSubKeys) {
                                std::string currLocalUniqueKey = subBaseUniqueKey;  // p_card-00-acc-0_002|0|produceDescriptions|
                                currLocalUniqueKey.push_back('[');
                                currLocalUniqueKey.append(std::to_string(currIndex));
                                currLocalUniqueKey.append("]|");
                                currLocalUniqueKey.append(localSubKey);  // p_card-00-acc-0_002|0|produceDescriptions|[0]|text

                                if (tableLocalData.GetSubKeyType(subLocalParentKey, localSubKey) == JsonValueType::JVT_ArrayString) {
                                    // if (obj[localSubKey].is_array()) {
                                    tableLocalData.transStrListData.emplace(currLocalUniqueKey, ArrayStrJsonToVec(obj[localSubKey]));
                                }
                                else if (obj[localSubKey].is_string()) {
                                    tableLocalData.transData.emplace(currLocalUniqueKey, obj[localSubKey]);
                                }
                            }
                            currIndex++;
                        }
                    } break;
                    default:
                        break;
                }
            }
            return true;
        }

#define MainKeyTypeProcess() if (!data.contains(mainPrimaryKey)) { Log::ErrorFmt("mainPrimaryKey: %s not found", mainPrimaryKey.c_str()); isFailed = true; break; } \
    auto currType = checkJsonValueType(data[mainPrimaryKey]); \
    if (currType == JsonValueType::JVT_NeedMore_EmptyArray) goto NextLoop; \
    tableLocalData.mainKeyType[mainPrimaryKey] = currType
#define SubKeyTypeProcess() if (!data.contains(subKeyParent)) { Log::ErrorFmt("subKeyParent: %s not found", subKeyParent.c_str()); isFailed = true; break; } \
                for (auto& subKey : subKeys) { \
                    auto& subKeyValue = data[subKeyParent]; \
                    if (subKeyValue.is_object()) { \
                        if (!subKeyValue.contains(subKey)) { \
                            Log::ErrorFmt("subKey: %s not in subKeyParent: %s", subKey.c_str(), subKeyParent.c_str()); isFailed = true; break; \
                        }                                                                                                                                    \
                        auto currType = checkJsonValueType(subKeyValue[subKey]);                                                                             \
                        if (currType == JsonValueType::JVT_NeedMore_EmptyArray) goto NextLoop; \
                        tableLocalData.subKeyType[subKeyParent].emplace(subKey, currType); \
                    } \
                    else if (subKeyValue.is_array()) {                                                                                                       \
                        if (subKeyValue.empty()) goto NextLoop;                                                                                              \
                        for (auto& i : subKeyValue) { \
                            if (!i.is_object()) continue; \
                            if (!i.contains(subKey)) continue;  \
                            auto currType = checkJsonValueType(i[subKey]); \
                            if (currType == JsonValueType::JVT_NeedMore_EmptyArray) goto NextLoop; \
                            tableLocalData.subKeyType[subKeyParent].emplace(subKey, currType); \
                            break; \
                        } \
                    }                                                                                                                                        \
                    else {                                                                                                                                   \
                        goto NextLoop;\
                    } \
                }

        bool GetTableLocalData(nlohmann::json& fullData, TableLocalData& tableLocalData) {
            bool isFailed = false;

            // 首先 Build mainKeyType 和 subKeyType
            for (auto& data : fullData["data"]) {
                if (!data.is_object()) continue;

                for (auto& mainPrimaryKey : tableLocalData.itemRule.mainPrimaryKey) {
                    MainKeyTypeProcess();
                }
                for (auto& mainPrimaryKey : tableLocalData.itemRule.mainLocalKey) {
                    MainKeyTypeProcess();
                }

                for (const auto& [subKeyParent, subKeys] : tableLocalData.itemRule.subPrimaryKey) {
                    SubKeyTypeProcess()

                    if (isFailed) break;
                }
                for (const auto& [subKeyParent, subKeys] : tableLocalData.itemRule.subLocalKey) {
                    SubKeyTypeProcess()
                    if (isFailed) break;
                }
                if (!isFailed) break;
            NextLoop:
                ;
            }

            if (isFailed) return false;

            bool hasSuccess = false;
            // 然后构造 transData
            for (auto& data : fullData["data"]) {
                if (!data.is_object()) continue;
                if (BuildUniqueKeyValue(data, tableLocalData)) {
                    hasSuccess = true;
                }
            }
            if (!hasSuccess) {
                Log::ErrorFmt("BuildUniqueKeyValue failed.");
            }
            return hasSuccess;
        }

        void LoadData() {
            masterLocalData.clear();
            static auto masterDir = Local::GetBasePath() / "local-files" / "masterTrans";
            if (!std::filesystem::is_directory(masterDir)) {
                Log::ErrorFmt("LoadData: not found: %s", masterDir.string().c_str());
                return;
            }

            bool isFirstIteration = true;
            for (auto& p : std::filesystem::directory_iterator(masterDir)) {
                if (isFirstIteration) {
                    auto totalFileCount = std::distance(
                            std::filesystem::directory_iterator(masterDir),
                            std::filesystem::directory_iterator{}
                    );
                    UnityResolveProgress::classProgress.total = totalFileCount <= 0 ? 1 : totalFileCount;
                    isFirstIteration = false;
                }
                UnityResolveProgress::classProgress.current++;

                if (!p.is_regular_file()) continue;
                const auto& path = p.path();
                if (path.extension() != ".json") continue;

                std::string tableName = path.stem().string();
                auto fileContent = ReadFileToString(path);
                if (fileContent.empty()) continue;

                try {
                    auto j = nlohmann::json::parse(fileContent);
                    if (!j.contains("rules") || !j["rules"].contains("primaryKeys")) {
                        continue;
                    }
                    ItemRule currRule;
                    if (!GetItemRule(j, currRule)) {
                        Log::ErrorFmt("GetItemRule failed: %s", path.string().c_str());
                        continue;
                    }

                    /*
                    if (tableName == "ProduceStepEventDetail") {
                        for (auto& i : currRule.mainLocalKey) {
                            Log::DebugFmt("currRule.mainLocalKey: %s", i.c_str());
                        }
                        for (auto& i : currRule.mainPrimaryKey) {
                            Log::DebugFmt("currRule.mainPrimaryKey: %s", i.c_str());
                        }
                        for (auto& i : currRule.subLocalKey) {
                            for (auto& m : i.second) {
                                Log::DebugFmt("currRule.subLocalKey: %s - %s", i.first.c_str(), m.c_str());
                            }
                        }
                        for (auto& i : currRule.subPrimaryKey) {
                            for (auto& m : i.second) {
                                Log::DebugFmt("currRule.subPrimaryKey: %s - %s", i.first.c_str(), m.c_str());
                            }
                        }
                    }*/

                    TableLocalData tableLocalData{ .itemRule = currRule };
                    if (GetTableLocalData(j, tableLocalData)) {
                        for (auto& i : tableLocalData.transData) {
                            // Log::DebugFmt("%s: %s -> %s", tableName.c_str(), i.first.c_str(), i.second.c_str());
                            Local::translatedText.emplace(i.second);
                        }
                        for (auto& i : tableLocalData.transStrListData) {
                            for (auto& str : i.second) {
                                // Log::DebugFmt("%s[]: %s -> %s", tableName.c_str(), i.first.c_str(), str.c_str());
                                Local::translatedText.emplace(str);
                            }
                        }

                        /*
                        if (tableName == "ProduceStepEventDetail") {
                            for (auto& i : tableLocalData.mainKeyType) {
                                Log::DebugFmt("mainKeyType: %s -> %d", i.first.c_str(), i.second);
                            }
                            for (auto& i : tableLocalData.subKeyType) {
                                for (auto& m : i.second) {
                                    Log::DebugFmt("subKeyType: %s - %s -> %d", i.first.c_str(), m.first.c_str(), m.second);
                                }
                            }
                        }*/
                        // JVT_ArrayString in HelpCategory, ProduceStory, Tutorial

                        masterLocalData.emplace(tableName, std::move(tableLocalData));
                    }
                    else {
                        Log::ErrorFmt("GetTableLocalData failed: %s", path.string().c_str());
                    }
                } catch (std::exception& e) {
                    Log::ErrorFmt("MasterLocal::LoadData: parse error in '%s': %s",
                                  path.string().c_str(), e.what());
                }
            }
        }
    }

    void LoadData() {
        return Load::LoadData();
    }

    std::string GetTransString(const std::string& key, const TableLocalData& localData) {
        if (auto it = localData.transData.find(key); it != localData.transData.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<std::string> GetTransArrayString(const std::string& key, const TableLocalData& localData) {
        if (auto it = localData.transStrListData.find(key); it != localData.transStrListData.end()) {
            return it->second;
        }
        return {};
    }

    void LocalizeMasterItem(FieldController& fc, const std::string& tableName) {
        auto it = masterLocalData.find(tableName);
        if (it == masterLocalData.end()) return;
        const auto& localData = it->second;

        // 首先拼 BasePrimaryKey
        std::string baseDataKey;  // p_card-00-acc-0_002|0|
        for (auto& mainPk : localData.itemRule.mainPrimaryKey) {
            auto mainPkType = localData.GetMainKeyType(mainPk);
            switch (mainPkType) {
                case JsonValueType::JVT_Int: {
                    auto readValue = std::to_string(fc.ReadIntField(mainPk));
                    baseDataKey.append(readValue);
                    baseDataKey.push_back('|');
                } break;
                case JsonValueType::JVT_String: {
                    auto readValue = fc.ReadStringField(mainPk);
                    if (!readValue) return;
                    baseDataKey.append(readValue->ToString());
                    baseDataKey.push_back('|');
                } break;
                default:
                    break;
            }
        }

        // 然后本地化 mainLocal
        for (auto& mainLocal : localData.itemRule.mainLocalKey) {
            std::string currSearchKey = baseDataKey;
            currSearchKey.append(mainLocal);  // p_card-00-acc-0_002|0|name
            auto localVType = localData.GetMainKeyType(mainLocal);
            switch (localVType) {
                case JsonValueType::JVT_String: {
                    auto localValue = GetTransString(currSearchKey, localData);
                    if (!localValue.empty()) {
                        fc.SetStringField(mainLocal, localValue);
                    }
                } break;
                case JsonValueType::JVT_ArrayString: {
                    auto localValue = GetTransArrayString(currSearchKey, localData);
                    if (!localValue.empty()) {
                        fc.SetStringListField(mainLocal, localValue);
                    }
                } break;
                default:
                    break;
            }
        }

        // 处理 sub
        for (const auto& [subParentKey, subLocalKeys] : localData.itemRule.subLocalKey) {
            const auto subBaseSearchKey = baseDataKey + subParentKey + '|';  // p_card-00-acc-0_002|0|produceDescriptions|

            const auto subParentType = localData.GetMainKeyType(subParentKey);
            switch (subParentType) {
                case JsonValueType::JVT_Object: {
                    auto subParentField = fc.CreateSubFieldController(subParentKey);
                    for (const auto& subLocalKey : subLocalKeys) {
                        const auto currSearchKey = subBaseSearchKey + subLocalKey;  // p_card-00-acc-0_002|0|produceDescriptions|text
                        auto localKeyType = localData.GetSubKeyType(subParentKey, subLocalKey);
                        if (localKeyType == JsonValueType::JVT_String) {
                            auto setData = GetTransString(currSearchKey, localData);
                            if (!setData.empty()) {
                                subParentField.SetStringField(subLocalKey, setData);
                            }
                        }
                        else if (localKeyType == JsonValueType::JVT_ArrayString) {
                            auto setData = GetTransArrayString(currSearchKey, localData);
                            if (!setData.empty()) {
                                subParentField.SetStringListField(subLocalKey, setData);
                            }
                        }
                    }
                } break;
                case JsonValueType::JVT_ArrayObject: {
                    auto subArrField = fc.ReadObjectListField(subParentKey);
                    if (!subArrField) continue;
                    Il2cppUtils::Tools::CSListEditor<void*> subListEdit(subArrField);
                    auto count = subListEdit.get_Count();
                    for (int idx = 0; idx < count; idx++) {
                        auto currItem = subListEdit.get_Item(idx);
                        if (!currItem) continue;
                        auto currFc = FieldController::CreateSubFieldController(currItem);

                        std::string currSearchBaseKey = subBaseSearchKey;  // p_card-00-acc-0_002|0|produceDescriptions|
                        currSearchBaseKey.push_back('[');
                        currSearchBaseKey.append(std::to_string(idx));
                        currSearchBaseKey.append("]|");  // p_card-00-acc-0_002|0|produceDescriptions|[0]|

                        for (const auto& subLocalKey : subLocalKeys) {
                            std::string currSearchKey = currSearchBaseKey + subLocalKey;  // p_card-00-acc-0_002|0|produceDescriptions|[0]|text

                            auto localKeyType = localData.GetSubKeyType(subParentKey, subLocalKey);

                            /*
                            if (tableName == "ProduceStepEventDetail") {
                                Log::DebugFmt("localKeyType: %d currSearchKey: %s", localKeyType, currSearchKey.c_str());
                            }*/

                            if (localKeyType == JsonValueType::JVT_String) {
                                auto setData = GetTransString(currSearchKey, localData);
                                if (!setData.empty()) {
                                    currFc.SetStringField(subLocalKey, setData);
                                }
                            }
                            else if (localKeyType == JsonValueType::JVT_ArrayString) {
                                auto setData = GetTransArrayString(currSearchKey, localData);
                                if (!setData.empty()) {
                                    currFc.SetStringListField(subLocalKey, setData);
                                }
                            }
                        }
                    }

                } break;
                default:
                    break;
            }
        }

    }

//    void LocalizeMasterItem(void* item, const std::string& tableName) {
//        if (!Config::useMasterTrans) return;
//        // Log::DebugFmt("LocalizeMasterItem: %s", tableName.c_str());
//        FieldController fc(item);
//        LocalizeMasterItem(fc, tableName);
//    }

} // namespace LinkuraLocal::MasterLocal
