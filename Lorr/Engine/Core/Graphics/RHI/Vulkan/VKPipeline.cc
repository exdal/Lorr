#include "VKPipeline.hh"

#include "VKAPI.hh"
#include "VKShader.hh"

namespace lr::Graphics
{
    constexpr VkBlendFactor kBlendFactorLUT[] = {
        VK_BLEND_FACTOR_ZERO,                      // Zero
        VK_BLEND_FACTOR_ONE,                       // One
        VK_BLEND_FACTOR_SRC_COLOR,                 // SrcColor
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,       // InvSrcColor
        VK_BLEND_FACTOR_SRC_ALPHA,                 // SrcAlpha
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,       // InvSrcAlpha
        VK_BLEND_FACTOR_DST_ALPHA,                 // DestAlpha
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,       // InvDestAlpha
        VK_BLEND_FACTOR_DST_COLOR,                 // DestColor
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,       // InvDestColor
        VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,        // SrcAlphaSat
        VK_BLEND_FACTOR_CONSTANT_COLOR,            // ConstantColor
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,  // InvConstantColor
        VK_BLEND_FACTOR_SRC1_COLOR,                // Src1Color
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,      // InvSrc1Color
        VK_BLEND_FACTOR_SRC1_ALPHA,                // Src1Alpha
        VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,      // InvSrc1Alpha
    };

    void VKGraphicsPipelineBuildInfo::Init()
    {
        ZoneScoped;

        m_CreateInfo = {};
        m_CreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        m_CreateInfo.pVertexInputState = &m_VertexInputState;
        m_CreateInfo.pInputAssemblyState = &m_InputAssemblyState;
        m_CreateInfo.pTessellationState = &m_TessellationState;
        m_CreateInfo.pViewportState = &m_ViewportState;
        m_CreateInfo.pRasterizationState = &m_RasterizationState;
        m_CreateInfo.pMultisampleState = &m_MultisampleState;
        m_CreateInfo.pDepthStencilState = &m_DepthStencilState;
        m_CreateInfo.pColorBlendState = &m_ColorBlendState;
        m_CreateInfo.pDynamicState = &m_DynamicState;
        m_CreateInfo.pStages = m_pShaderStages;
        m_CreateInfo.pNext = &m_RenderingInfo;

        m_VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_TessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        m_RenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        for (u32 i = 0; i < VKGraphicsPipelineBuildInfo::kMaxShaderStageCount; i++)
            m_pShaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        /// ------------------------------------- ///

        VkVertexInputBindingDescription &vertexBinding = m_VertexBindingDesc;
        vertexBinding.binding = 0;
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        m_VertexInputState.vertexBindingDescriptionCount = 1;
        m_VertexInputState.pVertexBindingDescriptions = &m_VertexBindingDesc;
        m_VertexInputState.pVertexAttributeDescriptions = m_pVertexAttribs;

        /// ------------------------------------- ///

        m_ViewportState.pViewports = nullptr;
        m_ViewportState.pScissors = nullptr;

        /// ------------------------------------- ///

        m_RasterizationState.lineWidth = 1.0;
        SetPrimitiveType(LR_PRIMITIVE_TYPE_TRIANGLE_LIST);

        /// ------------------------------------- ///

        m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        /// ------------------------------------- ///

        m_ColorBlendState.pAttachments = m_pColorBlendAttachments;
        m_ColorBlendState.logicOpEnable = false;

        /// ------------------------------------- ///

        m_RenderingInfo.pColorAttachmentFormats = m_ColorAttachmnetFormats;

        /// ------------------------------------- ///

        constexpr static eastl::array<VkDynamicState, 3> kDynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        };

