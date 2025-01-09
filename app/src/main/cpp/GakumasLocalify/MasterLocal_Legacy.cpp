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

namespace GakumasLocal::MasterLocal {
    using Il2cppString = UnityResolve::UnityType::String;

    enum class JsonValueType {
        JVT_String,
        JVT_Int,
        JVT_Object,
        JVT_ArrayObject,
    };

    struct PKItem {
        std::string topLevel;
        std::string subField;
        JsonValueType topLevelType;
        JsonValueType subFieldType;
    };

    struct TableInfo {
        std::vector<PKItem> pkItems;
        std::unordered_map<std::string, nlohmann::json> dataMap;
    };

    static std::unordered_map<std::string, TableInfo> g_loadedData;
    static std::unordered_map<std::string, Il2cppUtils::MethodInfo*> fieldSetCache;
    static std::unordered_map<std::string, Il2cppUtils::MethodInfo*> fieldGetCache;

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
            self = from;
            self_klass = Il2cppUtils::get_class_from_instance(self);
            if (self_klass) {
                self_klass_name = self_klass->name;
            }
        }

        template<typename T>
        T ReadField(const std::string& fieldName) {
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
            auto newString = Il2cppString::New(value);
            SetField(fieldName, newString);
        }

        void SetStringListField(const std::string& fieldName, const std::vector<std::string>& data) {
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
            return ReadField<void*>(fieldName);
        }

        void* ReadObjectListField(const std::string& fieldName) {
            return ReadField<void*>(fieldName);
        }

        static FieldController CreateSubFieldController(void* subObj) {
            return FieldController(subObj);
        }
    };

    //==============================================================
    // 帮助函数：判断 JSON 字段类型
    //==============================================================
    JsonValueType checkJsonValueType(const nlohmann::json& j) {
        if (j.is_string())  return JsonValueType::JVT_String;
        if (j.is_number_integer()) return JsonValueType::JVT_Int;
        if (j.is_object())  return JsonValueType::JVT_Object;
        if (j.is_array()) {
            if (!j.empty() && j.begin()->is_object()) {
                return JsonValueType::JVT_ArrayObject;
            }
        }
        return JsonValueType::JVT_String;
    }

    //==============================================================
    // 解析 pkName => PKItem
    //==============================================================
    PKItem parsePK(const nlohmann::json& row, const std::string& pkStr) {
        auto pos = pkStr.find('.');
        PKItem item;
        if (pos == std::string::npos) {
            item.topLevel = pkStr;
            item.subField = "";
            if (!row.contains(pkStr)) {
                item.topLevelType = JsonValueType::JVT_String;
            } else {
                item.topLevelType = checkJsonValueType(row[pkStr]);
            }
            item.subFieldType = JsonValueType::JVT_String;
        } else {
            item.topLevel = pkStr.substr(0, pos);
            item.subField = pkStr.substr(pos + 1);
            if (!row.contains(item.topLevel)) {
                item.topLevelType = JsonValueType::JVT_Object;
            } else {
                auto& jTop = row[item.topLevel];
                auto t = checkJsonValueType(jTop);
                if (t == JsonValueType::JVT_Object) {
                    item.topLevelType = JsonValueType::JVT_Object;
                } else if (t == JsonValueType::JVT_ArrayObject) {
                    item.topLevelType = JsonValueType::JVT_ArrayObject;
                } else {
                    item.topLevelType = JsonValueType::JVT_Object;
                }
            }
            item.subFieldType = JsonValueType::JVT_String;
            if (row.contains(item.topLevel)) {
                auto& jTop = row[item.topLevel];
                if (jTop.is_object()) {
                    if (jTop.contains(item.subField)) {
                        item.subFieldType = checkJsonValueType(jTop[item.subField]);
                    }
                } else if (jTop.is_array() && !jTop.empty()) {
                    auto& firstElem = *jTop.begin();
                    if (firstElem.is_object() && firstElem.contains(item.subField)) {
                        item.subFieldType = checkJsonValueType(firstElem[item.subField]);
                    }
                }
            }
        }
        return item;
    }

    std::vector<PKItem> parseAllPKItems(const nlohmann::json& row, const std::vector<std::string>& pkNames) {
        std::vector<PKItem> result;
        result.reserve(pkNames.size());
        for (auto& pk : pkNames) {
            auto item = parsePK(row, pk);
            result.push_back(item);
        }
        return result;
    }

    //==============================================================
    // 将 jval 拼接到 uniqueKey
    //==============================================================
    inline void appendPKValue(std::string& uniqueKey, const nlohmann::json& jval, bool& isFirst) {
        if (!isFirst) uniqueKey += "|";
        if (jval.is_string()) {
            uniqueKey += jval.get<std::string>();
        } else if (jval.is_number_integer()) {
            uniqueKey += std::to_string(jval.get<int>());
        }
        isFirst = false;
    }

    //==============================================================
    // 读取文件 => 解析 => 加载 dataMap
    //==============================================================
    std::string ReadFileToString(const std::filesystem::path& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return {};
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        return buffer.str();
    }

    // 判断 row 里，与 pkNames/主键相关的字段（若是数组）是否为空
    bool hasEmptyArrayForPk(const nlohmann::json& row, const std::vector<std::string>& pkNames) {
        // 如果行为空，直接返回 false（或 true，看你需求）
        if (row.is_null() || !row.is_object()) {
            return false;
        }

        for (auto& pk : pkNames) {
            // 先看该行是否包含此顶层字段
            auto dotPos = pk.find('.');
            std::string topLevel = (dotPos == std::string::npos) ? pk : pk.substr(0, dotPos);

            if (!row.contains(topLevel)) {
                // 没有这个字段就略过
                continue;
            }

            // 如果 pk 中含 '.', 说明可能是 array<object> 类型
            // 这里仅检查 "顶层字段是否是空数组"
            // 若需要更深层的判断，需扩展
            const auto& jTop = row[topLevel];
            if (jTop.is_array()) {
                // 一旦发现是空数组，就返回 true
                if (jTop.empty()) {
                    return true;
                }
            }
        }

        return false;
    }

    // 根据 pkItems 构造一个 skipSet，里面包含 "topLevel" 和 "topLevel.subField"
