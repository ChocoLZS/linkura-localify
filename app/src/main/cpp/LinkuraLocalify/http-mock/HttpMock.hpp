#pragma once

#include <string>

namespace LinkuraLocal::HttpMock {
    // Returns nullptr → noop or no route/file matched; caller should return nullptr without making a request.
    // Returns task    → completed Task<object> wrapping a mock RestResponse.
    void* CreateMockTaskForApiPath(const std::string& apiPath, const std::string& requestBodyJson);
}
