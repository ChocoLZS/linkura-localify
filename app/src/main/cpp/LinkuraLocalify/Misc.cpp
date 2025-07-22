#include "Misc.hpp"

#include <codecvt>
#include <locale>
#include "fmt/core.h"

#include <stdexcept>
#include <cstdio>

#ifndef GKMS_WINDOWS
    #include <jni.h>
    
    extern JavaVM* g_javaVM;
#else
    #include "cpprest/details/http_helpers.h"
#endif


namespace LinkuraLocal::Misc {

#ifdef GKMS_WINDOWS
    std::string ToUTF8(const std::wstring_view& str) {
		return utility::conversions::to_utf8string(str.data());
    }

    std::u16string ToUTF16(const std::string_view& str) {
        std::string input(str);
        std::wstring wstr = utility::conversions::utf8_to_utf16(input);
        return std::u16string(wstr.begin(), wstr.end());
    }

    std::string ToUTF8(const std::u16string_view& str) {
        std::u16string u16(str);
        std::wstring wstr(u16.begin(), u16.end());
        return utility::conversions::utf16_to_utf8(wstr);
    }
#else
    std::u16string ToUTF16(const std::string_view& str) {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utf16conv;
        return utf16conv.from_bytes(str.data(), str.data() + str.size());
    }

    std::string ToUTF8(const std::u16string_view& str) {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> utf16conv;
        return utf16conv.to_bytes(str.data(), str.data() + str.size());
    }
#endif

#ifndef GKMS_WINDOWS
    JNIEnv* GetJNIEnv() {
        if (!g_javaVM) return nullptr;
        JNIEnv* env = nullptr;
        if (g_javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            int status = g_javaVM->AttachCurrentThread(&env, nullptr);
            if (status < 0) {
                return nullptr;
            }
        }
        return env;
    }
#endif

    CSEnum::CSEnum(const std::string& name, const int value) {
        this->Add(name, value);
    }

    CSEnum::CSEnum(const std::vector<std::string>& names, const std::vector<int>& values) {
        if (names.size() != values.size()) return;
        this->names = names;
        this->values = values;
    }

    int CSEnum::GetIndex() {
        return currIndex;
    }

    void CSEnum::SetIndex(int index) {
        if (index < 0) return;
        if (index + 1 >= values.size()) return;
        currIndex = index;
    }

    int CSEnum::GetTotalLength() {
        return values.size();
    }

    void CSEnum::Add(const std::string &name, const int value) {
        this->names.push_back(name);
        this->values.push_back(value);
    }

    std::pair<std::string, int> CSEnum::GetCurrent() {
        return std::make_pair(names[currIndex], values[currIndex]);
    }

    std::pair<std::string, int> CSEnum::Last() {
        const auto maxIndex = this->GetTotalLength() - 1;
        if (currIndex <= 0) {
            currIndex = maxIndex;
        }
        else {
            currIndex--;
        }
        return this->GetCurrent();
    }

    std::pair<std::string, int> CSEnum::Next() {
        const auto maxIndex = this->GetTotalLength() - 1;
        if (currIndex >= maxIndex) {
            currIndex = 0;
        }
        else {
            currIndex++;
        }
        return this->GetCurrent();
    }

    int CSEnum::GetValueByName(const std::string &name) {
        for (int i = 0; i < names.size(); i++) {
            if (names[i] == name) {
                return values[i];
            }
        }
        return values[0];
    }


    namespace StringFormat {
        template<typename... Args>
        std::string string_format(const std::string& fmt, Args&&... args) {
            // return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
            return fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
        }


        template <std::size_t N, std::size_t... Indices, typename T>
        auto vectorToTupleImpl(const std::vector<T>& vec, std::index_sequence<Indices...>) {
            if (vec.size() != N) {
                // printf("vec.size: %zu, N: %zu\n", vec.size(), N);
                throw std::out_of_range("Vector size does not match tuple size.");
            }
            return std::make_tuple(vec[Indices]...);
        }

