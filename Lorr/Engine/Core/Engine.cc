#include "Engine.hh"

#include "Utils/Timer.hh"

namespace lr
{
    using namespace g;

    VKPipeline pipeline;
    VKDescriptorSet descriptorSet;

    void Engine::Init(ApplicationDesc &desc, WindowDesc &windowDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(windowDesc);
        m_ThreadPoolSPSCAt.Init();

        m_pAPI = new VKAPI;
        m_pAPI->Init(&m_Window, windowDesc.Width, windowDesc.Height, APIFlags::VSync);

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

        /// -------------------------------------------------------

        VkRenderPass pRenderPass = m_pAPI->m_SwapChain.m_pRenderPass;

        InputLayout inputLayout = {
            { VertexAttribType::Vec4, "POSITION" },
        };

        m_pAPI->CreateDescriptorSetLayout(&descriptorSet, { { 0, DescriptorType::ConstantBuffer, VK_SHADER_STAGE_FRAGMENT_BIT } });

        /// -------------------------------------------------------

        VKGraphicsPipelineBuildInfo buildInfo = {};
        m_pAPI->BeginPipelineBuildInfo(buildInfo, pRenderPass);

        buildInfo.SetInputLayout(0, inputLayout);
        buildInfo.SetShaderStage(vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
        buildInfo.SetShaderStage(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
        buildInfo.SetPrimitiveType(PrimitiveType::TriangleList);
        buildInfo.AddViewport(1, 1);
        buildInfo.SetViewport(0, 1280, 720, 0.01, 1.0);
        buildInfo.SetScissor(0, 0, 0, 1280, 720);
        buildInfo.SetDynamicState({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR });
        buildInfo.SetCullMode(CullMode::Back, true);

        m_pAPI->EndPipelineBuildInfo(buildInfo, &pipeline, { &descriptorSet });

        Run();
    }

    void Engine::Run()
    {
        ZoneScoped;

        while (!m_Window.ShouldClose())
        {
            VKCommandList *pList = m_pAPI->GetCurrentCommandList();

            CommandRenderPassBeginInfo beginInfo = {};
            beginInfo.ClearValue.RenderTargetColor = { 0.0, 0.0, 0.0, 1.0 };

            pList->BeginRenderPass(beginInfo);
            pList->SetPipeline(&pipeline);
            pList->SetPipelineDescriptorSets({ &descriptorSet });
            // pList->DrawIndexed(3);
            pList->EndRenderPass();

            m_pAPI->Frame();
            m_Window.Poll();
        }
    }

}  // namespace lr