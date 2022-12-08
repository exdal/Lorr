//
// Created on Wednesday 21st September 2022 by e-erdal
//

#pragma once

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

        static constexpr u32 kMaxShaderStageCount = 8;

        // Good thing that this create info takes pointers,
        // so we can easily enable/disable them by referencing them (nullptr means disabled)
        VkGraphicsPipelineCreateInfo m_CreateInfo = {};

        // Vars we will reference to create info
        VkPipelineVertexInputStateCreateInfo m_VertexInputState = {};
        VkVertexInputBindingDescription m_VertexBindingDesc = {};
        VkVertexInputAttributeDescription m_pVertexAttribs[8] = {};

        VkPipelineInputAssemblyStateCreateInfo m_InputAssemblyState = {};
        VkPipelineTessellationStateCreateInfo m_TessellationState = {};

        VkPipelineViewportStateCreateInfo m_ViewportState = {};

        VkPipelineRasterizationStateCreateInfo m_RasterizationState = {};
        VkPipelineMultisampleStateCreateInfo m_MultisampleState = {};
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilState = {};

        VkPipelineColorBlendStateCreateInfo m_ColorBlendState = {};
        VkPipelineColorBlendAttachmentState m_pColorBlendAttachments[8] = {};

        VkPipelineDynamicStateCreateInfo m_DynamicState = {};
        
        VkPipelineRenderingCreateInfo m_RenderingInfo = {};
        VkFormat m_ColorAttachmnetFormats[8] = {};

        VkPipelineShaderStageCreateInfo m_pShaderStages[kMaxShaderStageCount];
    };

}  // namespace lr::Graphics