        template <std::size_t N, typename T>
        auto vectorToTuple(const std::vector<T>& vec) {
            return vectorToTupleImpl<N>(vec, std::make_index_sequence<N>{});
        }


        template <typename T>
        std::string stringFormat(const std::string& fmt, const std::vector<T>& vec) {
            std::string ret = fmt;

#define CASE_ARG_COUNT(N) \
    case N: {\
        auto tp = vectorToTuple<N>(vec); \
        std::apply([&](auto&&... args) { \
            ret = string_format(fmt, args...); \
        }, tp); } break;

            switch (vec.size()) {
                CASE_ARG_COUNT(1)
                CASE_ARG_COUNT(2)
                CASE_ARG_COUNT(3)
                CASE_ARG_COUNT(4)
                CASE_ARG_COUNT(5)
                CASE_ARG_COUNT(6)
                CASE_ARG_COUNT(7)
                CASE_ARG_COUNT(8)
                CASE_ARG_COUNT(9)
                CASE_ARG_COUNT(10)
                CASE_ARG_COUNT(11)
                CASE_ARG_COUNT(12)
                CASE_ARG_COUNT(13)
                CASE_ARG_COUNT(14)
                CASE_ARG_COUNT(15)
                CASE_ARG_COUNT(16)
                CASE_ARG_COUNT(17)
                CASE_ARG_COUNT(18)
                CASE_ARG_COUNT(19)
                CASE_ARG_COUNT(20)
                CASE_ARG_COUNT(21)
                CASE_ARG_COUNT(22)
                CASE_ARG_COUNT(23)
                CASE_ARG_COUNT(24)
            }
            return ret;
        }

        std::string stringFormatString(const std::string& fmt, const std::vector<std::string>& vec) {
            try {
                return stringFormat(fmt, vec);
            }
            catch (std::exception& e) {
                return fmt;
            }
        }

        std::vector<std::string> split(const std::string& str, char delimiter) {
            std::vector<std::string> result;
            std::string current;
            for (char c : str) {
                if (c == delimiter) {
                    if (!current.empty()) {
                        result.push_back(current);
                    }
                    current.clear();
                } else {
                    current += c;
                }
            }
            if (!current.empty()) {
                result.push_back(current);
            }
            return result;
        }

        std::pair<std::string, std::string> split_once(const std::string& str, const std::string& delimiter) {
            size_t pos = str.find(delimiter);
            if (pos != std::string::npos) {
                return {str.substr(0, pos), str.substr(pos + delimiter.size())};
            }
            return {str, ""};
        }
    }

    namespace Time {
        long long parseISOTime(const std::string& isoTime) {
            std::string timeStr = isoTime;
            if (!timeStr.empty() && timeStr.back() == 'Z') {
                timeStr.pop_back();
            }

            int year, month, day, hour, minute, second, milliseconds = 0;

            size_t dotPos = timeStr.find('.');
            if (dotPos != std::string::npos) {
                std::string msStr = timeStr.substr(dotPos + 1);
                timeStr = timeStr.substr(0, dotPos);

                if (msStr.length() > 3) msStr = msStr.substr(0, 3);
                else if (msStr.length() < 3) msStr += std::string(3 - msStr.length(), '0');
                milliseconds = std::stoi(msStr);
            }

            if (sscanf(timeStr.c_str(), "%d-%d-%dT%d:%d:%d",
                       &year, &month, &day, &hour, &minute, &second) != 6) {
                throw std::runtime_error("Invalid time format");
            }

            auto isLeap = [](int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); };
            auto daysInMonth = [&](int m, int y) {
                int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                return (m == 2 && isLeap(y)) ? 29 : days[m-1];
            };

            long days = 0;
            for (int y = 1970; y < year; ++y) days += isLeap(y) ? 366 : 365;
            for (int m = 1; m < month; ++m) days += daysInMonth(m, year);
            days += day - 1;

            return (days * 24LL * 3600 + hour * 3600LL + minute * 60 + second) * 1000 + milliseconds;
        }
    }
}
