#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace LinkuraLocal::HttpMock {
    class HttpMockBackend;

    struct MockRequestContext {
        std::string_view path;
        std::string_view payloadJson;
    };

    struct MockResponse {
        std::string body;
        std::string headersText;
        int statusCode = 200;
        std::string statusDescription = "OK (offline mock)";
    };

    using RegisteredRouteHandler = std::function<std::optional<MockResponse>(const MockRequestContext&, HttpMockBackend&)>;

    std::optional<MockResponse> ResolveRegisteredRoute(const MockRequestContext& request);
}
