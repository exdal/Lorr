#include "Engine.hh"

#include "Core/Graphics/Camera.hh"
#include "Core/Graphics/RHI/D3D12/D3D12API.hh"
#include "Core/Graphics/RHI/Vulkan/VKAPI.hh"

#include "Core/IO/MemoryAllocator.hh"
#include "Core/Utils/Timer.hh"

#include "UI/ImGuiRenderer.hh"

namespace lr
{
    using namespace Graphics;

    BasePipeline *pPipeline = nullptr;
    BaseDescriptorSet *pDescriptorSet = nullptr;

    BaseBuffer *pConstantBuffer = nullptr;
    BaseBuffer *pVertexBuffer = nullptr;
    BaseBuffer *pIndexBuffer = nullptr;

    UI::ImGuiRenderer imguiHandler;

    void Engine::Init(ApplicationDesc &desc, WindowDesc &windowDesc)
    {
        ZoneScoped;

        Logger::Init();

        m_Window.Init(windowDesc);

        m_pAPI = new VKAPI;
        m_pAPI->Init(&m_Window, windowDesc.Width, windowDesc.Height, APIFlags::None);

        Camera3DDesc camera3DDesc;
        camera3DDesc.Position = XMFLOAT3(0.0, 0.0, -3.0);
        camera3DDesc.ViewSize = XMFLOAT2(windowDesc.Width, windowDesc.Height);
        camera3DDesc.ViewDirection = XMFLOAT3(0.0, 0.0, 1.0);
        camera3DDesc.UpDirection = XMFLOAT3(0.0, 1.0, 0.0);
        camera3DDesc.FOV = 60.0;
        camera3DDesc.ZFar = 10000.0;
        camera3DDesc.ZNear = 0.1;

        m_Camera3D.Init(&camera3DDesc);

        Camera2DDesc camera2DDesc;
        camera2DDesc.Position = XMFLOAT2(0.0, 0.0);
        camera2DDesc.ViewSize = XMFLOAT2(windowDesc.Width, windowDesc.Height);
        camera2DDesc.ZFar = 1.0;
        camera2DDesc.ZNear = 0.0;

        m_Camera2D.Init(&camera2DDesc);

        BufferDesc constantBufDesc = {};
        BufferData constantBufData = {};

        constantBufDesc.Mappable = true;
        constantBufDesc.TargetAllocator = AllocatorType::Descriptor;
        constantBufDesc.UsageFlags = ResourceUsage::ConstantBuffer;

        constantBufData.DataLen = 256;

        pConstantBuffer = m_pAPI->CreateBuffer(&constantBufDesc, &constantBufData);

        DescriptorSetDesc descriptorDesc = {};
        descriptorDesc.BindingCount = 1;
        descriptorDesc.pBindings[0].Type = DescriptorType::ConstantBufferView;
        descriptorDesc.pBindings[0].TargetShader = ShaderStage::Vertex;
        descriptorDesc.pBindings[0].ArraySize = 1;
        descriptorDesc.pBindings[0].pBuffer = pConstantBuffer;

        pDescriptorSet = m_pAPI->CreateDescriptorSet(&descriptorDesc);
        m_pAPI->UpdateDescriptorData(pDescriptorSet);

        InputLayout presentLayout = {
            { VertexAttribType::Vec3, "POSITION" },
            { VertexAttribType::Vec3, "COLOR" },
        };

        BaseShader *pVertexShader = m_pAPI->CreateShader(ShaderStage::Vertex, "test.v.spv");
        BaseShader *pPixelShader = m_pAPI->CreateShader(ShaderStage::Pixel, "test.p.spv");

        GraphicsPipelineBuildInfo *pBuildInfo = m_pAPI->BeginPipelineBuildInfo();

        pBuildInfo->SetInputLayout(presentLayout);
        pBuildInfo->SetShader(pVertexShader, "main");
        pBuildInfo->SetShader(pPixelShader, "main");
        pBuildInfo->SetFillMode(FillMode::Fill);
        pBuildInfo->SetDepthState(false, false);
        pBuildInfo->SetDepthFunction(DepthCompareOp::LessEqual);
        pBuildInfo->SetCullMode(CullMode::None, false);
        pBuildInfo->SetDescriptorSets({ pDescriptorSet });

        PipelineAttachment colorAttachment = {};
        colorAttachment.Format = ResourceFormat::RGBA8F;
        colorAttachment.BlendEnable = false;
        colorAttachment.WriteMask = 0xf;
        colorAttachment.SrcBlend = BlendFactor::SrcAlpha;
        colorAttachment.DstBlend = BlendFactor::InvSrcAlpha;
        colorAttachment.SrcBlendAlpha = BlendFactor::One;
        colorAttachment.DstBlendAlpha = BlendFactor::InvSrcAlpha;
        colorAttachment.Blend = BlendOp::Add;
        colorAttachment.BlendAlpha = BlendOp::Add;
        pBuildInfo->AddAttachment(&colorAttachment, false);

        pPipeline = m_pAPI->EndPipelineBuildInfo(pBuildInfo);

        // imguiHandler.Init();

        struct VertexData
        {
            XMFLOAT3 Position;
            XMFLOAT3 Color;
        };

        VertexData pData[3] = { { { 0, -0.5, 0 }, { 1.0, 0.0, 0.0 } },
                                { { 0.5, 0.5, 0 }, { 0.0, 1.0, 0.0 } },
                                { { -0.5, 0.5, 0 }, { 0.0, 0.0, 1.0 } } };
        u32 pIndexData[3] = { 0, 1, 2 };

        //~ Idea:
        //~ Create a pool that lasts one frame, it would be very useful for staging buffers

        BufferDesc bufDesc = {};
        bufDesc.UsageFlags = ResourceUsage::VertexBuffer | ResourceUsage::CopySrc;
        bufDesc.Mappable = true;
        bufDesc.TargetAllocator = AllocatorType::None;

        BufferData bufData = {};
        bufData.DataLen = sizeof(VertexData) * 3;
        bufData.Stride = sizeof(VertexData);

        BaseBuffer *pTempVertexBuffer = m_pAPI->CreateBuffer(&bufDesc, &bufData);

        void *pMapData = nullptr;
        m_pAPI->MapMemory(pTempVertexBuffer, pMapData);
        memcpy(pMapData, pData, bufData.DataLen);
        m_pAPI->UnmapMemory(pTempVertexBuffer);

        /// ------------------------------------------------------- ///

        bufDesc.UsageFlags = ResourceUsage::IndexBuffer | ResourceUsage::CopySrc;
        bufDesc.Mappable = true;

        bufData.DataLen = sizeof(u32) * 3;

        BaseBuffer *pTempIndexBuffer = m_pAPI->CreateBuffer(&bufDesc, &bufData);

        m_pAPI->MapMemory(pTempIndexBuffer, pMapData);
        memcpy(pMapData, pIndexData, bufData.DataLen);
        m_pAPI->UnmapMemory(pTempIndexBuffer);

        /// ------------------------------------------------------- ///

        BaseCommandList *pList = m_pAPI->GetCommandList();
        m_pAPI->BeginCommandList(pList);

        pVertexBuffer = m_pAPI->ChangeAllocator(pList, pTempVertexBuffer, AllocatorType::BufferTLSF);
        pIndexBuffer = m_pAPI->ChangeAllocator(pList, pTempIndexBuffer, AllocatorType::BufferTLSF);

        m_pAPI->EndCommandList(pList);
        m_pAPI->ExecuteCommandList(pList, true);

        m_pAPI->DeleteBuffer(pTempVertexBuffer);
        m_pAPI->DeleteBuffer(pTempIndexBuffer);

        Run();
    }

