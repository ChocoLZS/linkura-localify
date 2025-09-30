#include "Local.h"
#include "Log.h"
#include "Plugin.h"
#include "config/Config.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <thread>
#include <regex>
#include <ranges>
#include <string>
#include <cctype>
#include <algorithm>
#include <re2/re2.h>
#include "BaseDefine.h"
#include "string_parser/StringParser.hpp"

// #include "cpprest/details/http_helpers.h"


namespace LinkuraLocal::Local {
    std::unordered_map<std::string, std::string> i18nData{};
    std::unordered_map<std::string, std::string> i18nDumpData{};
    std::unordered_map<std::string, std::string> genericText{};
    std::unordered_map<std::string, std::string> masterText{};
    std::unordered_map<std::string, std::string> genericSplitText{};
    std::unordered_map<std::string, std::string> genericFmtText{};
    std::vector<std::string> genericTextDumpData{};
    std::vector<std::string> genericSplittedDumpData{};
    std::vector<std::string> genericOrigTextDumpData{};
    std::vector<std::string> genericFmtTextDumpData{};

    // 正则表达式匹配存储结构
    struct RegexTranslationItem {
        std::unique_ptr<re2::RE2> regex;
        std::string translation;
        std::string originalPattern;
        // for debug output temp
        std::string originalKey;
        std::string originalValue;

        RegexTranslationItem(const std::string& pattern, const std::string& trans, const std::string& key, const std::string& value)
            : translation(trans), originalPattern(pattern), originalKey(key), originalValue(value) {
            regex = std::make_unique<re2::RE2>(pattern);
        }

        // 禁用拷贝构造和赋值
        RegexTranslationItem(const RegexTranslationItem&) = delete;
        RegexTranslationItem& operator=(const RegexTranslationItem&) = delete;

        // 允许移动构造和赋值
        RegexTranslationItem(RegexTranslationItem&&) = default;
        RegexTranslationItem& operator=(RegexTranslationItem&&) = default;
    };

    std::vector<RegexTranslationItem> regexText{};

    std::unordered_set<std::string> translatedText{};
    int genericDumpFileIndex = 0;
    const std::string splitTextPrefix = "[__split__]";

    std::filesystem::path GetBasePath() {
        return Plugin::GetInstance().GetHookInstaller()->localizationFilesDir;
    }

    std::string trim(const std::string& str) {
        auto is_not_space = [](char ch) { return !std::isspace(ch); };
        auto start = std::ranges::find_if(str, is_not_space);
        auto end = std::ranges::find_if(str | std::views::reverse, is_not_space).base();

        if (start < end) {
            return {start, end};
        }
        return "";
    }

    std::string ConvertToRegexPattern(const std::string& text) {
        if (text.find('{') == std::string::npos) {
            return "";
        }

        // 检查是否包含时间格式化占位符，如果是则跳过
        // 匹配包含日期时间格式的占位符：MM、dd、HH、mm、yyyy、'分'、'時間'、'日' 等
        static const std::regex timeFormatRegex(R"(\{[^}]*(?:MM|dd|HH|mm|yyyy|'[^']*'|[HMmdyh]'[^']*')[^}]*\})");
        if (std::regex_search(text, timeFormatRegex)) {
            return "";
        }

        std::string pattern = text;

        // 转义正则表达式特殊字符，但保留 {} 用于后续处理
        static const std::string specialChars = "\\^$.|?*+()[]";
        for (char c : specialChars) {
            std::string from(1, c);
            std::string to = "\\" + from;
            size_t pos = 0;
            while ((pos = pattern.find(from, pos)) != std::string::npos) {
                pattern.replace(pos, 1, to);
                pos += 2;
            }
        }

        // 将简单的占位符替换为正则表达式：{数字}、{数字:f}、{数字:F1}、{数字:N0} 等
        // 直接使用占位符右边的第一个字符来构建 [^字符]+ 模式
        std::string tempPattern = pattern;
        static const std::regex placeholderRegex(R"(\{(\d+)(?::[fFNndDpPcCgGxX]\d*)?\})");

        std::string result = "";
        std::string::const_iterator searchStart(tempPattern.cbegin());
        std::smatch match;

        while (std::regex_search(searchStart, tempPattern.cend(), match, placeholderRegex)) {
            result += match.prefix().str();

            // 查看占位符后面的字符来决定匹配模式
            std::string suffix = match.suffix().str();
            if (!suffix.empty()) {
                // 提取第一个UTF-8字符（可能是多字节）
                std::string nextChar;
                unsigned char first_byte = static_cast<unsigned char>(suffix[0]);

                if (first_byte < 0x80) {
                    // ASCII字符（单字节）
                    nextChar = suffix.substr(0, 1);
                } else if ((first_byte & 0xE0) == 0xC0) {
                    // 2字节UTF-8字符
                    nextChar = suffix.substr(0, std::min(2, static_cast<int>(suffix.length())));
                } else if ((first_byte & 0xF0) == 0xE0) {
                    // 3字节UTF-8字符（大部分日文字符）
                    nextChar = suffix.substr(0, std::min(3, static_cast<int>(suffix.length())));
                } else if ((first_byte & 0xF8) == 0xF0) {
                    // 4字节UTF-8字符
                    nextChar = suffix.substr(0, std::min(4, static_cast<int>(suffix.length())));
                } else {
                    // 无效UTF-8，回退到单字节
                    nextChar = suffix.substr(0, 1);
                }

                // 转义特殊的正则表达式字符用于字符类
                std::string escapedChar = nextChar;
                // 对于字符类中的特殊字符进行转义
                if (nextChar == "\\" || nextChar == "^" || nextChar == "-" || nextChar == "]") {
                    escapedChar = "\\" + nextChar;
                }

                // 使用适合RE2的匹配模式
                if (first_byte < 0x80) {
                    // ASCII字符可以安全地放在字符类中
                    result += "([^" + escapedChar + "]+)";
                } else {
                    // 对于UTF-8字符，使用贪婪匹配 + 手动限制
                    // 由于RE2不支持先行断言，使用更通用的模式
                    result += "(.*?)";
                }
            } else {
                // 如果是最后一个占位符，使用通用匹配
                result += "(.+)";
            }

            searchStart = match.suffix().first;
        }
        result += std::string(searchStart, tempPattern.cend());

        return result;
    }

