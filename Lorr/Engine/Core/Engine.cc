#include "Engine.hh"

#include "Graphics/Camera.hh"

#include "IO/MemoryAllocator.hh"

#include "Utils/Timer.hh"

namespace lr
{
    using namespace g;
    using namespace Graphics;

    Camera3D camera;

    VKPipeline pipeline;
    VKDescriptorSet descriptorSet;
    VKBuffer vertexBuffer;
    VKBuffer indexBuffer;
    VKBuffer constantBuffer;

    Memory::TLSFMemoryAllocator memoryAllocator;

    void Engine::Init(ApplicationDesc &desc, WindowDesc &windowDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(windowDesc);
        m_ThreadPoolSPSCAt.Init();

        m_pAPI = new VKAPI;
        m_pAPI->Init(&m_Window, windowDesc.Width, windowDesc.Height, APIFlags::VSync);

        Camera3DDesc cameraDesc;
        cameraDesc.Position = XMFLOAT3(0.0, 0.0, -5.0);
        cameraDesc.ViewSize = XMFLOAT2(windowDesc.Width, windowDesc.Height);
        cameraDesc.ViewDirection = XMFLOAT3(0.0, 0.0, 1.0);
        cameraDesc.UpDirection = XMFLOAT3(0.0, 1.0, 0.0);
        cameraDesc.FOV = 65.0;
        cameraDesc.ZFar = 10000.0;
        cameraDesc.ZNear = 0.1;

        camera.Init(&cameraDesc);

        /// -------------------------------------------------------

        FileStream fsVert("test.v.spv", false);
        FileStream fsFrag("test.f.spv", false);

        void *pVertData = fsVert.ReadAll<void>();
        void *pFragData = fsFrag.ReadAll<void>();

        fsVert.Close();
        fsFrag.Close();

        BufferReadStream bufVert(pVertData, fsVert.Size());
        BufferReadStream bufFrag(pFragData, fsFrag.Size());

        VkShaderModule vertexShader = m_pAPI->CreateShaderModule(bufVert);
        VkShaderModule fragmentShader = m_pAPI->CreateShaderModule(bufFrag);

        free(pVertData);
        free(pFragData);

        /// ------------------------------------------------------- ///

        VkRenderPass pRenderPass = m_pAPI->m_SwapChain.m_pRenderPass;

        InputLayout inputLayout = {
            { VertexAttribType::Vec3, "POSITION" },
        };

        BufferDesc constantBufferDesc = {};
        constantBufferDesc.UsageFlags = BufferUsage::ConstantBuffer;
        constantBufferDesc.Mappable = true;

        BufferData constantBufferData = {};
        constantBufferData.DataLen = sizeof(XMMATRIX);

        m_pAPI->CreateBuffer(&constantBuffer, &constantBufferDesc, &constantBufferData);
        m_pAPI->AllocateBufferMemory(&constantBuffer, AllocatorType::Linear);

        m_pAPI->BindMemory(&constantBuffer);

        VKDescriptorSetDesc descriptorDesc = {};
        descriptorDesc.Bindings = { { 0, DescriptorType::ConstantBuffer, VK_SHADER_STAGE_VERTEX_BIT, 1, &constantBuffer, nullptr } };

        m_pAPI->CreateDescriptorSetLayout(&descriptorSet, &descriptorDesc);

        /// ------------------------------------------------------- ///

        VKGraphicsPipelineBuildInfo buildInfo = {};
        m_pAPI->BeginPipelineBuildInfo(buildInfo, pRenderPass);

        buildInfo.SetInputLayout(0, inputLayout);
        buildInfo.SetShaderStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
        buildInfo.SetShaderStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
        buildInfo.SetPrimitiveType(PrimitiveType::TriangleList);
        buildInfo.AddViewport(1, 1);
        buildInfo.SetViewport(0, 1280, 780, 0.0, 1.0);
        buildInfo.SetScissor(0, 0, 0, 1280, 780);
        buildInfo.SetDepthState(true, true, false);
        buildInfo.SetBlendAttachment(0, false, 0xf);
        // buildInfo.SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT });
        buildInfo.SetCullMode(CullMode::None, false);

        m_pAPI->EndPipelineBuildInfo(buildInfo, &pipeline, { &descriptorSet });

        XMFLOAT3 pData[3] = { { 0, -0.5, 0 }, { 0.5, 0.5, 0 }, { -0.5, 0.5, 0 } };
        u32 pIndexData[3] = { 0, 1, 2 };

        BufferDesc bufDesc = {};
        bufDesc.UsageFlags = BufferUsage::Vertex;
        bufDesc.Mappable = true;

        BufferData bufData = {};
        bufData.DataLen = sizeof(XMFLOAT3) * 3;

        m_pAPI->CreateBuffer(&vertexBuffer, &bufDesc, &bufData);
        m_pAPI->AllocateBufferMemory(&vertexBuffer, AllocatorType::TLSF);

        void *pMapData = nullptr;
        m_pAPI->MapMemory(&vertexBuffer, pMapData);
        memcpy(pMapData, pData, bufData.DataLen);
        m_pAPI->UnmapMemory(&vertexBuffer);

        m_pAPI->BindMemory(&vertexBuffer);

        /// ------------------------------------------------------- ///

        bufDesc.UsageFlags = BufferUsage::Index;
        bufDesc.Mappable = true;

        bufData.DataLen = sizeof(u32) * 3;

        m_pAPI->CreateBuffer(&indexBuffer, &bufDesc, &bufData);
        m_pAPI->AllocateBufferMemory(&indexBuffer, AllocatorType::TLSF);

        m_pAPI->MapMemory(&indexBuffer, pMapData);
        memcpy(pMapData, pIndexData, bufData.DataLen);
        m_pAPI->UnmapMemory(&indexBuffer);

        m_pAPI->BindMemory(&indexBuffer);

        Run();
    }

    void Engine::Run()
    {
        ZoneScoped;

        while (!m_Window.ShouldClose())
        {
            camera.Update(0, 25.0);
            XMMATRIX mat = XMMatrixMultiply(camera.m_View, camera.m_Projection);

            void *pMapData = nullptr;
            m_pAPI->MapMemory(&constantBuffer, pMapData);
            memcpy(pMapData, &mat, sizeof(XMMATRIX));
            m_pAPI->UnmapMemory(&constantBuffer);

            VKCommandList *pList = m_pAPI->GetCurrentCommandList();

            CommandRenderPassBeginInfo beginInfo = {};
            beginInfo.pClearValues[0].RenderTargetColor = { 0.0, 0.0, 0.0, 1.0 };
            beginInfo.pClearValues[1].DepthStencil.Depth = 1.0;
            beginInfo.pClearValues[1].DepthStencil.Stencil = 0xff;

            pList->BeginRenderPass(beginInfo);
            pList->SetPipeline(&pipeline);
            pList->SetPipelineDescriptorSets({ &descriptorSet });

            pList->SetVertexBuffer(&vertexBuffer);
            pList->SetIndexBuffer(&indexBuffer);
            pList->DrawIndexed(3);

            pList->EndRenderPass();

            m_pAPI->Frame();
            m_Window.Poll();
        }
    }

}  // namespace lr