// 或者只包含 subField, 看你具体需求
    static std::unordered_set<std::string> buildSkipFields(const std::vector<PKItem>& pkItems) {
        std::unordered_set<std::string> skipSet;
        for (auto& pk : pkItems) {
            if (pk.subField.empty()) {
                // e.g. "id"
                skipSet.insert(pk.topLevel);
            } else {
                // e.g. "descriptions.type" => 既要跳过 "type" 又要跳过 "descriptions"?
                // 具体看你业务需要:
                // skipSet.insert(pk.topLevel);  // 可能不需要
                skipSet.insert(pk.subField);   // "type"
            }
        }
        return skipSet;
    }

    // 递归枚举 JSON 值里的字符串并插入到 localSet
    void collectLocalizableStrings_impl(
            const nlohmann::json& node,
            const std::unordered_set<std::string>& skipSet,
            std::unordered_set<std::string>& localSet
    ) {
        if (node.is_string()) {
            // node本身就是string => 这时无法知道key名，但一般情况下我们是key->value对？
            // 这里仅当外层调用传入一个object时可取到key
            // 先写成仅object字段时处理
            return;
        }
        if (node.is_object()) {
            // 枚举键值
            for (auto it = node.begin(); it != node.end(); ++it) {
                auto& key = it.key();
                auto& val = it.value();
                // 如果key在skipSet里，则跳过
                if (skipSet.count(key)) {
                    continue;
                }
                // 否则看val的类型
                if (val.is_string()) {
                    // 收集
                    localSet.insert(val.get<std::string>());
                    // Log::DebugFmt("localSet.insert: %s", val.get<std::string>().c_str());
                } else if (val.is_object() || val.is_array()) {
                    // 递归下去
                    collectLocalizableStrings_impl(val, skipSet, localSet);
                }
                // 其他类型 (int/bool/float) 不做本地化
            }
        } else if (node.is_array()) {
            // 枚举数组元素
            for (auto& element : node) {
                if (element.is_string()) {
                    localSet.insert(element.get<std::string>());
                    // Log::DebugFmt("localSet.insert: %s", element.get<std::string>().c_str());
                } else if (element.is_object() || element.is_array()) {
                    collectLocalizableStrings_impl(element, skipSet, localSet);
                }
            }
        }
    }

    // 对外接口：根据 row + pkItems，把所有非主键字段的字符串插到 localSet
    void collectLocalizableStrings(const nlohmann::json& row, const std::vector<PKItem>& pkItems, std::unordered_set<std::string>& localSet) {
        // 先构建一个 skipSet，表示"主键字段"要跳过
        auto skipSet = buildSkipFields(pkItems);
        // 然后递归遍历
        collectLocalizableStrings_impl(row, skipSet, localSet);
    }


    void LoadData() {
        g_loadedData.clear();
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
                std::vector<std::string> pkNames;
                for (auto& x : j["rules"]["primaryKeys"]) {
                    pkNames.push_back(x.get<std::string>());
                }
                if (!j.contains("data") || !j["data"].is_array()) {
                    continue;
                }

                TableInfo tableInfo;
                if (!j["data"].empty()) {
                    for (auto & currRow : j["data"]) {
                        if (!hasEmptyArrayForPk(currRow, pkNames)) {
                            tableInfo.pkItems = parseAllPKItems(currRow, pkNames);
                        }
                    }
                    // auto& firstRow = j["data"][0];
                    // tableInfo.pkItems = parseAllPKItems(firstRow, pkNames);
                }

                //==============================================================
                // 构建 dataMap, 支持 array + index
                //==============================================================
                for (auto& row : j["data"]) {
                    std::string uniqueKey;
                    bool firstKey = true;
                    bool failed = false;

                    for (auto& pkItem : tableInfo.pkItems) {
                        if (!row.contains(pkItem.topLevel)) {
                            failed = true;
                            break;
                        }
                        auto& jTop = row[pkItem.topLevel];

                        // 无子字段 => 直接处理
                        if (pkItem.subField.empty()) {
                            if (jTop.is_string() || jTop.is_number_integer()) {
                                appendPKValue(uniqueKey, jTop, firstKey);
                            } else {
                                failed = true; break;
                            }
                        }
                        else {
                            // 若是 array<object> + subField，就遍历数组每个下标 + subField => 并将 index + value 拼进 uniqueKey
                            if (pkItem.topLevelType == JsonValueType::JVT_ArrayObject) {
                                if (!jTop.is_array()) { failed = true; break; }
                                // 遍历数组所有元素
                                for (int i = 0; i < (int)jTop.size(); i++) {
                                    auto& elem = jTop[i];
                                    if (!elem.is_object()) { failed = true; break; }
                                    if (!elem.contains(pkItem.subField)) { failed = true; break; }
                                    auto& subVal = elem[pkItem.subField];
                                    // 只支持 string/int
                                    if (!subVal.is_string() && !subVal.is_number_integer()) {
                                        failed = true; break;
                                    }
                                    // 拼上索引 + 值
                                    // e.g. "|0:xxx|1:yyy"...
                                    if (!firstKey) uniqueKey += "|";
                                    uniqueKey += std::to_string(i);
                                    uniqueKey += ":";
                                    if (subVal.is_string()) {
                                        uniqueKey += subVal.get<std::string>();
                                    } else {
                                        uniqueKey += std::to_string(subVal.get<int>());
                                    }
                                    firstKey = false;
                                }
                                if (failed) break;
                            }
                            else if (pkItem.topLevelType == JsonValueType::JVT_Object) {
                                if (!jTop.is_object()) {
                                    failed = true;
                                    break;
                                }
                                if (!jTop.contains(pkItem.subField)) { failed = true; break; }
                                auto& subVal = jTop[pkItem.subField];
                                if (subVal.is_string() || subVal.is_number_integer()) {
                                    appendPKValue(uniqueKey, subVal, firstKey);
                                } else {
                                    failed = true; break;
                                }
                            }
                            else {
                                failed = true;
                                break;
                            }
                        }
                        if (failed) break;
                    }
                    if (!failed && !uniqueKey.empty()) {
                        tableInfo.dataMap[uniqueKey] = row;
                        collectLocalizableStrings(row, tableInfo.pkItems, Local::translatedText);
                    }
                }

                // Log::DebugFmt("Load table: %s, %d, %d", tableName.c_str(), tableInfo.pkItems.size(), tableInfo.dataMap.size());
                g_loadedData[tableName] = std::move(tableInfo);

            } catch (std::exception& e) {
                Log::ErrorFmt("MasterLocal::LoadData: parse error in '%s': %s",
                              path.string().c_str(), e.what());
            }
        }
    }

    //==============================================================
    // 在 C# 对象里，根据 pkItems 构造 uniqueKey
    // 同样要支持 array<object> + index
    //==============================================================
    bool buildUniqueKeyFromCSharp(FieldController& fc, const TableInfo& tableInfo, std::string& outKey) {
        outKey.clear();
        bool firstKey = true;

        for (auto& pk : tableInfo.pkItems) {
            if (pk.subField.empty()) {
                // 顶层无子字段
                if (pk.topLevelType == JsonValueType::JVT_String) {
                    auto sptr = fc.ReadStringField(pk.topLevel);
                    if (!sptr) return false;
                    if (!firstKey) outKey += "|";
                    outKey += sptr->ToString();
                    firstKey = false;
                } else if (pk.topLevelType == JsonValueType::JVT_Int) {
                    int ival = fc.ReadIntField(pk.topLevel);
                    if (!firstKey) outKey += "|";
                    outKey += std::to_string(ival);
                    firstKey = false;
                } else {
                    return false;
                }
            }
            else {
                // subField
                if (pk.topLevelType == JsonValueType::JVT_ArrayObject) {
                    // => c# 里 readObjectListField
                    void* listPtr = fc.ReadObjectListField(pk.topLevel);
                    if (!listPtr) return false;
                    Il2cppUtils::Tools::CSListEditor<void*> listEdit(listPtr);
                    int arrCount = listEdit.get_Count();

                    // 遍历每个 index
                    for (int i = 0; i < arrCount; i++) {
                        auto elemPtr = listEdit.get_Item(i);
                        if (!elemPtr) return false;
                        FieldController subFC = FieldController::CreateSubFieldController(elemPtr);

                        // 只支持 string/int
                        if (pk.subFieldType == JsonValueType::JVT_String) {
                            auto sptr = subFC.ReadStringField(pk.subField);
                            if (!sptr) return false;
                            if (!firstKey) outKey += "|";
                            // "|i:xxx"
                            outKey += std::to_string(i);
                            outKey += ":";
                            outKey += sptr->ToString();
                            firstKey = false;
                        }
                        else if (pk.subFieldType == JsonValueType::JVT_Int) {
                            int ival = subFC.ReadIntField(pk.subField);
                            if (!firstKey) outKey += "|";
                            outKey += std::to_string(i);
                            outKey += ":";
                            outKey += std::to_string(ival);
                            firstKey = false;
                        } else {
                            return false;
                        }
                    }
                }
                else if (pk.topLevelType == JsonValueType::JVT_Object) {
                    void* subObj = fc.ReadObjectField(pk.topLevel);
                    if (!subObj) return false;
                    FieldController subFC = FieldController::CreateSubFieldController(subObj);

                    if (pk.subFieldType == JsonValueType::JVT_String) {
                        auto sptr = subFC.ReadStringField(pk.subField);
                        if (!sptr) return false;
                        if (!firstKey) outKey += "|";
                        outKey += sptr->ToString();
                        firstKey = false;
                    }
                    else if (pk.subFieldType == JsonValueType::JVT_Int) {
                        int ival = subFC.ReadIntField(pk.subField);
                        if (!firstKey) outKey += "|";
                        outKey += std::to_string(ival);
                        firstKey = false;
                    }
                    else {
                        return false;
                    }
                }
                else {
                    return false;
                }
            }
        }
        return !outKey.empty();
    }

    // 声明
    void localizeJsonToCsharp(FieldController& fc, const nlohmann::json& jdata, const std::unordered_set<std::string>& skipKeySet);
    void localizeArrayOfObject(FieldController& fc, const std::string& fieldName, const nlohmann::json& arrVal, const std::unordered_set<std::string>& skipKeySet);
    void localizeObject(FieldController& fc, const std::string& fieldName, const nlohmann::json& objVal, const std::unordered_set<std::string>& skipKeySet);

    //====================================================================
    // 对 array<object> 做一层递归 —— 需要带着 skipKeySet
    //====================================================================
    void localizeArrayOfObject(FieldController& fc, const std::string& fieldName, const nlohmann::json& arrVal, const std::unordered_set<std::string>& skipKeySet) {
        void* listPtr = fc.ReadObjectListField(fieldName);
        if (!listPtr) return;
        Il2cppUtils::Tools::CSListEditor<void*> listEdit(listPtr);
        int cmin = std::min<int>(listEdit.get_Count(), (int)arrVal.size());
        for (int i = 0; i < cmin; i++) {
            auto elemPtr = listEdit.get_Item(i);
            if (!elemPtr) continue;
            FieldController subFC = FieldController::CreateSubFieldController(elemPtr);
            localizeJsonToCsharp(subFC, arrVal[i], skipKeySet);
        }
    }

    //====================================================================
    // 对单个 object 做一层递归 —— 需要带着 skipKeySet
    //====================================================================
    void localizeObject(FieldController& fc, const std::string& fieldName, const nlohmann::json& objVal, const std::unordered_set<std::string>& skipKeySet) {
        void* subObj = fc.ReadObjectField(fieldName);
        if (!subObj) return;
        FieldController subFC = FieldController::CreateSubFieldController(subObj);
        localizeJsonToCsharp(subFC, objVal, skipKeySet);
    }

    //====================================================================
    // 仅一层本地化: string, string[], object, object[]，带 skipKeySet
    //====================================================================
    void localizeJsonToCsharp(FieldController& fc, const nlohmann::json& jdata, const std::unordered_set<std::string>& skipKeySet) {
        if (!jdata.is_object()) return;
        for (auto it = jdata.begin(); it != jdata.end(); ++it) {
            const std::string& key = it.key();
            // 如果 key 在 skipKeySet 里，则跳过本地化
            if (skipKeySet.count(key)) {
                // Debug输出可以留意一下
                // Log::DebugFmt("skip field: %s", key.c_str());
                continue;
            }

            const auto& val = it.value();
            if (val.is_string()) {
                // 打印一下做验证
                auto origStr = fc.ReadStringField(key);
                auto newStr = val.get<std::string>();
                if (origStr) {
                    std::string oldVal = origStr->ToString();
                    // Log::DebugFmt("SetStringField key: %s, oldVal: %s -> newVal: %s", key.c_str(), oldVal.c_str(), newStr.c_str());
                    if (((oldVal == "\n") || (oldVal == "\r\n")) && newStr.empty()) {
                        continue;
                    }
                }
                fc.SetStringField(key, val.get<std::string>());
            }
            else if (val.is_array()) {
                if (!val.empty() && val.begin()->is_string()) {
                    bool allStr = true;
                    std::vector<std::string> strArray;
                    for (auto& x : val) {
                        if (!x.is_string()) { allStr = false; break; }
                        strArray.push_back(x.get<std::string>());
                    }
                    if (allStr) {
                        // Log::DebugFmt("SetStringListField in %s, key: %s", fc.self_klass->name, key.c_str());
                        fc.SetStringListField(key, strArray);
                        continue;
                    }
                }
                // array<object>
                if (!val.empty() && val.begin()->is_object()) {
                    localizeArrayOfObject(fc, key, val, skipKeySet);
                }
            }
            else if (val.is_object()) {
                localizeObject(fc, key, val, skipKeySet);
            }
        }
    }

    //====================================================================
    // 真正处理单个C#对象
    //====================================================================
    void LocalizeMasterItem(FieldController& fc, const std::string& tableName) {
        auto it = g_loadedData.find(tableName);
        if (it == g_loadedData.end()) return;
        // Log::DebugFmt("LocalizeMasterItem: %s", tableName.c_str());
        auto& tableInfo = it->second;
        if (tableInfo.dataMap.empty()) {
            return;
        }

        std::string uniqueKey;
        if (!buildUniqueKeyFromCSharp(fc, tableInfo, uniqueKey)) {
            return;
        }

        auto itRow = tableInfo.dataMap.find(uniqueKey);
        if (itRow == tableInfo.dataMap.end()) {
            return;
        }
        const auto& rowData = itRow->second;

        //=====================================================
        // 把「有子字段」的 pkItem 也加入 skipKeySet，但用它的 `subField` 部分
        //=====================================================
        std::unordered_set<std::string> skipKeySet;
        for (auto& pk : tableInfo.pkItems) {
            if (pk.subField.empty()) {
                // 若没有子字段，说明 topLevel 本身是主键
                skipKeySet.insert(pk.topLevel);
            } else {
                // 如果有子字段，说明这个子字段才是 PK
                // e.g. produceDescriptions.examEffectType => skipKeySet.insert("examEffectType");
                skipKeySet.insert(pk.subField);
            }
        }

        // 然后带着 skipKeySet 去做本地化
        localizeJsonToCsharp(fc, rowData, skipKeySet);
    }

    void LocalizeMasterTables(const std::string& tableName, UnityResolve::UnityType::List<void*>* result) {
        if (!result) return;
        Il2cppUtils::Tools::CSListEditor resultList(result);
        if (resultList.get_Count() <= 0) return;

        for (auto i : resultList) {
            if (!i) continue;
            FieldController fc(i);
            LocalizeMasterItem(fc, tableName);
        }
    }

    void LocalizeMaster(const std::string& sql, UnityResolve::UnityType::List<void*>* result) {
        static const std::regex tableNameRegex(R"(\bFROM\s+(?:`([^`]+)`|(\S+)))");
        std::smatch match;
        if (std::regex_search(sql, match, tableNameRegex)) {
            std::string tableName = match[1].matched ? match[1].str() : match[2].str();
            LocalizeMasterTables(tableName, result);
        }
    }

    void LocalizeMaster(const std::string& sql, void* result) {
        if (!Config::useMasterTrans) return;
        LocalizeMaster(sql, reinterpret_cast<UnityResolve::UnityType::List<void*>*>(result));
    }

    void LocalizeMaster(void* result, const std::string& tableName) {
        if (!Config::useMasterTrans) return;
        LocalizeMasterTables(tableName, reinterpret_cast<UnityResolve::UnityType::List<void*>*>(result));
    }

    void LocalizeMasterItem(void* item, const std::string& tableName) {
        if (!Config::useMasterTrans) return;
        FieldController fc(item);
        LocalizeMasterItem(fc, tableName);
    }

} // namespace GakumasLocal::MasterLocal
