//
// Created on Thursday 10th November 2022 by exdal
//

#pragma once

namespace lr::Resource
{
    enum ResourceType : u32
    {
        LR_RESOURCE_TYPE_UNKNOWN = 0,
        LR_RESOURCE_TYPE_SHADER,
        LR_RESOURCE_TYPE_TEXTURE,
        LR_RESOURCE_TYPE_MODEL,
    };

    struct ResourceMeta
    {
    };

}  // namespace lr::Resource