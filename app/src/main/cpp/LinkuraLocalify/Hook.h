#ifndef LINKURA_LOCALIFY_HOOK_H
#define LINKURA_LOCALIFY_HOOK_H

#include <string>
#include <vector>
#include <cstdint>

namespace LinkuraLocal::Hook
{
    void Install();
    std::string getCameraInfo();
    std::vector<uint8_t> getCameraInfoProtobuf();
}

#endif //LINKURA_LOCALIFY_HOOK_H
