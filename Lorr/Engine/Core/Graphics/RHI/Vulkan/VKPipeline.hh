//
// Created on Wednesday 21st September 2022 by e-erdal
//

#pragma once

#include "Core/Graphics/RHI/APIConfig.hh"
#include "Core/Graphics/RHI/Base/BasePipeline.hh"

#include "VKSym.hh"

namespace lr::Graphics
{
    struct VKPipeline : BasePipeline
    {
        VkPipeline pHandle = nullptr;
        VkPipelineLayout pLayout = nullptr;
    };

    struct VKGraphicsPipelineBuildInfo : GraphicsPipelineBuildInfo
    {
        void Init();

        /// States - in order by member index
        // Shader Stages
        void SetShader(BaseShader *pShader, eastl::string_view entryPoint);

        // Vertex Input
        // `bindingID` always will be 0, since we only support one binding for now, see notes
        void SetInputLayout(InputLayout &inputLayout);

        // Input Assembly
        void SetPrimitiveType(PrimitiveType type);

        // Rasterizer
        void SetDepthClamp(bool enabled);
        void SetCullMode(CullMode mode, bool frontFaceClockwise);
        void SetFillMode(FillMode mode);
        void SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor);

        // Multisample
        void SetSampleCount(u32 bits);
        void SetAlphaToCoverage(bool alphaToCoverage);

        // Depth Stencil
        void SetDepthState(bool depthTestEnabled, bool depthWriteEnabled);
        void SetStencilState(bool stencilTestEnabled);
        void SetDepthFunction(CompareOp function);
        void SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back);

        void AddAttachment(PipelineAttachment *pAttachment, bool depth);

        VkGraphicsPipelineCreateInfo m_CreateInfo = {};

        // Vars we will reference to create info
        VkPipelineVertexInputStateCreateInfo m_VertexInputState = {};
        VkVertexInputBindingDescription m_VertexBindingDesc = {};
        VkVertexInputAttributeDescription m_pVertexAttribs[LR_MAX_VERTEX_ATTRIBS_PER_PIPELINE] = {};

        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyState = {};
        VkPipelineTessellationStateCreateInfo m_TessellationState = {};

        VkPipelineViewportStateCreateInfo m_ViewportState = {};

        VkPipelineRasterizationStateCreateInfo m_RasterizationState = {};
        VkPipelineMultisampleStateCreateInfo m_MultisampleState = {};
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilState = {};

        VkPipelineColorBlendStateCreateInfo m_ColorBlendState = {};
        VkPipelineColorBlendAttachmentState m_pColorBlendAttachments[LR_MAX_RENDER_TARGET_PER_PASS] = {};

        VkPipelineDynamicStateCreateInfo m_DynamicState = {};

        VkPipelineRenderingCreateInfo m_RenderingInfo = {};
        VkFormat m_ColorAttachmnetFormats[LR_MAX_RENDER_TARGET_PER_PASS] = {};

        static constexpr u32 kMaxShaderStageCount = 8;
        VkPipelineShaderStageCreateInfo m_pShaderStages[8];
    };

    struct VKComputePipelineBuildInfo : ComputePipelineBuildInfo
    {
        void Init() override;

        // Shader Stages
        void SetShader(BaseShader *pShader, eastl::string_view entryPoint) override;

        VkComputePipelineCreateInfo m_CreateInfo = {};
    };

}  // namespace lr::Graphics