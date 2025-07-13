#ifndef LINKURA_LOCALIFY_LOCAL_H
#define LINKURA_LOCALIFY_LOCAL_H

#include <string>
#include <filesystem>
#include <unordered_set>

namespace LinkuraLocal::Local {
    extern std::unordered_set<std::string> translatedText;

    std::filesystem::path GetBasePath();
    void LoadData();
    bool GetI18n(const std::string& key, std::string* ret);
    void DumpI18nItem(const std::string& key, const std::string& value);

    bool GetResourceText(const std::string& name, std::string* ret);
    bool GetGenericText(const std::string& origText, std::string* newStr);

    std::string OnKeyDown(int message, int key);
}

#endif //LINKURA_LOCALIFY_LOCAL_H