    // 使用 [@数字] 作为分割点分割文本
    std::vector<std::string> SplitByMentions(const std::string& text) {
        std::vector<std::string> segments;

//        Log::InfoFmt("SplitByMentions input text: [%s]", text.c_str());

        // 使用 RE2::GlobalReplace 的思路，先找到所有分割点
        static const RE2 splitPattern(R"(\[@\d+\])");
//        Log::InfoFmt("Using regex pattern: \\[@\\d+\\]");

        // 先测试正则表达式是否有效
        if (!splitPattern.ok()) {
//            Log::ErrorFmt("Regex pattern compilation failed: %s", splitPattern.error().c_str());
            segments.push_back(text);
            return segments;
        }

        // 测试简单匹配
//        if (RE2::PartialMatch(text, splitPattern)) {
//            Log::InfoFmt("PartialMatch found [@数字] pattern in text");
//        } else {
//            Log::InfoFmt("PartialMatch did NOT find [@数字] pattern in text");
//        }

        // 手动查找所有匹配位置，使用标准的字符串查找
        std::vector<std::pair<size_t, size_t>> matches; // (start_pos, length)

        // 用字符串搜索来查找所有 [@数字] 模式
        size_t pos = 0;
        while ((pos = text.find("[@", pos)) != std::string::npos) {
            // 找到 [@，现在检查后面是否跟着数字和 ]
            size_t start_pos = pos;
            pos += 2; // 跳过 [@

            // 检查是否有数字
            size_t digit_start = pos;
            while (pos < text.length() && std::isdigit(text[pos])) {
                pos++;
            }

            // 检查是否有至少一个数字，并且以 ] 结尾
            if (pos > digit_start && pos < text.length() && text[pos] == ']') {
                // 找到完整的 [@数字] 模式
                size_t match_length = pos - start_pos + 1; // +1 包括 ]
                std::string found_pattern = text.substr(start_pos, match_length);

                matches.push_back({start_pos, match_length});
//                Log::InfoFmt("Found [@数字] pattern: [%s] at position %zu", found_pattern.c_str(), start_pos);

                pos++; // 跳过 ]，继续搜索
            } else {
                // 不是有效的 [@数字] 模式，继续搜索下一个 [@
                pos = start_pos + 1;
            }
        }

        // 按位置排序匹配结果
        std::sort(matches.begin(), matches.end());
//        Log::InfoFmt("Total [@数字] patterns found: %zu", matches.size());

        if (matches.empty()) {
            // 没有找到分割点，返回整个文本
            segments.push_back(text);
//            Log::InfoFmt("No [@数字] patterns found, returning whole text");
            return segments;
        }

        // 根据匹配位置分割文本
        size_t last_pos = 0;

        for (const auto& match_info : matches) {
            size_t match_start = match_info.first;
            size_t match_length = match_info.second;

            // 添加分割点之前的部分
            if (match_start > last_pos) {
                std::string segment = text.substr(last_pos, match_start - last_pos);
                if (!segment.empty()) {
                    segments.push_back(segment);
//                    Log::InfoFmt("Added segment: [%s]", segment.c_str());
                }
            }

            // 跳过分割标记
            last_pos = match_start + match_length;
        }

        // 添加最后剩余的部分
        if (last_pos < text.length()) {
            std::string segment = text.substr(last_pos);
            if (!segment.empty()) {
                segments.push_back(segment);
//                Log::InfoFmt("Added final segment: [%s]", segment.c_str());
            }
        }

//        Log::InfoFmt("Total segments created: %zu", segments.size());
        return segments;
    }

