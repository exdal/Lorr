#include "VKPipeline.hh"

#include "VKAPI.hh"

namespace lr::Graphics
{
    void VKGraphicsPipelineBuildInfo::SetShaderStage(VkShaderModule pShader, VkShaderStageFlagBits stage, eastl::string_view entryPoint)
    {
        ZoneScoped;

        VkPipelineShaderStageCreateInfo &shaderStage = m_pShaderStages[m_CreateInfo.stageCount++];
        shaderStage.stage = stage;
        shaderStage.pName = entryPoint.data();
        shaderStage.module = pShader;
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

    void VKGraphicsPipelineBuildInfo::SetSampleCount(VkSampleCountFlagBits bits)
    {
        ZoneScoped;

        m_MultisampleState.rasterizationSamples = bits;
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

    void VKGraphicsPipelineBuildInfo::SetStencilState(bool stencilTestEnabled, VkStencilOpState frontState, VkStencilOpState backState)
    {
        ZoneScoped;

        m_DepthStencilState.stencilTestEnable = stencilTestEnabled;
        m_DepthStencilState.front = frontState;
        m_DepthStencilState.back = backState;
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
    }

    void VKGraphicsPipelineBuildInfo::SetDynamicState(const std::initializer_list<VkDynamicState> &dynamicState)
    {
        ZoneScoped;

        memcpy(m_DynamicStates, dynamicState.begin(), dynamicState.size() * sizeof(VkDynamicState));
    }

    void VKPipelineManager::Init(VKAPI *pAPI)
    {
        ZoneScoped;

        m_pPipelineCache = pAPI->CreatePipelineCache();
    }

    void VKPipelineManager::InitBuildInfo(VKGraphicsPipelineBuildInfo &buildInfo, VkRenderPass pRenderPass)
    {
        ZoneScoped;

        buildInfo.m_pSetRenderPass = pRenderPass;

        VkGraphicsPipelineCreateInfo &createInfo = buildInfo.m_CreateInfo;

        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        createInfo.pVertexInputState = &buildInfo.m_VertexInputState;
        createInfo.pInputAssemblyState = &buildInfo.m_InputAssemblyState;
        createInfo.pTessellationState = &buildInfo.m_TessellationState;
        createInfo.pViewportState = &buildInfo.m_ViewportState;
        createInfo.pRasterizationState = &buildInfo.m_RasterizationState;
        createInfo.pMultisampleState = &buildInfo.m_MultisampleState;
        createInfo.pDepthStencilState = &buildInfo.m_DepthStencilState;
        createInfo.pColorBlendState = &buildInfo.m_ColorBlendState;
        createInfo.pDynamicState = &buildInfo.m_DynamicState;
        createInfo.pStages = buildInfo.m_pShaderStages;

        buildInfo.m_VertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        buildInfo.m_InputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        buildInfo.m_TessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        buildInfo.m_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        buildInfo.m_RasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        buildInfo.m_MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        buildInfo.m_DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        buildInfo.m_ColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        buildInfo.m_DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

        for (u32 i = 0; i < VKGraphicsPipelineBuildInfo::kMaxShaderStageCount; i++)
            buildInfo.m_pShaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        /// ------------------------------------- ///

        VkVertexInputBindingDescription &vertexBinding = buildInfo.m_VertexBindingDesc;
        vertexBinding.binding = 0;
        vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        buildInfo.m_VertexInputState.vertexBindingDescriptionCount = 1;
        buildInfo.m_VertexInputState.pVertexBindingDescriptions = &buildInfo.m_VertexBindingDesc;
        buildInfo.m_VertexInputState.pVertexAttributeDescriptions = buildInfo.m_pVertexAttribs;

        /// ------------------------------------- ///

        buildInfo.m_ViewportState.pViewports = buildInfo.m_pViewports;
        buildInfo.m_ViewportState.pScissors = buildInfo.m_pScissors;

        /// ------------------------------------- ///

        buildInfo.m_RasterizationState.lineWidth = 1.0;
        buildInfo.SetPrimitiveType(PrimitiveType::TriangleList);

        /// ------------------------------------- ///

        buildInfo.m_MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        /// ------------------------------------- ///

        buildInfo.m_ColorBlendState.pAttachments = buildInfo.m_pColorBlendAttachments;

        /// ------------------------------------- ///

        buildInfo.m_DynamicState.pDynamicStates = buildInfo.m_DynamicStates;
    }

}  // namespace lr::Graphics