//
// Created on Sunday 23rd October 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "BaseResource.hh"

namespace lr::Graphics
{
    enum class AttachmentType
    {
        Undefined = 0,
        RenderTarget,
        DepthStencil,
    };
    EnumFlags(AttachmentType);

    enum class RenderPassOperation
    {
        DontCare,
        Clear,
        Store,
        Load
    };

    struct RenderPassAttachment
    {
        AttachmentType Flags = AttachmentType::Undefined;
        ResourceFormat Format = ResourceFormat::Unknown;
        u32 SampleCount = 1;  // MSAA -> If > 1_BIT

        RenderPassOperation LoadOp = RenderPassOperation::DontCare;
        RenderPassOperation StoreOp = RenderPassOperation::DontCare;
        RenderPassOperation StencilLoadOp = RenderPassOperation::DontCare;
        RenderPassOperation StencilStoreOp = RenderPassOperation::DontCare;
    };
    
    // TODO: Multiple subpasses
    struct RenderPassDesc
    {
        // `8` is the maximum attachments per `RenderPass` we support.
        u32 ColorAttachmentCount = 0;
        
        RenderPassAttachment *ppColorAttachments[APIConfig::kMaxColorAttachmentCount];
        RenderPassAttachment *pDepthAttachment = nullptr;
    };

    struct BaseRenderTarget
    {
    };

    struct BaseRenderPass
    {
        u32 ColorAttachmentCount = 0;
        bool HasDepthAttachment = false;

        RenderPassAttachment pColorAttachments[APIConfig::kMaxColorAttachmentCount];
        RenderPassAttachment DepthAttachment;

        BaseRenderTarget *pBoundRenderTarget = nullptr;
    };

}  // namespace lr::Graphics