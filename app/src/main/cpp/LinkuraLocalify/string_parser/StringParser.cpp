#include <unordered_set>
#include "StringParser.hpp"
#include "fmt/core.h"
#include "fmt/ranges.h"
#include "../Misc.hpp"

namespace StringParser {

    std::string ParseItems::ToFmtString() {
        std::vector<std::string> ret{};
        int currFlagIndex = 0;
        for (const auto& i : items) {
            if (i.type == ParseItemType::FLAG) {
                ret.push_back(fmt::format("{{{}}}", currFlagIndex));
                currFlagIndex++;
            }
            else {
                ret.push_back(i.content);
            }
        }
        return fmt::format("{}", fmt::join(ret, ""));
    }

    std::vector<std::string> ParseItems::GetFlagValues() {
        std::vector<std::string> ret{};
        for (const auto& i : items) {
            if (i.type == ParseItemType::FLAG) {
                ret.push_back(i.content);
            }
        }
        return ret;
    }

    ParseItems ParseItems::parse(const std::string &str, bool parseTags) {
        static const std::unordered_set<char16_t> splitFlags = {u'0', u'1', u'2', u'3', u'4', u'5',
                                                                u'6', u'7', u'8', u'9', u'+', u'＋',
                                                                u'-', u'－', u'%', u'％',
                                                                u'.', u'×', u',', u'，'};

        ParseItems result;
        if (str.contains("{")) {
            result.isValid = false;
            return result;
        }
        std::u16string origText = LinkuraLocal::Misc::ToUTF16(str);
        bool isInTag = false;
        bool isInFlagSequence = false;
        std::u16string currentCacheText;

        for (char16_t currChar : origText) {
            if (parseTags && currChar == u'<') {
                if (!currentCacheText.empty()) {
                    result.items.push_back({isInFlagSequence ? ParseItemType::FLAG : ParseItemType::TEXT,
                                            LinkuraLocal::Misc::ToUTF8(currentCacheText)});
                    currentCacheText.clear();
                    isInFlagSequence = false;
                }
                isInTag = true;
                currentCacheText.push_back(currChar);
            } else if (parseTags && currChar == u'>') {
                isInTag = false;
                currentCacheText.push_back(currChar);
                result.items.push_back({ParseItemType::FLAG, LinkuraLocal::Misc::ToUTF8(currentCacheText)});
                currentCacheText.clear();
            } else if (isInTag) {
                currentCacheText.push_back(currChar);
            } else if (splitFlags.contains(currChar)) {
                if (!isInFlagSequence && !currentCacheText.empty()) {
                    result.items.push_back({ParseItemType::TEXT, LinkuraLocal::Misc::ToUTF8(currentCacheText)});
                    currentCacheText.clear();
                }
                isInFlagSequence = true;
                currentCacheText.push_back(currChar);
            } else {
                if (isInFlagSequence && !currentCacheText.empty()) {
                    result.items.push_back({ParseItemType::FLAG, LinkuraLocal::Misc::ToUTF8(currentCacheText)});
                    currentCacheText.clear();
                    isInFlagSequence = false;
                }
                currentCacheText.push_back(currChar);
            }
        }

        if (!currentCacheText.empty()) {
            result.items.push_back({isInFlagSequence ? ParseItemType::FLAG : ParseItemType::TEXT,
                                    LinkuraLocal::Misc::ToUTF8(currentCacheText)});
        }

        for (auto& i : result.items) {
            if (i.type == ParseItemType::FLAG) {
                return result;
            }
        }
        result.isValid = false;
        return result;
    }

    std::string ParseItems::MergeText(ParseItems &textTarget, ParseItems &valueTarget) {
        if (!textTarget.isValid) return "";
        if (!valueTarget.isValid) return "";
        const auto fmtText = textTarget.ToFmtString();
        const auto values = valueTarget.GetFlagValues();
        const std::string ret = LinkuraLocal::Misc::StringFormat::stringFormatString(fmtText, values);
        return {ret.begin(), ret.end()};
    }

    std::string ParseItems::MergeText(const std::string &newStr) {
        if (!isValid) return "";
        const auto values = GetFlagValues();
        return LinkuraLocal::Misc::StringFormat::stringFormatString(newStr, values);
    }

    int ParseItems::GetFlagCount() {
        int ret = 0;
        for (auto& i : items) {
            if (i.type == ParseItemType::FLAG) {
                ret++;
            }
        }
        return ret;
    }

}
