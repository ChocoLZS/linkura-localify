#pragma once

#include <string>

namespace LinkuraLocal::HttpMock {
    // Create a completed Task<object> that wraps a RestSharp.RestResponse instance.
    // Returns nullptr on failure (caller may decide to fall back or abort).
    void* CreateMockTaskForApiPath(const std::string& apiPath, const std::string& requestBodyJson);
}
