#include "VKPipeline.hh"

#include "VKAPI.hh"

namespace lr::Graphics
{
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

        m_VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_TessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

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

        m_ViewportState.pViewports = m_pViewports;
        m_ViewportState.pScissors = m_pScissors;

        /// ------------------------------------- ///

        m_RasterizationState.lineWidth = 1.0;
        SetPrimitiveType(PrimitiveType::TriangleList);

        /// ------------------------------------- ///

        m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        /// ------------------------------------- ///

        m_ColorBlendState.pAttachments = m_pColorBlendAttachments;

        /// ------------------------------------- ///

        m_DynamicState.pDynamicStates = m_DynamicStates;
    }

    void VKGraphicsPipelineBuildInfo::SetShader(BaseShader *pShader)
    {
        ZoneScoped;

        // VkPipelineShaderStageCreateInfo &shaderStage = m_pShaderStages[m_CreateInfo.stageCount++];
        // shaderStage.stage = stage;
        // shaderStage.pName = entryPoint.data();
        // shaderStage.module = pShader;
    }

    void VKGraphicsPipelineBuildInfo::SetInputLayout(u32 bindingID, InputLayout &inputLayout)
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
            attribDesc.format = VKAPI::ToVulkanFormat(element.m_Type);
        }
    }

    void VKGraphicsPipelineBuildInfo::SetPrimitiveType(PrimitiveType type)
    {
        ZoneScoped;

        m_InputAssemblyState.topology = VKAPI::ToVulkanTopology(type);
    }

    void VKGraphicsPipelineBuildInfo::SetPatchCount(u32 count)
    {
        ZoneScoped;

        m_TessellationState.patchControlPoints = count;
    }

    void VKGraphicsPipelineBuildInfo::AddViewport(u32 viewportCount, u32 scissorCount)
    {
        ZoneScoped;

        m_ViewportState.viewportCount += viewportCount;
        m_ViewportState.scissorCount += scissorCount;
    }

    void VKGraphicsPipelineBuildInfo::SetViewport(u32 viewportID, u32 width, u32 height, f32 minDepth, f32 maxDepth)
    {
        ZoneScoped;

        VkViewport &vp = m_pViewports[viewportID];

        vp.width = width;
        vp.height = height;
        vp.x = 0;
        vp.y = 0;
        vp.minDepth = minDepth;
        vp.maxDepth = maxDepth;
    }

    void VKGraphicsPipelineBuildInfo::SetScissor(u32 scissorID, u32 x, u32 y, u32 w, u32 h)
    {
        ZoneScoped;

        VkRect2D &sc = m_pScissors[scissorID];

        sc.offset.x = x;
        sc.offset.y = y;
        sc.extent.width = w;
        sc.extent.height = h;
    }

    void VKGraphicsPipelineBuildInfo::SetDepthClamp(bool enabled)
    {
        ZoneScoped;

        m_RasterizationState.depthBiasClamp = enabled;
    }

    void VKGraphicsPipelineBuildInfo::SetRasterizerDiscard(bool enabled)
    {
        ZoneScoped;

        m_RasterizationState.rasterizerDiscardEnable = enabled;
    }

    void VKGraphicsPipelineBuildInfo::SetPolygonMode(VkPolygonMode mode)
    {
        ZoneScoped;

        m_RasterizationState.polygonMode = mode;
    }

    void VKGraphicsPipelineBuildInfo::SetCullMode(CullMode mode, bool frontFaceClockwise)
    {
        ZoneScoped;

        m_RasterizationState.cullMode = VKAPI::ToVulkanCullMode(mode);
        m_RasterizationState.frontFace = frontFaceClockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }

    void VKGraphicsPipelineBuildInfo::SetDepthBias(bool enabled, f32 constantFactor, f32 clamp, f32 slopeFactor)
    {
        ZoneScoped;

        m_RasterizationState.depthBiasEnable = enabled;
        m_RasterizationState.depthBiasConstantFactor = constantFactor;
        m_RasterizationState.depthBiasClamp = clamp;
        m_RasterizationState.depthBiasSlopeFactor = slopeFactor;
    }

    void VKGraphicsPipelineBuildInfo::SetLineWidth(f32 size)
    {
        ZoneScoped;

        m_RasterizationState.lineWidth = size;
    }

    void VKGraphicsPipelineBuildInfo::SetSampleCount(u32 bits)
    {
        ZoneScoped;

        m_MultisampleState.rasterizationSamples = (VkSampleCountFlagBits)bits;
    }

    void VKGraphicsPipelineBuildInfo::SetSampledShading(bool enabled, f32 minSampleShading)
    {
        ZoneScoped;
    }

    void VKGraphicsPipelineBuildInfo::SetAlphaToCoverage(bool alphaToCoverage, bool alphaToOne)
    {
        ZoneScoped;
    }

    void VKGraphicsPipelineBuildInfo::SetDepthState(bool depthTestEnabled, bool depthWriteEnabled, bool depthBoundsTestEnabled)
    {
        ZoneScoped;

        m_DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        m_DepthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
        m_DepthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
        m_DepthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
        m_DepthStencilState.front = m_DepthStencilState.front;

        m_DepthStencilState.depthTestEnable = depthTestEnabled;
        m_DepthStencilState.depthWriteEnable = depthWriteEnabled;
        m_DepthStencilState.depthBoundsTestEnable = depthBoundsTestEnabled;
    }

    void VKGraphicsPipelineBuildInfo::SetStencilState(bool stencilTestEnabled)
    {
        ZoneScoped;

        m_DepthStencilState.stencilTestEnable = stencilTestEnabled;
    }

    void VKGraphicsPipelineBuildInfo::SetStencilOperation(DepthStencilOpDesc front, DepthStencilOpDesc back)
    {
        ZoneScoped;

        m_DepthStencilState.front.compareOp = (VkCompareOp)front.Function;
        m_DepthStencilState.front.depthFailOp = (VkStencilOp)front.DepthFail;
        m_DepthStencilState.front.failOp = (VkStencilOp)front.Fail;
        m_DepthStencilState.front.passOp = (VkStencilOp)front.Pass;

        m_DepthStencilState.back.compareOp = (VkCompareOp)back.Function;
        m_DepthStencilState.back.depthFailOp = (VkStencilOp)back.DepthFail;
        m_DepthStencilState.back.failOp = (VkStencilOp)back.Fail;
        m_DepthStencilState.back.passOp = (VkStencilOp)back.Pass;
    }

    void VKGraphicsPipelineBuildInfo::SetDepthBounds(f32 min, f32 max)
    {
        ZoneScoped;

        m_DepthStencilState.minDepthBounds = min;
        m_DepthStencilState.maxDepthBounds = max;
    }

    void VKGraphicsPipelineBuildInfo::SetBlendAttachment(u32 attachmentID, bool enabled, u8 mask)
    {
        ZoneScoped;

        u32 &count = m_ColorBlendState.attachmentCount;
        count = eastl::max(count, attachmentID + 1);

        m_pColorBlendAttachments[attachmentID].colorWriteMask = mask;
        m_pColorBlendAttachments[attachmentID].blendEnable = enabled;

        m_pColorBlendAttachments[attachmentID].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        m_pColorBlendAttachments[attachmentID].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        m_pColorBlendAttachments[attachmentID].colorBlendOp = VK_BLEND_OP_ADD;
        m_pColorBlendAttachments[attachmentID].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        m_pColorBlendAttachments[attachmentID].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        m_pColorBlendAttachments[attachmentID].alphaBlendOp = VK_BLEND_OP_ADD;
    }

}  // namespace lr::Graphics