    void Engine::OnWindowResize(u32 width, u32 height)
    {
        ZoneScoped;

        m_pAPI->ResizeSwapChain(width, height);
        m_Camera3D.SetSize(XMFLOAT2(width, height));
        m_Camera2D.SetSize(XMFLOAT2(width, height));
    }

    void Engine::Run()
    {
        ZoneScoped;

        Timer timer;

        while (!m_Window.ShouldClose())
        {
            f32 deltaTime = timer.elapsed();
            timer.reset();

            // static f32 angleX = 0;
            // angleX += 45.0 * deltaTime;

            // m_Camera3D.SetDirectionAngle(XMFLOAT2(angleX, 0.0));

            m_Camera3D.Update(0, 25.0);
            m_Camera2D.Update(0, 25.0);

            XMMATRIX mat3D = XMMatrixMultiply(m_Camera3D.m_View, m_Camera3D.m_Projection);
            XMMATRIX mat2D = XMMatrixMultiply(m_Camera2D.m_View, m_Camera2D.m_Projection);

            void *pMapData = nullptr;
            m_pAPI->MapMemory(pConstantBuffer, pMapData);
            memcpy(pMapData, &mat3D, sizeof(XMMATRIX));
            m_pAPI->UnmapMemory(pConstantBuffer);

            BaseCommandList *pList = m_pAPI->GetCommandList();
            BaseImage *pImage = m_pAPI->GetSwapChain()->GetCurrentImage();

            m_pAPI->BeginCommandList(pList);

            pList->BarrierTransition(pImage, ResourceUsage::Undefined, ShaderStage::None, ResourceUsage::RenderTarget, ShaderStage::None);

            CommandListAttachment attachment;
            attachment.pHandle = pImage;
            attachment.LoadOp = AttachmentOperation::Clear;
            attachment.StoreOp = AttachmentOperation::Store;

            CommandListBeginDesc beginDesc = {};
            beginDesc.RenderArea = { 0, 0, m_Window.m_Width, m_Window.m_Height };
            beginDesc.ColorAttachmentCount = 1;
            beginDesc.pColorAttachments[0] = attachment;
            pList->BeginPass(&beginDesc);

            pList->SetPipeline(pPipeline);
            pList->SetPipelineDescriptorSets({ pDescriptorSet });

            pList->SetViewport(0, 0, 0, m_Window.m_Width, m_Window.m_Height);
            pList->SetScissors(0, 0, 0, m_Window.m_Width, m_Window.m_Height);
            pList->SetPrimitiveType(PrimitiveType::TriangleList);

            pList->SetVertexBuffer(pVertexBuffer);
            pList->SetIndexBuffer(pIndexBuffer);
            pList->DrawIndexed(3);

            pList->EndPass();

            pList->BarrierTransition(pImage, ResourceUsage::RenderTarget, ShaderStage::None, ResourceUsage::Present, ShaderStage::None);

            m_pAPI->EndCommandList(pList);
            m_pAPI->ExecuteCommandList(pList, false);

            // imguiHandler.NewFrame();
            // ImGui::Begin("Hello");
            // ImGui::Button("a");
            // ImGui::ProgressBar(100.5, ImVec2(0, 0), "Test");
            // ImGui::Image(0, ImVec2(512, 128));
            // ImGui::End();
            // imguiHandler.EndFrame();

            m_pAPI->Frame();
            m_Window.Poll();
        }
    }

}  // namespace lr