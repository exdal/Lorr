//
// Created on Wednesday 21st September 2022 by e-erdal
//

#pragma once

#include "Graphics/API/Common.hh"
#include "Graphics/API/InputLayout.hh"

#include "VKSym.hh"

namespace lr::Graphics
{
    struct VKPipeline
    {
        VkPipeline pHandle = nullptr;
        VkPipelineLayout pLayout = nullptr;
    };

    /// Some notes:
    // * Currently only one vertex binding is supported and it only supports vertex, not instances (actually a TODO)
    // * MSAA is not actually supported
    // * Color blending stage is too restricted, only supporting one attachment
    struct VKGraphicsPipelineBuildInfo
    {
        /// States - in order by member index
        // Shader Stages
        void SetShaderStage(VkShaderModule pShader, VkShaderStageFlagBits stage, eastl::string_view entryPoint = "main");
        // Vertex Input
        // `bindingID` always will be 0, since we only support one binding for now, see notes
        void SetInputLayout(u32 bindingID, InputLayout &inputLayout);
        // Input Assembly
        void SetPrimitiveType(PrimitiveType type);
        // Tessellation
        void SetPatchCount(u32 count);
        // Viewport
        void AddViewport(u32 viewportCount = 1, u32 scissorCount = 0);
        void SetViewport(u32 viewportID, u32 width, u32 height, f32 minDepth, f32 maxDepth);
        void SetScissor(u32 scissorID, u32 x, u32 y, u32 w, u32 h);
        // Rasterizer
        void SetDepthClamp(bool enabled);
        void SetRasterizerDiscard(bool enabled);
        void SetPolygonMode(VkPolygonMode mode);  // TODO: Maybe abstract `VkPolygonMode`
        void SetCullMode(CullMode mode, bool frontFaceClockwise);
        void SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor);
        void SetLineWidth(f32 size);
        // Multisample
        void SetSampleCount(VkSampleCountFlagBits bits);
        void SetSampledShading(bool enabled, f32 minSampleShading);
        void SetAlphaToCoverage(bool alphaToCoverage, bool alphaToOne);
        // Depth Stencil
        void SetDepthState(bool depthTestEnabled, bool depthWriteEnabled, bool depthBoundsTestEnabled);
        void SetStencilState(bool stencilTestEnabled, VkStencilOpState frontState, VkStencilOpState backState);
        void SetDepthBounds(f32 min, f32 max);
        // TODO: Color Blend
        void SetBlendAttachment(u32 attachmentID, bool enabled, u8 mask);
        // void SetBlendState(bool enabled);
        // void SetBlendColorFactor(VkBlendFactor src, VkBlendFactor dst, VkBlendOp op);
        // void SetBlendAlphaFactor(VkBlendFactor src, VkBlendFactor dst, VkBlendOp op);
        // void SetBlendWriteState(bool writeR, bool writeG, bool writeB, bool writeA);
        // Dynamic State
        void SetDynamicState(const std::initializer_list<VkDynamicState> &dynamicState);

        static constexpr u32 kMaxShaderStageCount = 8;

        // Good thing that this create info takes pointers,
        // so we can easily enable/disable them by referencing them (nullptr means disabled)
        VkGraphicsPipelineCreateInfo m_CreateInfo;

        // Vars we will reference to create info
        VkPipelineVertexInputStateCreateInfo m_VertexInputState;
        VkVertexInputBindingDescription m_VertexBindingDesc;
        VkVertexInputAttributeDescription m_pVertexAttribs[8];

        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyState;
        VkPipelineTessellationStateCreateInfo m_TessellationState;

        VkPipelineViewportStateCreateInfo m_ViewportState;
        VkViewport m_pViewports[8];
        VkRect2D m_pScissors[8];

        VkPipelineRasterizationStateCreateInfo m_RasterizationState;
        VkPipelineMultisampleStateCreateInfo m_MultisampleState;
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilState;

        VkPipelineColorBlendStateCreateInfo m_ColorBlendState;
        VkPipelineColorBlendAttachmentState m_pColorBlendAttachments[8];

        VkPipelineDynamicStateCreateInfo m_DynamicState;
        VkDynamicState m_DynamicStates[16];

        VkPipelineShaderStageCreateInfo m_pShaderStages[kMaxShaderStageCount];

        VkRenderPass m_pSetRenderPass = nullptr;
    };

    struct VKAPI;

    // A manager to build pipelines, with native cache support(ty vk)
    struct VKPipelineManager
    {
        void Init(VKAPI *pAPI);

        void InitBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo, VkRenderPass pRenderPass);

        VkPipelineCache m_pPipelineCache;
    };

}  // namespace lr::Graphics