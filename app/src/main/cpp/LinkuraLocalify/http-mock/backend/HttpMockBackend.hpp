#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace LinkuraLocal::HttpMock {
    struct MockStoredResponse {
        std::string body;
        std::string headersText;
        int statusCode = 200;
        std::string statusDescription = "OK (offline mock)";
    };

    class HttpMockBackend {
    public:
        static HttpMockBackend& Get();

        ~HttpMockBackend();

        bool Reset();
        bool Rebuild();
        static bool ResetAtFilesDir(const std::string& filesDir);
        static bool RebuildAtFilesDir(const std::string& filesDir);
        std::optional<MockStoredResponse> GetArchiveDetailById(std::string_view archivesId);
        std::optional<MockStoredResponse> LookupArchiveDetailFromPayload(std::string_view payloadJson,
                                                                         std::string_view fallbackArchiveId = {});

        std::optional<MockStoredResponse> GetCardDetailByDCardId(std::string_view dCardDatasId);
        std::optional<MockStoredResponse> LookupCardDetailFromPayload(std::string_view payloadJson);

        std::optional<MockStoredResponse> GetItemDetailByDItemId(std::string_view dItemDatasId);
        std::optional<MockStoredResponse> LookupItemDetailFromPayload(std::string_view payloadJson);

        std::optional<MockStoredResponse> GetDeckListResponse();
        std::optional<MockStoredResponse> ModifyDeckList(std::string_view payloadJson);

        std::optional<MockStoredResponse> GetCharacterInfoById(std::string_view characterId);
        std::optional<MockStoredResponse> LookupCharacterInfoFromPayload(std::string_view payloadJson);

        bool IsPersistentStorageAvailable() const;
        std::string GetStatusSummary() const;

        static std::string ExtractPayloadStringField(std::string_view payloadJson, std::string_view fieldName);
        static std::string ExtractPayloadIntegerFieldAsString(std::string_view payloadJson, std::string_view fieldName);

    private:
        HttpMockBackend();

        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