        m_DynamicState.dynamicStateCount = kDynamicStates.count;
        m_DynamicState.pDynamicStates = kDynamicStates.data();
    }

    void VKGraphicsPipelineBuildInfo::SetShader(BaseShader *pShader, eastl::string_view entryPoint)
    {
        ZoneScoped;

        VkPipelineShaderStageCreateInfo &shaderStage = m_pShaderStages[m_CreateInfo.stageCount++];
        shaderStage.flags = 0;
        shaderStage.pNext = nullptr;

        shaderStage.stage = VKAPI::ToVKShaderType(pShader->Type);
        shaderStage.pName = entryPoint.data();
        shaderStage.module = ((VKShader *)pShader)->pHandle;
        shaderStage.pSpecializationInfo = nullptr;
    }

    void VKGraphicsPipelineBuildInfo::SetInputLayout(InputLayout &inputLayout)
    {
        ZoneScoped;

        m_VertexInputState.vertexAttributeDescriptionCount = inputLayout.m_Count;
        m_VertexBindingDesc.stride = inputLayout.m_Stride;

        for (u32 i = 0; i < inputLayout.m_Count; i++)
        {
            VertexAttrib &element = inputLayout.m_Elements[i];
            VkVertexInputAttributeDescription &attribDesc = m_pVertexAttribs[i];

            attribDesc.binding = 0;
            attribDesc.location = i;
            attribDesc.offset = element.m_Offset;
            attribDesc.format = VKAPI::ToVKFormat(element.m_Type);
        }
    }

    void VKGraphicsPipelineBuildInfo::SetPrimitiveType(PrimitiveType type)
    {
        ZoneScoped;

        m_InputAssemblyState.topology = VKAPI::ToVKTopology(type);
    }

    void VKGraphicsPipelineBuildInfo::SetDepthClamp(bool enabled)
    {
        ZoneScoped;

        m_RasterizationState.depthBiasClamp = enabled;
    }

    void VKGraphicsPipelineBuildInfo::SetCullMode(CullMode mode, bool frontFaceClockwise)
    {
        ZoneScoped;

        m_RasterizationState.cullMode = VKAPI::ToVKCullMode(mode);
        m_RasterizationState.frontFace = frontFaceClockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    void VKGraphicsPipelineBuildInfo::SetFillMode(FillMode mode)
    {
        ZoneScoped;

        m_RasterizationState.polygonMode = (VkPolygonMode)mode;
    }

    void VKGraphicsPipelineBuildInfo::SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor)
    {
        ZoneScoped;

        m_RasterizationState.depthBiasEnable = enabled;
        m_RasterizationState.depthBiasConstantFactor = constantFactor;
        m_RasterizationState.depthBiasClamp = clamp;
        m_RasterizationState.depthBiasSlopeFactor = slopeFactor;
    }

    void VKGraphicsPipelineBuildInfo::SetSampleCount(u32 bits)
    {
        ZoneScoped;

        m_MultisampleState.rasterizationSamples = (VkSampleCountFlagBits)bits;
    }

    void VKGraphicsPipelineBuildInfo::SetAlphaToCoverage(bool alphaToCoverage)
    {
        ZoneScoped;
    }

    void VKGraphicsPipelineBuildInfo::SetDepthState(bool depthTestEnabled, bool depthWriteEnabled)
    {
        ZoneScoped;

        m_DepthStencilState.depthTestEnable = depthTestEnabled;
        m_DepthStencilState.depthWriteEnable = depthWriteEnabled;
    }

    void VKGraphicsPipelineBuildInfo::SetStencilState(bool stencilTestEnabled)
    {
        ZoneScoped;

        m_DepthStencilState.stencilTestEnable = stencilTestEnabled;
    }

    void VKGraphicsPipelineBuildInfo::SetDepthFunction(CompareOp function)
    {
        ZoneScoped;

        m_DepthStencilState.depthCompareOp = (VkCompareOp)function;
    }

    void VKGraphicsPipelineBuildInfo::SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back)
    {
        ZoneScoped;

        m_DepthStencilState.front.compareOp = (VkCompareOp)front.CompareFunc;
        m_DepthStencilState.front.depthFailOp = (VkStencilOp)front.DepthFail;
        m_DepthStencilState.front.failOp = (VkStencilOp)front.Fail;
        m_DepthStencilState.front.passOp = (VkStencilOp)front.Pass;

        m_DepthStencilState.back.compareOp = (VkCompareOp)back.CompareFunc;
        m_DepthStencilState.back.depthFailOp = (VkStencilOp)back.DepthFail;
        m_DepthStencilState.back.failOp = (VkStencilOp)back.Fail;
        m_DepthStencilState.back.passOp = (VkStencilOp)back.Pass;
    }

    void VKGraphicsPipelineBuildInfo::AddAttachment(PipelineAttachment *pAttachment, bool depth)
    {
        ZoneScoped;

        if (depth)
            return;

        u32 id = m_ColorBlendState.attachmentCount++;
        m_RenderingInfo.colorAttachmentCount = m_ColorBlendState.attachmentCount;

        m_ColorAttachmnetFormats[id] = VKAPI::ToVKFormat(pAttachment->Format);

        m_pColorBlendAttachments[id].blendEnable = pAttachment->BlendEnable;
        m_pColorBlendAttachments[id].colorWriteMask = pAttachment->WriteMask;

        m_pColorBlendAttachments[id].srcColorBlendFactor = kBlendFactorLUT[(u32)pAttachment->SrcBlend];
        m_pColorBlendAttachments[id].dstColorBlendFactor = kBlendFactorLUT[(u32)pAttachment->DstBlend];

        m_pColorBlendAttachments[id].srcAlphaBlendFactor = kBlendFactorLUT[(u32)pAttachment->SrcBlendAlpha];
        m_pColorBlendAttachments[id].dstAlphaBlendFactor = kBlendFactorLUT[(u32)pAttachment->DstBlendAlpha];

        m_pColorBlendAttachments[id].colorBlendOp = (VkBlendOp)pAttachment->ColorBlendOp;
        m_pColorBlendAttachments[id].alphaBlendOp = (VkBlendOp)pAttachment->AlphaBlendOp;
    }

    void VKComputePipelineBuildInfo::Init()
    {
        ZoneScoped;

        m_CreateInfo = {};
        m_CreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        m_CreateInfo.pNext = nullptr;
    }

    void VKComputePipelineBuildInfo::SetShader(BaseShader *pShader, eastl::string_view entryPoint)
    {
        ZoneScoped;

        VkPipelineShaderStageCreateInfo &shaderStage = m_CreateInfo.stage;
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.pNext = nullptr;
        shaderStage.flags = 0;

        shaderStage.stage = VKAPI::ToVKShaderType(pShader->Type);
        shaderStage.pName = entryPoint.data();
        shaderStage.module = ((VKShader *)pShader)->pHandle;
        shaderStage.pSpecializationInfo = nullptr;
    }

}  // namespace lr::Graphics