    // 处理 BeginnerMissionsHint.json 的特殊分割逻辑
    void ProcessBeginnerMissionsHint(std::unordered_map<std::string, std::string>& dict, const std::string& key, const std::string& value) {
//        Log::InfoFmt("ProcessBeginnerMissionsHint called with key: [%s]", key.c_str());
//        Log::InfoFmt("ProcessBeginnerMissionsHint called with value: [%s]", value.c_str());

        // 检查key是否包含 [@数字] 模式
        static const RE2 mentionPattern(R"(\[@\d+\])");
        bool hasPattern = RE2::PartialMatch(key, mentionPattern);
//        Log::InfoFmt("Key contains [@数字] pattern: %s", hasPattern ? "YES" : "NO");

        if (!hasPattern) {
            return; // 如果key没有 [@数字] 模式，不需要特殊处理
        }

        // 分割key和value
        std::vector<std::string> keySegments = SplitByMentions(key);
        std::vector<std::string> valueSegments = SplitByMentions(value);

//        Log::InfoFmt("Key split into %zu segments", keySegments.size());
//        for (size_t i = 0; i < keySegments.size(); ++i) {
//            Log::InfoFmt("Key segment %zu: [%s]", i, keySegments[i].c_str());
//        }
//
//        Log::InfoFmt("Value split into %zu segments", valueSegments.size());
//        for (size_t i = 0; i < valueSegments.size(); ++i) {
//            Log::InfoFmt("Value segment %zu: [%s]", i, valueSegments[i].c_str());
//        }

        // 确保key和value的段落数量相同
        if (keySegments.size() > 1 && keySegments.size() == valueSegments.size()) {
//            Log::InfoFmt("Processing %zu segment pairs", keySegments.size());
            for (size_t i = 0; i < keySegments.size(); ++i) {
                std::string keySegment = keySegments[i];
                std::string valueSegment = valueSegments[i];

//                // 清理段落：移除开头和结尾的多余换行符
//                while (keySegment.starts_with("\\n")) {
//                    keySegment = keySegment.substr(2);
//                }
//                while (keySegment.ends_with("\\n")) {
//                    keySegment = keySegment.substr(0, keySegment.length() - 2);
//                }
//                while (valueSegment.starts_with("\\n")) {
//                    valueSegment = valueSegment.substr(2);
//                }
//                while (valueSegment.ends_with("\\n")) {
//                    valueSegment = valueSegment.substr(0, valueSegment.length() - 2);
//                }

                if (!keySegment.empty() && !valueSegment.empty()) {
                    // 将分割后的段落对应存入字典
                    dict[keySegment] = valueSegment;
//                    Log::InfoFmt("BeginnerMissionsHint split segment %zu: [%s] -> [%s]", i, keySegment.c_str(), valueSegment.c_str());
                } else {
//                    Log::WarnFmt("Skipping empty segment %zu (key empty: %s, value empty: %s)", i, keySegment.empty() ? "YES" : "NO", valueSegment.empty() ? "YES" : "NO");
                }
            }
        } else if (keySegments.size() > 1) {
//            Log::WarnFmt("BeginnerMissionsHint: key and value segment count mismatch (key: %zu, value: %zu)", keySegments.size(), valueSegments.size());
        } else {
//            Log::InfoFmt("Key has only %zu segments, not splitting", keySegments.size());
        }
    }


    std::string findInMapIgnoreSpace(const std::string& key, const std::unordered_map<std::string, std::string>& searchMap) {
        auto is_space = [](char ch) { return std::isspace(ch); };
        auto front = std::ranges::find_if_not(key, is_space);
        auto back = std::ranges::find_if_not(key | std::views::reverse, is_space).base();

        std::string prefix(key.begin(), front);
        std::string suffix(back, key.end());

        std::string trimmedKey = trim(key);
        if ( auto it = searchMap.find(trimmedKey); it != searchMap.end()) {
            return prefix + it->second + suffix;
        }
        else {
            return "";
        }
    }

    void ReplaceDollarWithColorTag(std::unordered_map<std::string, std::string>& dict, const std::string& key, const std::string& value, const std::string& color) {
        if (key.find('$') == std::string::npos || value.find('$') == std::string::npos) {
            return;
        }
        std::string modifiedKey = key;
        std::string modifiedValue = value;
        modifiedKey.erase(std::remove(modifiedKey.begin(), modifiedKey.end(), '$'), modifiedKey.end());
        modifiedValue.erase(std::remove(modifiedValue.begin(), modifiedValue.end(), '$'), modifiedValue.end());
        dict[modifiedKey] = modifiedValue;
        std::string coloredKey = key;
        std::string coloredValue = value;
        std::string color_tag = "<color=";
        color_tag += color;
        color_tag += ">";
        // <color=#FF008D>
        // 处理key中的$符号
        for (size_t pos = 0, count = 0; (pos = coloredKey.find('$', pos)) != std::string::npos; count++) {

            coloredKey.replace(pos, 1, (count % 2 == 0) ? color_tag : "</color>");
            pos += (count % 2 == 0) ? 15 : 8; // 跳过替换后的字符串长度
        }
        // 处理value中的$符号
        for (size_t pos = 0, count = 0; (pos = coloredValue.find('$', pos)) != std::string::npos; count++) {
            coloredValue.replace(pos, 1, (count % 2 == 0) ? color_tag : "</color>");
            pos += (count % 2 == 0) ? 15 : 8; // 跳过替换后的字符串长度
        }
        dict[coloredKey] = coloredValue;
    }

    enum class DumpStrStat {
        DEFAULT = 0,
        SPLITTABLE_ORIG = 1,
        SPLITTED = 2,
        FMT = 3
    };

    enum class SplitTagsTranslationStat {
        NO_TRANS,
        PART_TRANS,
        FULL_TRANS,
        NO_SPLIT,
        NO_SPLIT_AND_EMPTY
    };

