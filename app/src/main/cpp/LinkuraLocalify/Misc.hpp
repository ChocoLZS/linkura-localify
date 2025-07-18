#pragma once

#include <string>
#include <string_view>
#include <deque>
#include <numeric>
#include <vector>

#include "../platformDefine.hpp"

#ifndef GKMS_WINDOWS
    #include <jni.h>
#endif


namespace LinkuraLocal {
    using OpaqueFunctionPointer = void (*)();

    namespace Misc {
        std::u16string ToUTF16(const std::string_view& str);
        std::string ToUTF8(const std::u16string_view& str);
#ifdef GKMS_WINDOWS
        std::string ToUTF8(const std::wstring_view& str);
#endif

#ifndef GKMS_WINDOWS
        JNIEnv* GetJNIEnv();
#endif

        class CSEnum {
        public:
            CSEnum(const std::string& name, const int value);

            CSEnum(const std::vector<std::string>& names, const std::vector<int>& values);

            int GetIndex();

            void SetIndex(int index);

            int GetTotalLength();

            void Add(const std::string& name, const int value);

            std::pair<std::string, int> GetCurrent();

            std::pair<std::string, int> Last();

            std::pair<std::string, int> Next();

            int GetValueByName(const std::string& name);

        private:
            int currIndex = 0;
            std::vector<std::string> names{};
            std::vector<int> values{};

        };

        template <typename T>
        class FixedSizeQueue {
            static_assert(std::is_arithmetic<T>::value, "T must be an arithmetic type");

        public:
            FixedSizeQueue(size_t maxSize) : maxSize(maxSize), sum(0) {}

            void Push(T value) {
                if (deque.size() >= maxSize) {
                    sum -= deque.front();
                    deque.pop_front();
                }
                deque.push_back(value);
                sum += value;
            }

            float Average() {
                if (deque.empty()) {
                    return 0.0;
                }
                return static_cast<float>(sum) / deque.size();
            }

        private:
            std::deque<T> deque;
            size_t maxSize;
            T sum;
        };

        namespace StringFormat {
            std::string stringFormatString(const std::string& fmt, const std::vector<std::string>& vec);
            std::vector<std::string> split(const std::string& str, char delimiter);
            std::pair<std::string, std::string> split_once(const std::string& str, const std::string& delimiter);
        }
    }
}
