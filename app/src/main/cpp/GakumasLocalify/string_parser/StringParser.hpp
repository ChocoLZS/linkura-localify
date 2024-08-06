#pragma once

#include <string>
#include <vector>

namespace StringParser {
    enum class ParseItemType {
        FLAG,
        TEXT
    };

    struct ParseItem {
        ParseItemType type;
        std::string content;
    };

    struct ParseItems {
        bool isValid = true;
        std::vector<ParseItem> items;

        std::string ToFmtString();
        std::vector<std::string> GetFlagValues();
        std::string MergeText(const std::string& newStr);
        int GetFlagCount();

        static ParseItems parse(const std::string& str, bool parseTags);
        static std::string MergeText(ParseItems& textTarget, ParseItems& valueTarget);
    };

}