    void LoadJsonDataToMap(const std::filesystem::path& filePath, std::unordered_map<std::string, std::string>& dict,
                           const bool insertToTranslated = false, const bool needClearDict = true,
                           const bool needCheckSplitPrefix = false,
                           std::vector<RegexTranslationItem>* regexDict = nullptr) {
        if (!exists(filePath)) return;
        try {
            if (needClearDict) {
                dict.clear();
            }
            std::ifstream file(filePath);
            if (!file.is_open()) {
                Log::ErrorFmt("Load %s failed.\n", filePath.string().c_str());
                return;
            }
            std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            auto fileData = nlohmann::json::parse(fileContent);
            for (auto& i : fileData.items()) {
                std::string key = i.key();
                std::string value = i.value();
                if (needCheckSplitPrefix && key.starts_with(splitTextPrefix) && value.starts_with(splitTextPrefix)) {
                    static const auto splitTextPrefixLength = splitTextPrefix.size();
                    const auto splitValue = value.substr(splitTextPrefixLength);
                    genericSplitText[key.substr(splitTextPrefixLength)] = splitValue;
                    if (insertToTranslated) translatedText.emplace(splitValue);
                }
                else {
                    // 检查是否包含占位符，如果包含则生成正则表达式
                    if (regexDict != nullptr && key.find('{') != std::string::npos) {
                        std::string regexPattern = ConvertToRegexPattern(key);
                        if (!regexPattern.empty()) {
                            try {
                                regexDict->emplace_back(regexPattern, value, key, value);
                                if (!regexDict->back().regex->ok()) {
                                    Log::WarnFmt("Invalid regex pattern: %s (from: %s), error: %s", regexPattern.c_str(), key.c_str(), regexDict->back().regex->error().c_str());
                                    regexDict->pop_back();
                                } else {
//                                    Log::VerboseFmt("Successfully created regex pattern: %s (from: %s)", regexPattern.c_str(), key.c_str());
                                }
                            } catch (const std::exception& e) {
                                Log::WarnFmt("Failed to create regex for pattern: %s, error: %s", key.c_str(), e.what());
                            }
                        }
                    }

                    dict[key] = value;
                    auto filename = filePath.filename().string();

                    // 特殊处理 BeginnerMissionsHint.json 的 [@数字] 分割
                    if (filename.ends_with("BeginnerMissionsHint.json")) {
                        ProcessBeginnerMissionsHint(dict, key, value);
                    }

                    if (filename.ends_with("CardSkills.json")) {
                        ReplaceDollarWithColorTag(dict, key, value, "#FF008D");
                    }
                    if (filename.ends_with("RhythmGameSkills.json")) {
                        ReplaceDollarWithColorTag(dict, key, value, "#FFFFFF");
                        ReplaceDollarWithColorTag(dict, key, value, "#FD5B91");
                    }
                    if (filename.ends_with("CenterSkills.json")) {
                        ReplaceDollarWithColorTag(dict, key, value, "#FFFFFF");
                        ReplaceDollarWithColorTag(dict, key, value, "#FD5B91");
                    }
                    if (insertToTranslated) translatedText.emplace(value);
                }
            }
        }
        catch (std::exception& e) {
            Log::ErrorFmt("Load %s failed: %s\n", filePath.string().c_str(), e.what());
        }
    }

    void DumpMapDataToJson(const std::filesystem::path& dumpBasePath, const std::filesystem::path& fileName,
                           const std::unordered_map<std::string, std::string>& dict) {
        const auto dumpFilePath = dumpBasePath / fileName;
        try {
            if (!is_directory(dumpBasePath)) {
                std::filesystem::create_directories(dumpBasePath);
            }
            if (!std::filesystem::exists(dumpFilePath)) {
                std::ofstream dumpWriteLrcFile(dumpFilePath, std::ofstream::out);
                dumpWriteLrcFile << "{}";
                dumpWriteLrcFile.close();
            }

            std::ifstream dumpLrcFile(dumpFilePath);
            std::string fileContent((std::istreambuf_iterator<char>(dumpLrcFile)), std::istreambuf_iterator<char>());
            dumpLrcFile.close();
            auto fileData = nlohmann::ordered_json::parse(fileContent);
            for (const auto& i : dict) {
                fileData[i.first] = i.second;
            }
            const auto newStr = fileData.dump(4, 32, false);
            std::ofstream dumpWriteLrcFile(dumpFilePath, std::ofstream::out);
            dumpWriteLrcFile << newStr.c_str();
            dumpWriteLrcFile.close();
        }
        catch (std::exception& e) {
            Log::ErrorFmt("DumpMapDataToJson %s failed: %s", dumpFilePath.c_str(), e.what());
        }
    }

    void DumpVectorDataToJson(const std::filesystem::path& dumpBasePath, const std::filesystem::path& fileName,
                           const std::vector<std::string>& vec, const std::string& prefix = "") {
        const auto dumpFilePath = dumpBasePath / fileName;
        try {
            if (!is_directory(dumpBasePath)) {
                std::filesystem::create_directories(dumpBasePath);
            }
            if (!std::filesystem::exists(dumpFilePath)) {
                std::ofstream dumpWriteLrcFile(dumpFilePath, std::ofstream::out);
                dumpWriteLrcFile << "{}";
                dumpWriteLrcFile.close();
            }

            std::ifstream dumpLrcFile(dumpFilePath);
            std::string fileContent((std::istreambuf_iterator<char>(dumpLrcFile)), std::istreambuf_iterator<char>());
            dumpLrcFile.close();
            auto fileData = nlohmann::ordered_json::parse(fileContent);
            for (const auto& i : vec) {
                if (!prefix.empty()) {
                    fileData[prefix + i] = prefix + i;
                }
                else {
                    fileData[i] = i;
                }
            }
            const auto newStr = fileData.dump(4, 32, false);
            std::ofstream dumpWriteLrcFile(dumpFilePath, std::ofstream::out);
            dumpWriteLrcFile << newStr.c_str();
            dumpWriteLrcFile.close();
        }
        catch (std::exception& e) {
            Log::ErrorFmt("DumpVectorDataToJson %s failed: %s", dumpFilePath.c_str(), e.what());
        }
    }

