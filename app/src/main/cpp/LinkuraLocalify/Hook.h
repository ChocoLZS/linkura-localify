#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace LinkuraLocal::Hook
{
    void Install();
    std::vector<uint8_t> getCameraInfoProtobuf();
}


