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
        void SetPolygonMode(VkPolygonMode mode);  // TODO: Maybe abstract `VkPolygonMode`
        void SetCullMode(CullMode mode, bool frontFaceClockwise);
        void SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor);

        // Multisample
        void SetSampleCount(u32 bits);
        void SetAlphaToCoverage(bool alphaToCoverage);

        // Depth Stencil
        void SetDepthState(bool depthTestEnabled, bool depthWriteEnabled);
        void SetStencilState(bool stencilTestEnabled);
        void SetDepthFunction(DepthCompareOp function);
        void SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back);

        // TODO: Color Blend
        void SetBlendAttachment(u32 attachmentID, bool enabled, u8 mask);
        // void SetBlendState(bool enabled);
        // void SetBlendColorFactor(VkBlendFactor src, VkBlendFactor dst, VkBlendOp op);
        // void SetBlendAlphaFactor(VkBlendFactor src, VkBlendFactor dst, VkBlendOp op);
        // void SetBlendWriteState(bool writeR, bool writeG, bool writeB, bool writeA);
        // Dynamic State
        // void SetDynamicState(const std::initializer_list<VkDynamicState> &dynamicState);

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

        VkPipelineRasterizationStateCreateInfo m_RasterizationState;
        VkPipelineMultisampleStateCreateInfo m_MultisampleState;
        VkPipelineDepthStencilStateCreateInfo m_DepthStencilState;

        VkPipelineColorBlendStateCreateInfo m_ColorBlendState;
        VkPipelineColorBlendAttachmentState m_pColorBlendAttachments[8];

        VkPipelineDynamicStateCreateInfo m_DynamicState;

        VkPipelineShaderStageCreateInfo m_pShaderStages[kMaxShaderStageCount];
    };

}  // namespace lr::Graphics