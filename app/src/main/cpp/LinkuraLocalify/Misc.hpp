#pragma once

#include <string>
#include <string_view>
#include <deque>
#include <numeric>
#include <vector>
#include <set>
#include <unordered_set>

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

        template <typename T>
        class IndexedSet {
        private:
            std::vector<T> data;                    // 存储所有值，支持下标访问
            std::unordered_set<T> lookup;           // 快速查找是否存在
            size_t currentIndex = 0;                // 当前索引

        public:
            virtual ~IndexedSet() = default;

            void initialize(const std::vector<T>& items) {
                data = items;
                lookup.clear();
                lookup.insert(items.begin(), items.end());
                currentIndex = 0;
            }

            bool contains(const T& value) const {
                return lookup.find(value) != lookup.end();
            }

            void add(const T& value) {
                if (!contains(value)) {
                    data.push_back(value);
                    lookup.insert(value);
                }
            }

            const T& operator[](size_t index) const {
                return data[index];
            }

            T& operator[](size_t index) {
                return data[index];
            }

            const T& getCurrentValue() const {
                return data[currentIndex];
            }

            virtual void next() {
                currentIndex = (currentIndex + 1) % data.size();
            }

            virtual void prev() {
                currentIndex = (currentIndex == 0) ? data.size() - 1 : currentIndex - 1;
            }

            void setCurrentIndex(size_t index) {
                if (index < data.size()) {
                    currentIndex = index;
                }
            }

            [[nodiscard]] size_t getCurrentIndex() const {
                return currentIndex;
            }

            [[nodiscard]] size_t size() const {
                return data.size();
            }

            virtual void clear() {
                data.clear();
                lookup.clear();
                currentIndex = 0;
            }

            void finalize() {
                this->clear();
            }
        };

        namespace StringFormat {
            std::string stringFormatString(const std::string& fmt, const std::vector<std::string>& vec);
            std::vector<std::string> split(const std::string& str, char delimiter);
            std::pair<std::string, std::string> split_once(const std::string& str, const std::string& delimiter);
        }

        namespace Time {
            long long parseISOTime(const std::string& isoTime);
        }


    }
}
