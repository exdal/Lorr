#pragma once

#include "Common.hh"

namespace lr::graphics {
struct InstanceInfo {
    std::string_view app_name = "Lorr App";
    std::string_view engine_name = "Lorr";
};

struct Instance {
    VKResult init(const InstanceInfo &info);

    vkb::Instance m_handle;
};
}  // namespace lr::graphics
