#include "VKAPI.hh"

namespace lr::Graphics
{
    void APIStateManager::Init(VKAPI *pAPI)
    {
        ZoneScoped;

        m_pAPI = pAPI;

        BufferDesc bufferDesc = {};
        BufferData bufferData = {};
        // VKDescriptorSetDesc descriptorDesc = {};
        // VKGraphicsPipelineBuildInfo graphicsBuildInfo = {};
        VkShaderModule pVertexShader = nullptr;
        VkShaderModule pPixelShader = nullptr;

        /// Geometry Pass --------------------------------------------- ///

        // {
        //     RenderPassAttachment colorAttachment;
        //     colorAttachment.Format = ResourceFormat::RGBA8F;
        //     colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        //     colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        //     colorAttachment.InitialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        //     colorAttachment.FinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //     RenderPassAttachment depthAttachment;
        //     depthAttachment.Format = ResourceFormat::D32FS8U;
        //     depthAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        //     depthAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        //     depthAttachment.StencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        //     depthAttachment.StencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        //     depthAttachment.FinalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        //     RenderPassSubpassDesc subpassDesc;
        //     subpassDesc.ColorAttachmentCount = 1;
        //     subpassDesc.ppColorAttachments[0] = &colorAttachment;
        //     subpassDesc.pDepthAttachment = &depthAttachment;

        //     RenderPassDesc renderPassDesc;
        //     renderPassDesc.pSubpassDesc = &subpassDesc;
        //     m_pGeometryPass = pAPI->CreateRenderPass(&renderPassDesc);
        // }

        // /// UI Pass --------------------------------------------------- ///

        // {
        //     RenderPassAttachment colorAttachment;
        //     colorAttachment.Format = ResourceFormat::RGBA8F;
        //     colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        //     colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        //     colorAttachment.InitialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        //     colorAttachment.FinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //     RenderPassSubpassDesc subpassDesc;
        //     subpassDesc.ColorAttachmentCount = 1;
        //     subpassDesc.ppColorAttachments[0] = &colorAttachment;

        //     RenderPassDesc renderPassDesc;
        //     renderPassDesc.pSubpassDesc = &subpassDesc;
        //     m_pUIPass = pAPI->CreateRenderPass(&renderPassDesc);
        // }

        // /// Present Pass ---------------------------------------------- ///

        // {
        //     RenderPassAttachment colorAttachment;
        //     colorAttachment.Format = ResourceFormat::BGRA8F;
        //     colorAttachment.LoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        //     colorAttachment.StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        //     colorAttachment.InitialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        //     colorAttachment.FinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        //     RenderPassSubpassDesc subpassDesc;
        //     subpassDesc.ColorAttachmentCount = 1;
        //     subpassDesc.ppColorAttachments[0] = &colorAttachment;

        //     RenderPassDesc renderPassDesc;
        //     renderPassDesc.pSubpassDesc = &subpassDesc;
        //     m_pPresentPass = pAPI->CreateRenderPass(&renderPassDesc);
        // }

        /// ----------------------------------------------------------- ///

        // bufferDesc.UsageFlags = BufferUsage::ConstantBuffer;
        // bufferDesc.Mappable = true;

        // bufferData.DataLen = sizeof(XMMATRIX);

        // pAPI->CreateBuffer(&m_Camera3DBuffer, &bufferDesc, &bufferData);
        // pAPI->AllocateBufferMemory(&m_Camera3DBuffer, AllocatorType::Descriptor);
        // pAPI->BindMemory(&m_Camera3DBuffer);

        // pAPI->CreateBuffer(&m_Camera2DBuffer, &bufferDesc, &bufferData);
        // pAPI->AllocateBufferMemory(&m_Camera2DBuffer, AllocatorType::Descriptor);
        // pAPI->BindMemory(&m_Camera2DBuffer);

        /// ----------------------------------------------------------- ///

        // descriptorDesc.BindingCount = 1;
        // descriptorDesc.pBindings[0].Type = DescriptorType::ConstantBuffer;
        // descriptorDesc.pBindings[0].ShaderStageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // descriptorDesc.pBindings[0].ArraySize = 1;

        // descriptorDesc.pBindings[0].pBuffer = &m_Camera3DBuffer;
        // pAPI->CreateDescriptorSetLayout(&m_Camera3DDescriptor, &descriptorDesc);
        // pAPI->UpdateDescriptorData(&m_Camera3DDescriptor, &descriptorDesc);

        // descriptorDesc.pBindings[0].pBuffer = &m_Camera2DBuffer;
        // pAPI->CreateDescriptorSetLayout(&m_Camera2DDescriptor, &descriptorDesc);
        // pAPI->UpdateDescriptorData(&m_Camera2DDescriptor, &descriptorDesc);

        // descriptorDesc.pBindings[0].Type = DescriptorType::CombinedSampler;
        // descriptorDesc.pBindings[0].ShaderStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        // pAPI->CreateDescriptorSetLayout(&m_UISamplerDescriptor, &descriptorDesc);

        /// ----------------------------------------------------------- ///

        /// BUFFER LAYOUTS ///
        InputLayout presentLayout = {
            { VertexAttribType::Vec3, "POSITION" },
        };

        InputLayout UILayout = {
            { Graphics::VertexAttribType::Vec2, "POSITION" },
            { Graphics::VertexAttribType::Vec2, "TEXCOORD" },
            { Graphics::VertexAttribType::Vec4U_Packed, "COLOR" },
        };

        /// ----------------------------------------------------------- ///

        /// GEOMETRY PIPELINE ///

        pVertexShader = m_pAPI->CreateShaderModule("test.v.spv");
        pPixelShader = m_pAPI->CreateShaderModule("test.f.spv");

        // m_pAPI->BeginPipelineBuildInfo(graphicsBuildInfo, m_pGeometryPass);

        // graphicsBuildInfo.SetInputLayout(0, presentLayout);
        // graphicsBuildInfo.SetShaderStage(pVertexShader, VK_SHADER_STAGE_VERTEX_BIT);
        // graphicsBuildInfo.SetShaderStage(pPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT);
        // graphicsBuildInfo.SetPrimitiveType(PrimitiveType::TriangleList);
        // graphicsBuildInfo.AddViewport(1, 1);
        // graphicsBuildInfo.SetDepthState(true, true, false);
        // graphicsBuildInfo.SetBlendAttachment(0, false, 0xf);
        // graphicsBuildInfo.SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        // graphicsBuildInfo.SetCullMode(CullMode::None, false);

        // m_pAPI->EndPipelineBuildInfo(graphicsBuildInfo, &m_GeometryPipeline, { &m_Camera3DDescriptor });

        pAPI->DeleteShaderModule(pVertexShader);
        pAPI->DeleteShaderModule(pPixelShader);

        // graphicsBuildInfo = {};

        /// UI PIPELINE ///

        pVertexShader = m_pAPI->CreateShaderModule("imgui.v.spv");
        pPixelShader = m_pAPI->CreateShaderModule("imgui.f.spv");

        // m_pAPI->BeginPipelineBuildInfo(graphicsBuildInfo, m_pUIPass);

        // graphicsBuildInfo.SetInputLayout(0, UILayout);
        // graphicsBuildInfo.SetShaderStage(pVertexShader, VK_SHADER_STAGE_VERTEX_BIT);
        // graphicsBuildInfo.SetShaderStage(pPixelShader, VK_SHADER_STAGE_FRAGMENT_BIT);
        // graphicsBuildInfo.SetPrimitiveType(Graphics::PrimitiveType::TriangleList);
        // graphicsBuildInfo.AddViewport(1, 1);
        // graphicsBuildInfo.SetDepthState(false, false, false);
        // graphicsBuildInfo.SetBlendAttachment(0, true, 0xf);
        // graphicsBuildInfo.SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        // graphicsBuildInfo.SetCullMode(Graphics::CullMode::None, false);

        // m_pAPI->EndPipelineBuildInfo(graphicsBuildInfo, &m_UIPipeline, { &m_Camera2DDescriptor, &m_UISamplerDescriptor });

        pAPI->DeleteShaderModule(pVertexShader);
        pAPI->DeleteShaderModule(pPixelShader);

        // graphicsBuildInfo = {};
    }

    void APIStateManager::UpdateCamera3DData(XMMATRIX mat)
    {
        ZoneScoped;

        void *pMapData = nullptr;
        m_pAPI->MapMemory(&m_Camera3DBuffer, pMapData);
        memcpy(pMapData, &mat, sizeof(XMMATRIX));
        m_pAPI->UnmapMemory(&m_Camera3DBuffer);
    }

    void APIStateManager::UpdateCamera2DData(XMMATRIX mat)
    {
        ZoneScoped;

        void *pMapData = nullptr;
        m_pAPI->MapMemory(&m_Camera2DBuffer, pMapData);
        memcpy(pMapData, &mat, sizeof(XMMATRIX));
        m_pAPI->UnmapMemory(&m_Camera2DBuffer);
    }

}  // namespace lr::Graphics