    std::string to_lower(const std::string& str) {
        std::string lower_str = str;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
        return lower_str;
    }

    bool IsPureStringValue(const std::string& str) {
        static std::unordered_set<char> notDeeds = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':',
                                                    '/', ' ', '.', '%', ',', '+', '-', 'x', '\n'};
        for (const auto& i : str) {
            if (!notDeeds.contains(i)) {
                return false;
            }
        }
        return true;
    }

    std::vector<std::string> SplitByTags(const std::string& origText) {
        static const std::regex tagsRe("<.*?>(.*?)</.*?>");
        std::string text = origText;
        std::smatch match;

        std::vector<std::string> ret{};

        std::string lastSuffix;
        while (std::regex_search(text, match, tagsRe)) {
            const auto tagValue = match[1].str();
            if (IsPureStringValue(tagValue)) {
                ret.push_back(match.prefix().str());
                lastSuffix = match.suffix().str();
            }
            text = match.suffix().str();
        }
        if (!lastSuffix.empty()) {
            ret.push_back(lastSuffix);
        }

        return ret;
    }

    void ProcessGenericTextLabels() {
        std::unordered_map<std::string, std::string> appendsText{};

        for (const auto& i : genericText) {
            const auto origContents = SplitByTags(i.first);
            if (origContents.empty()) {
                continue;
            }
            const auto translatedContents = SplitByTags(i.second);
            if (origContents.size() == translatedContents.size()) {
                for (const auto& [orig, trans] : std::ranges::views::zip(origContents, translatedContents)) {
                    appendsText.emplace(orig, trans);
                }
            }
        }
        genericText.insert(appendsText.begin(), appendsText.end());
    }

    bool ReplaceString(std::string* str, const std::string& oldSubstr, const std::string& newSubstr) {
        size_t pos = str->find(oldSubstr);
        if (pos != std::string::npos) {
            str->replace(pos, oldSubstr.length(), newSubstr);
            return true;
        }
        return false;
    }

    // 智能替换所有格式化占位符
    void ReplaceAllPlaceholders(std::string* str, int paramIndex, const std::string& value) {
        // 使用RE2正则表达式一次性替换所有可能的格式
        std::string pattern = "\\{" + std::to_string(paramIndex) + "(?::[fF]\\d*)?\\}";
        RE2 re(pattern);
        if (re.ok()) {
            RE2::GlobalReplace(str, re, value);
        } else {
            // 如果正则表达式失败，回退到逐一替换
            std::string basePattern = "{" + std::to_string(paramIndex) + "}";
            ReplaceString(str, basePattern, value);
            ReplaceString(str, basePattern.substr(0, basePattern.length()-1) + ":f}", value);
            ReplaceString(str, basePattern.substr(0, basePattern.length()-1) + ":F}", value);
            for (int i = 1; i <= 9; ++i) {
                ReplaceString(str, basePattern.substr(0, basePattern.length()-1) + ":f" + std::to_string(i) + "}", value);
                ReplaceString(str, basePattern.substr(0, basePattern.length()-1) + ":F" + std::to_string(i) + "}", value);
            }
        }
    }

    bool GetSplitTagsTranslation(const std::string& origText, std::string* newText, std::vector<std::string>& unTransResultRet) {
        if (!origText.contains('<')) return false;
        const auto splitResult = SplitByTags(origText);
        if (splitResult.empty()) return false;

        *newText = origText;
        bool ret = true;
        for (const auto& i : splitResult) {
            if (const auto iter = genericText.find(i); iter != genericText.end()) {
                ReplaceString(newText, i, iter->second);
            }
            else {
                unTransResultRet.emplace_back(i);
                ret = false;
            }
        }
        return ret;
    }

    void ReplaceNumberComma(std::string* orig) {
        if (!orig->contains("，")) return;
        std::string newStr = *orig;
        ReplaceString(&newStr, "，", ",");
        if (IsPureStringValue(newStr)) {
            *orig = newStr;
        }
    }

    SplitTagsTranslationStat GetSplitTagsTranslationFull(const std::string& origTextIn, std::string* newText, std::vector<std::string>& unTransResultRet) {
        // static const std::u16string splitFlags = u"0123456789+＋-－%％【】.";
        static const std::unordered_set<char16_t> splitFlags = {u'0', u'1', u'2', u'3', u'4', u'5',
                                                                u'6', u'7', u'8', u'9', u'+', u'＋',
                                                                u'-', u'－', u'%', u'％', u'【', u'】',
                                                                u'.', u':', u'：', u'×'};

        const auto origText = Misc::ToUTF16(origTextIn);
        bool isInTag = false;
        std::vector<std::string> waitingReplaceTexts{};

        std::u16string currentWaitingReplaceText;

#ifdef GKMS_WINDOWS
#define checkCurrentWaitingReplaceTextAndClear() \
    if (!currentWaitingReplaceText.empty()) { \
        auto trimmed = trim(Misc::ToUTF8(currentWaitingReplaceText)); \
        waitingReplaceTexts.push_back(trimmed); \
        currentWaitingReplaceText.clear(); }
#else
#define checkCurrentWaitingReplaceTextAndClear() \
    if (!currentWaitingReplaceText.empty()) { \
        waitingReplaceTexts.push_back(Misc::ToUTF8(currentWaitingReplaceText)); \
        currentWaitingReplaceText.clear(); }
#endif

        for (char16_t currChar : origText) {
            if (currChar == u'<') {
                isInTag = true;
            }
            if (currChar == u'>') {
                isInTag = false;
                checkCurrentWaitingReplaceTextAndClear()
                continue;
            }
            if (isInTag) {
                checkCurrentWaitingReplaceTextAndClear()
                continue;
            }

            if (!splitFlags.contains(currChar)) {
                currentWaitingReplaceText.push_back(currChar);
            }
            else {
                checkCurrentWaitingReplaceTextAndClear()
            }
        }
        if (waitingReplaceTexts.empty()) {
            if (currentWaitingReplaceText.empty()) {
                return SplitTagsTranslationStat::NO_SPLIT_AND_EMPTY;
            }
            else {
                if (!(!origText.empty() && splitFlags.contains(origText[0]))) {  // 开头为特殊符号或数字
                    return SplitTagsTranslationStat::NO_SPLIT;
                }
            }
        }
        checkCurrentWaitingReplaceTextAndClear()

        *newText = origTextIn;
        SplitTagsTranslationStat ret;
        bool hasTrans = false;
        bool hasNotTrans = false;
        if (!waitingReplaceTexts.empty()) {
            for (const auto& i : waitingReplaceTexts) {
                std::string searchResult = findInMapIgnoreSpace(i, genericSplitText);
                if (!searchResult.empty()) {
                    ReplaceNumberComma(&searchResult);
                    ReplaceString(newText, i, searchResult);
                    hasTrans = true;
                }
                else {
                    unTransResultRet.emplace_back(trim(i));
                    hasNotTrans = true;
                }
            }
            if (hasTrans && hasNotTrans) {
                ret = SplitTagsTranslationStat::PART_TRANS;
            }
            else if (hasTrans && !hasNotTrans) {
                ret = SplitTagsTranslationStat::FULL_TRANS;
            }
            else {
                ret = SplitTagsTranslationStat::NO_TRANS;
            }
        }
        else {
            ret = SplitTagsTranslationStat::NO_TRANS;
        }
        return ret;
    }

    void LoadData() {
        static auto localizationFile = GetBasePath() / "local-files"/ Config::localeCode / "localization.json";
        static auto genericFile = GetBasePath() / "local-files"/ Config::localeCode / "generic.json";
        static auto genericSplitFile = GetBasePath() / "local-files"/ Config::localeCode / "generic.split.json";
        static auto genericDir = GetBasePath() / "local-files"/ Config::localeCode / "genericTrans";
        static auto masterDir = GetBasePath() / "local-files"/ Config::localeCode / "masterTrans";
//        if (!std::filesystem::is_regular_file(localizationFile)) {
//            Log::ErrorFmt("localizationFile: %s not found.", localizationFile.c_str());
//            return;
//        }
//        LoadJsonDataToMap(localizationFile, i18nData, true);
        Log::InfoFmt("%ld localization items loaded.", i18nData.size());

        LoadJsonDataToMap(genericFile, genericText, true, true, true, &regexText);
        genericSplitText.clear();
        genericFmtText.clear();
        LoadJsonDataToMap(genericSplitFile, genericSplitText, true, true, true);
        if (std::filesystem::exists(genericDir) || std::filesystem::is_directory(genericDir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(genericDir)) {
                if (std::filesystem::is_regular_file(entry.path())) {
                    const auto& currFile = entry.path();
                    if (to_lower(currFile.extension().string()) == ".json") {
                        if (currFile.filename().string().ends_with(".split.json")) {  // split text file
                            LoadJsonDataToMap(currFile, genericSplitText, true, false, true);
                        }
                        if (currFile.filename().string().ends_with(".fmt.json")) {  // fmt text file
                            LoadJsonDataToMap(currFile, genericFmtText, true, false, false);
                        }
                        else {
                            LoadJsonDataToMap(currFile, genericText, true, false, true, &regexText);
                        }
                    }
                }
            }
        }

        if (std::filesystem::exists(masterDir) || std::filesystem::is_directory(masterDir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(masterDir)) {
                if (std::filesystem::is_regular_file(entry.path())) {
                    const auto& currFile = entry.path();
                    if (to_lower(currFile.extension().string()) == ".json") {
                        LoadJsonDataToMap(currFile, masterText, true, false, true, &regexText);
                    }
                }
            }
        }

        ProcessGenericTextLabels();
        Log::InfoFmt("%ld generic text items loaded.", genericText.size());
        Log::InfoFmt("%ld master text items loaded.", masterText.size());
        Log::InfoFmt("%ld regex patterns loaded.", regexText.size());

        static auto dumpBasePath = GetBasePath() / "dump-files";
        static auto dumpFilePath = dumpBasePath / "localization.json";
        LoadJsonDataToMap(dumpFilePath, i18nDumpData);
    }

    bool GetI18n(const std::string& key, std::string* ret) {
        if (const auto iter = i18nData.find(key); iter != i18nData.end()) {
            *ret = iter->second;
            return true;
        }
        return false;
    }

    bool inDump = false;
    void DumpI18nItem(const std::string& key, const std::string& value) {
        if (!Config::dumpText) return;
        if (i18nDumpData.contains(key)) return;
        i18nDumpData[key] = value;
        Log::DebugFmt("DumpI18nItem: %s - %s", key.c_str(), value.c_str());

        static auto dumpBasePath = GetBasePath() / "dump-files";

        if (inDump) return;
        inDump = true;
        std::thread([](){
            std::this_thread::sleep_for(std::chrono::seconds(5));
            DumpMapDataToJson(dumpBasePath, "localization.json", i18nDumpData);
            inDump = false;
        }).detach();
    }

    std::string readFileToString(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::exception();
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            (std::istreambuf_iterator<char>()));
        file.close();
        return content;
    }

    bool GetResourceText(const std::string& name, std::string* ret) {
        static std::filesystem::path basePath = GetBasePath();

        try {
            const auto targetFilePath = basePath / "local-files" / "resource" / name;
            // Log::DebugFmt("GetResourceText: %s", targetFilePath.c_str());
            if (exists(targetFilePath)) {
                auto readStr = readFileToString(targetFilePath.string());
                *ret = readStr;
                return true;
            }
        }
        catch (std::exception& e) {
            Log::ErrorFmt("read file: %s failed.", name.c_str());
        }
        return false;
    }

    std::string GetDumpGenericFileName(DumpStrStat stat = DumpStrStat::DEFAULT) {
        if (stat == DumpStrStat::SPLITTABLE_ORIG) {
            if (genericDumpFileIndex == 0) return "generic_orig.json";
            return Log::StringFormat("generic_orig_%d.json", genericDumpFileIndex);
        }
        else if (stat == DumpStrStat::FMT) {
            if (genericDumpFileIndex == 0) return "generic.fmt.json";
            return Log::StringFormat("generic_%d.fmt.json", genericDumpFileIndex);
        }
        else {
            if (genericDumpFileIndex == 0) return "generic.json";
            return Log::StringFormat("generic_%d.json", genericDumpFileIndex);
        }
    }

    bool inDumpGeneric = false;
    void DumpGenericText(const std::string& origText, DumpStrStat stat = DumpStrStat::DEFAULT) {
        if (translatedText.contains(origText)) return;

        std::array<std::reference_wrapper<std::vector<std::string>>, 4> targets = {
                genericTextDumpData,
                genericOrigTextDumpData,
                genericSplittedDumpData,
                genericFmtTextDumpData
        };

        auto& appendTarget = targets[static_cast<int>(stat)].get();

        if (std::find(appendTarget.begin(), appendTarget.end(), origText) != appendTarget.end()) {
            return;
        }
        if (IsPureStringValue(origText)) return;

        appendTarget.push_back(origText);
        static auto dumpBasePath = GetBasePath() / "dump-files";

        if (inDumpGeneric) return;
        inDumpGeneric = true;
        std::thread([](){
            std::this_thread::sleep_for(std::chrono::seconds(5));
            DumpVectorDataToJson(dumpBasePath, GetDumpGenericFileName(DumpStrStat::DEFAULT), genericTextDumpData);
            DumpVectorDataToJson(dumpBasePath, GetDumpGenericFileName(DumpStrStat::SPLITTABLE_ORIG), genericOrigTextDumpData);
            DumpVectorDataToJson(dumpBasePath, GetDumpGenericFileName(DumpStrStat::SPLITTED), genericSplittedDumpData, splitTextPrefix);
            DumpVectorDataToJson(dumpBasePath, GetDumpGenericFileName(DumpStrStat::FMT), genericFmtTextDumpData);
            genericTextDumpData.clear();
            genericSplittedDumpData.clear();
            genericOrigTextDumpData.clear();
            genericFmtTextDumpData.clear();
            inDumpGeneric = false;
        }).detach();
    }

    bool GetGenericText(const std::string& origText, std::string* newStr) {
        // 完全匹配
        if (const auto iter = genericText.find(origText); iter != genericText.end()) {
            *newStr = iter->second;
            return true;
        }
        // TODO tmp masterText
        if (const auto iter = masterText.find(origText); iter != masterText.end()) {
            *newStr = iter->second;
            return true;
        }
        // 不翻译翻译过的文本
        if (translatedText.contains(origText)) {
            return false;
        }

        // 匹配升级卡名
        if (auto plusPos = origText.find_last_not_of('+'); plusPos != std::string::npos) {
            const auto noPlusText = origText.substr(0, plusPos + 1);

            if (const auto iter = genericText.find(noPlusText); iter != genericText.end()) {
                size_t plusCount = origText.length() - (plusPos + 1);
                *newStr = iter->second + std::string(plusCount, '+');
                return true;
            }
        }

        // fmt 文本
        auto fmtText = StringParser::ParseItems::parse(origText, false);
        if (fmtText.isValid) {
            const auto fmtStr = fmtText.ToFmtString();
            if (auto it = genericFmtText.find(fmtStr); it != genericFmtText.end()) {
                auto newRet = fmtText.MergeText(it->second);
                if (!newRet.empty()) {
                    *newStr = newRet;
                    return true;
                }
            }
            if (Config::dumpText) {
                DumpGenericText(fmtStr, DumpStrStat::FMT);
            }
        }
//        Log::VerboseFmt("Try to get generic text from regex: %s", origText.c_str());
        for (const auto& regexItem : regexText) {
            if (regexItem.regex && regexItem.regex->ok()) {
                // Log::VerboseFmt("Debug log in match regex");

                int numGroups = regexItem.regex->NumberOfCapturingGroups();
                // Log::VerboseFmt("Debug log when go in to matching, pattern: %s, capturing groups: %d", regexItem.regex->pattern().c_str(), numGroups);

                bool matchResult = false;
                try {
                    if (numGroups == 0) {
                        matchResult = re2::RE2::FullMatch(origText, *regexItem.regex);
//                        Log::VerboseFmt("FullMatch completed (no capture groups), result: %s", matchResult ? "true" : "false");
                    } else if (numGroups == 1) {
                        std::string match1;
                        matchResult = re2::RE2::FullMatch(origText, *regexItem.regex, &match1);
                        if (matchResult) {
                            Log::VerboseFmt("FullMatch completed (1 group), result: true, match1: %s", match1.c_str());
                            *newStr = regexItem.translation;
                            ReplaceAllPlaceholders(newStr, 0, match1);
                            return true;
                        }
                    } else if (numGroups == 2) {
                        std::string match1, match2;
                        matchResult = re2::RE2::FullMatch(origText, *regexItem.regex, &match1, &match2);
                        if (matchResult) {
                            Log::VerboseFmt("FullMatch completed (2 groups), result: true, match1: %s, match2: %s", match1.c_str(), match2.c_str());
                            *newStr = regexItem.translation;
                            ReplaceAllPlaceholders(newStr, 0, match1);
                            ReplaceAllPlaceholders(newStr, 1, match2);
                            return true;
                        }
                    } else if (numGroups == 3) {
                        std::string match1, match2, match3;
                        matchResult = re2::RE2::FullMatch(origText, *regexItem.regex, &match1, &match2, &match3);
                        if (matchResult) {
                            Log::VerboseFmt("FullMatch completed (3 groups), result: true, match1: %s, match2: %s, match3: %s", match1.c_str(), match2.c_str(), match3.c_str());
                            *newStr = regexItem.translation;
                            ReplaceAllPlaceholders(newStr, 0, match1);
                            ReplaceAllPlaceholders(newStr, 1, match2);
                            ReplaceAllPlaceholders(newStr, 2, match3);
                            return true;
                        }
                    } else if (numGroups == 4) {
                        std::string match1, match2, match3, match4;
                        matchResult = re2::RE2::FullMatch(origText, *regexItem.regex, &match1, &match2, &match3, &match4);
                        if (matchResult) {
                            Log::VerboseFmt("FullMatch completed (4 groups), result: true");
                            *newStr = regexItem.translation;
                            ReplaceAllPlaceholders(newStr, 0, match1);
                            ReplaceAllPlaceholders(newStr, 1, match2);
                            ReplaceAllPlaceholders(newStr, 2, match3);
                            ReplaceAllPlaceholders(newStr, 3, match4);
                            return true;
                        }
                    } else {
//                        Log::WarnFmt("Too many capture groups (%d), skipping regex match", numGroups);
                        continue;
                    }

//                    Log::VerboseFmt("FullMatch completed (%d capture groups), result: %s", numGroups, matchResult ? "true" : "false");
                } catch (const std::exception& e) {
                    Log::ErrorFmt("Exception in FullMatch: %s", e.what());
                    continue;
                } catch (...) {
                    Log::ErrorFmt("Unknown exception in FullMatch");
                    continue;
                }

                // 如果没有匹配成功但是到了这里，说明是无捕获组的情况
                if (matchResult) {
                    Log::WarnFmt("Hit generic regex: template is %s, regex is %s, text is %s", regexItem.originalKey.c_str(), regexItem.originalPattern.c_str(), origText.c_str());
                    *newStr = regexItem.translation;
                    return true;
                }
            }
        }
        auto ret = false;

        // 分割匹配
        std::vector<std::string> unTransResultRet;
        const auto splitTransStat = GetSplitTagsTranslationFull(origText, newStr, unTransResultRet);
        switch (splitTransStat) {
            case SplitTagsTranslationStat::FULL_TRANS: {
                DumpGenericText(origText, DumpStrStat::SPLITTABLE_ORIG);
                return true;
            } break;

            case SplitTagsTranslationStat::NO_SPLIT_AND_EMPTY: {
                return false;
            } break;

            case SplitTagsTranslationStat::NO_SPLIT: {
                ret = false;
            } break;

            case SplitTagsTranslationStat::NO_TRANS: {
                ret = false;
            } break;

            case SplitTagsTranslationStat::PART_TRANS: {
                ret = true;
            } break;
        }

        if (!Config::dumpText) {
            return ret;
        }

        if (unTransResultRet.empty() || (splitTransStat == SplitTagsTranslationStat::NO_SPLIT)) {
            DumpGenericText(origText);
        }
        else {
            for (const auto& i : unTransResultRet) {
                DumpGenericText(i, DumpStrStat::SPLITTED);
            }
            // 若未翻译部分长度为1，且未翻译文本等于原文本，则不 dump 到原文本文件
            //if (unTransResultRet.size() != 1 || unTransResultRet[0] != origText) {
                DumpGenericText(origText, DumpStrStat::SPLITTABLE_ORIG);
            //}
        }

        return ret;
    }

    std::string ChangeDumpTextIndex(int changeValue) {
        if (!Config::dumpText) return "";
        genericDumpFileIndex += changeValue;
        return Log::StringFormat("GenericDumpFile: %s", GetDumpGenericFileName().c_str());
    }

    std::string OnKeyDown(int message, int key) {
        if (message == WM_KEYDOWN) {
            switch (key) {
                case KEY_ADD: {
                    return ChangeDumpTextIndex(1);
                } break;
                case KEY_SUB: {
                    return ChangeDumpTextIndex(-1);
                } break;
            }
        }
        return "";
    }

}
