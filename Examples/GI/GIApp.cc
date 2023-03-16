#include "GIApp.hh"

#include "SceneData.hh"

using namespace lr;

struct PushConstantsCS
{
    XMFLOAT3 CameraPos;
    u32 Width;
    u32 Height;
    u32 Time;
    u32 TriangleCount;
};

struct GeometryDataCS
{
    XMVECTOR V1;
    XMVECTOR V2;
    XMVECTOR V3;
    XMFLOAT4 Normal;
    XMFLOAT4 Color;
};

static u32 gTime = 1;
static XMFLOAT3 gPosition = { 278.0, 273.0, -800.0 };

void GIApp::Init(BaseApplicationDesc &desc)
{
    ZoneScoped;

    PreInit(desc);

    // Renderer::RenderGraph &renderGraph = m_Engine.m_RenderGraph;

    /// SCENE GEOMETRY PRECOMPUTATION ///

    for (u32 i = 0; i < kSceneDataVertexCount; i++)
    {
        GeometryData &v = kSceneData[i];

        XMVECTOR E1 = XMVectorSubtract(v.Positions[1], v.Positions[0]);
        XMVECTOR E2 = XMVectorSubtract(v.Positions[2], v.Positions[0]);
        v.Normal = XMVector3Normalize(XMVector3Cross(E1, E2));
    }

    /// GPU RESOURCE CREATION ///

    struct GIPassData
    {
        Graphics::Image *m_pImage;
        Graphics::Buffer *m_pSceneDataBuffer;
        Graphics::DescriptorSet *m_pDescriptorSet;
        Graphics::Pipeline *m_pPipeline;
    };

    // renderGraph.AddGraphicsPassAfter<GIPassData>(
    //     "gi",
    //     "$head",
    //     [&](Renderer::RenderGraphResourceManager &resourceMan, GIPassData &passData)
    //     {
    //         Graphics::Image *pSwapChainImage = resourceMan.GetSwapChainImage();

    //         Graphics::ImageDesc imageDesc = {
    //             .m_UsageFlags = LR_RESOURCE_USAGE_SHADER_RESOURCE | LR_RESOURCE_USAGE_UNORDERED_ACCESS,
    //             .m_Format = LR_IMAGE_FORMAT_RGBA32F,
    //             .m_Width = pSwapChainImage->m_Width,
    //             .m_Height = pSwapChainImage->m_Height,
    //         };

    //         passData.m_pImage = resourceMan.CreateImage("gi_uav", &imageDesc);

    //         Graphics::BufferDesc bufferDesc = {
    //             .m_UsageFlags = LR_RESOURCE_USAGE_TRANSFER_DST | LR_RESOURCE_USAGE_SHADER_RESOURCE,
    //             .m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_TLSF,
    //             .m_Stride = sizeof(GeometryDataCS),
    //             .m_DataLen = kSceneDataVertexCount * sizeof(GeometryDataCS),
    //         };

    //         passData.m_pSceneDataBuffer = resourceMan.CreateBuffer("gi_scene_buffer", &bufferDesc);

    //         Graphics::DescriptorSetDesc descriptorDesc = {};
    //         descriptorDesc.m_BindingCount = 2;
    //         descriptorDesc.m_pBindings[0] = {
    //             .m_BindingID = 0,
    //             .m_Type = LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE,
    //             .m_TargetShader = LR_SHADER_STAGE_COMPUTE,
    //             .m_ArraySize = 1,
    //             .m_pImage = passData.m_pImage,
    //         };

    //         descriptorDesc.m_pBindings[1] = {
    //             .m_BindingID = 1,
    //             .m_Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_BUFFER,
    //             .m_TargetShader = LR_SHADER_STAGE_COMPUTE,
    //             .m_ArraySize = 1,
    //             .m_pBuffer = passData.m_pSceneDataBuffer,
    //         };

    //         passData.m_pDescriptorSet = resourceMan.CreateDescriptorSet(&descriptorDesc);

    //         Graphics::PushConstantDesc computeConstantDesc = {
    //             .m_Stage = LR_SHADER_STAGE_COMPUTE,
    //             .m_Offset = 0,
    //             .m_Size = sizeof(PushConstantsCS),
    //         };

    //         Graphics::Shader *pShader =
    //             resourceMan.CreateShader(LR_SHADER_STAGE_COMPUTE, LR_MAKE_APP_PATH("gi.c.hlsl"));
    //         Graphics::ComputePipelineBuildInfo computeBuildInfo = {
    //             .m_pShader = pShader,
    //             .m_DescriptorSetCount = 1,
    //             .m_ppDescriptorSets = { passData.m_pDescriptorSet },
    //             .m_PushConstantCount = 1,
    //             .m_pPushConstants = { computeConstantDesc },
    //         };

    //         passData.m_pPipeline = resourceMan.CreateComputePipeline(computeBuildInfo);

    //         resourceMan.DeleteShader(pShader);
    //     },
    //     [&](Renderer::RenderGraphResourceManager &resourceMan,
    //         GIPassData &passData,
    //         Graphics::CommandList *pList)
    //     {
    //         Graphics::BufferDesc uploadBufferDesc = {
    //             .m_UsageFlags = LR_RESOURCE_USAGE_TRANSFER_SRC | LR_RESOURCE_USAGE_HOST_VISIBLE,
    //             .m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_FRAMETIME,
    //             .m_Stride = sizeof(GeometryData),
    //             .m_DataLen = kSceneDataVertexCount * sizeof(GeometryData),
    //         };

    //         Graphics::Buffer *pUploadBuffer = resourceMan.CreateBuffer("$notrack", &uploadBufferDesc);

    //         void *pMapData = nullptr;
    //         resourceMan.MapBuffer(pUploadBuffer, pMapData);
    //         memcpy(pMapData, kSceneData, uploadBufferDesc.m_DataLen);
    //         resourceMan.UnmapBuffer(pUploadBuffer);

    //         pList->CopyBuffer(pUploadBuffer, passData.m_pSceneDataBuffer, uploadBufferDesc.m_DataLen);
    //         resourceMan.DeleteBuffer(pUploadBuffer);

    //         Graphics::PipelineBarrier barrier = {
    //             .m_CurrentUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
    //             .m_CurrentStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
    //             .m_CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
    //             .m_NextUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
    //             .m_NextStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
    //             .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ | LR_PIPELINE_ACCESS_SHADER_WRITE,
    //         };
    //         pList->SetImageBarrier(passData.m_pImage, &barrier);

    //         resourceMan.UpdateDescriptorSetData(passData.m_pDescriptorSet);
    //     },
    //     [=](Renderer::RenderGraphResourceManager &resourceMan,
    //         GIPassData &passData,
    //         Graphics::CommandList *pList)
    //     {
    //         XMUINT2 res = XMUINT2(passData.m_pImage->m_Width, passData.m_pImage->m_Height);
    //         PushConstantsCS pushConstantData = {
    //             .CameraPos = gPosition,
    //             .Width = res.x,
    //             .Height = res.y,
    //             .Time = gTime++,
    //             .TriangleCount = kSceneDataVertexCount,
    //         };

    //         pList->SetPipeline(passData.m_pPipeline);
    //         pList->SetPushConstants(LR_SHADER_STAGE_COMPUTE, 0, &pushConstantData, sizeof(PushConstantsCS));
    //         pList->SetDescriptorSets({ passData.m_pDescriptorSet });

    //         pList->Dispatch((res.x / 16) + 1, (res.y / 16) + 1, 1);
    //     },
    //     [](Renderer::RenderGraphResourceManager &resourceMan)
    //     {
    //     });

    struct GIFullscreenPassData
    {
        Graphics::Sampler *m_pSampler;
        Graphics::DescriptorSet *m_pFullScreenDescriptorSet;
        Graphics::DescriptorSet *m_pDescriptorSetSampler;
        Graphics::Pipeline *m_pPipeline;
    };

    // m_Engine.m_RenderGraph.AddPassAfter<GIFullscreenPassData, LR_COMMAND_LIST_TYPE_GRAPHICS>(
    //     "gi_fullscreen",
    //     "gi",
    //     Renderer::LR_RENDER_PASS_EXECUTION_DONT_CARE,
    //     [](Renderer::RenderGraphResourceManager &resourceMan, GIFullscreenPassData &passData)
    //     {
    //         Graphics::Image *pGIOutput = resourceMan.Get<Graphics::Image>("gi_uav");

    //         Graphics::SamplerDesc samplerDesc = {
    //             .m_MinFilter = LR_FILTERING_LINEAR,
    //             .m_MagFilter = LR_FILTERING_LINEAR,
    //             .m_MipFilter = LR_FILTERING_NEAREST,
    //             .m_AddressU = LR_TEXTURE_ADDRESS_WRAP,
    //             .m_AddressV = LR_TEXTURE_ADDRESS_WRAP,
    //             .m_MaxLOD = 0,
    //         };

    //         passData.m_pSampler = resourceMan.CreateSampler("gi_sampler", &samplerDesc);

    //         Graphics::DescriptorSetDesc descriptorDesc = {};
    //         descriptorDesc.m_BindingCount = 1;
    //         descriptorDesc.m_pBindings[0] = {
    //             .m_BindingID = 0,
    //             .m_Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE,
    //             .m_TargetShader = LR_SHADER_STAGE_PIXEL,
    //             .m_ArraySize = 1,
    //             .m_pImage = pGIOutput,
    //         };
    //         passData.m_pFullScreenDescriptorSet = resourceMan.CreateDescriptorSet(&descriptorDesc);

    //         descriptorDesc.m_pBindings[0] = {
    //             .m_BindingID = 0,
    //             .m_Type = LR_DESCRIPTOR_TYPE_SAMPLER,
    //             .m_TargetShader = LR_SHADER_STAGE_PIXEL,
    //             .m_ArraySize = 1,
    //             .m_pSampler = passData.m_pSampler,
    //         };
    //         passData.m_pDescriptorSetSampler = resourceMan.CreateDescriptorSet(&descriptorDesc);
    //         resourceMan.UpdateDescriptorSetData(passData.m_pDescriptorSetSampler);

    //         Graphics::PipelineAttachment colorAttachment = {
    //             .m_Format = resourceMan.GetSwapChainFormat(),
    //             .m_BlendEnable = false,
    //             .m_WriteMask = LR_COLOR_MASK_R | LR_COLOR_MASK_G | LR_COLOR_MASK_B | LR_COLOR_MASK_A,
    //             .m_SrcBlend = LR_BLEND_FACTOR_SRC_ALPHA,
    //             .m_DstBlend = LR_BLEND_FACTOR_INV_SRC_ALPHA,
    //             .m_SrcBlendAlpha = LR_BLEND_FACTOR_ONE,
    //             .m_DstBlendAlpha = LR_BLEND_FACTOR_SRC_ALPHA,
    //             .m_ColorBlendOp = LR_BLEND_OP_ADD,
    //             .m_AlphaBlendOp = LR_BLEND_OP_ADD,
    //         };

    //         Graphics::Shader *m_pFullscreenVS =
    //             resourceMan.CreateShader(LR_SHADER_STAGE_VERTEX, LR_MAKE_APP_PATH("fs.v.hlsl"));
    //         Graphics::Shader *m_pFullscreenPS =
    //             resourceMan.CreateShader(LR_SHADER_STAGE_PIXEL, LR_MAKE_APP_PATH("fs.p.hlsl"));

    //         Graphics::GraphicsPipelineBuildInfo graphicsBuildInfo = {
    //             .m_RenderTargetCount = 1,
    //             .m_pRenderTargets = { colorAttachment },
    //             .m_ShaderCount = 2,
    //             .m_ppShaders = { m_pFullscreenVS, m_pFullscreenPS },
    //             .m_DescriptorSetCount = 2,
    //             .m_ppDescriptorSets = { passData.m_pFullScreenDescriptorSet, passData.m_pDescriptorSetSampler },
    //             .m_PushConstantCount = 0,
    //             .m_SetFillMode = LR_FILL_MODE_FILL,
    //             .m_SetCullMode = LR_CULL_MODE_NONE,
    //             .m_EnableDepthWrite = false,
    //             .m_EnableDepthTest = false,
    //             .m_DepthCompareOp = LR_COMPARE_OP_NEVER,
    //             .m_MultiSampleBitCount = 1,
    //         };

    //         passData.m_pPipeline = resourceMan.CreateGraphicsPipeline(graphicsBuildInfo);

    //         resourceMan.DeleteShader(m_pFullscreenVS);
    //         resourceMan.DeleteShader(m_pFullscreenPS);
    //     },
    //     [](Renderer::RenderGraphResourceManager &resourceMan,
    //        GIFullscreenPassData &passData,
    //        Graphics::CommandList *pList)
    //     {
    //         Graphics::Image *pGIOutput = resourceMan.Get<Graphics::Image>("gi_uav");
    //         Graphics::PipelineBarrier barrier = {
    //             .m_CurrentUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
    //             .m_CurrentStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
    //             .m_CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ | LR_PIPELINE_ACCESS_SHADER_WRITE,
    //             .m_CurrentQueue = LR_COMMAND_LIST_TYPE_GRAPHICS,
    //             .m_NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
    //             .m_NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
    //             .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
    //             .m_NextQueue = LR_COMMAND_LIST_TYPE_COMPUTE,
    //         };
    //         pList->SetImageBarrier(pGIOutput, &barrier);
    //         resourceMan.UpdateDescriptorSetData(passData.m_pFullScreenDescriptorSet);
    //     },
    //     [&](Renderer::RenderGraphResourceManager &resourceMan,
    //         GIFullscreenPassData &passData,
    //         Graphics::CommandList *pList)
    //     {
    //         Graphics::Image *pImage = resourceMan.GetSwapChainImage();

    //         pList->SetViewport(0, 0, 0, pImage->m_Width, pImage->m_Height);
    //         pList->SetScissors(0, 0, 0, pImage->m_Width, pImage->m_Height);
    //         pList->SetGraphicsPipeline(passData.m_pPipeline);
    //         pList->SetGraphicsDescriptorSets(
    //             passData.m_pPipeline,
    //             { passData.m_pFullScreenDescriptorSet, passData.m_pDescriptorSetSampler });
    //         pList->SetPrimitiveType(LR_PRIMITIVE_TYPE_TRIANGLE_LIST);
    //         pList->Draw(3);
    //     },
    //     [](Renderer::RenderGraphResourceManager &resourceMan)
    //     {
    //     });
}

void GIApp::Shutdown()
{
    ZoneScoped;
}

void GIApp::Poll(f32 deltaTime)
{
    ZoneScoped;

    // Graphics::BaseAPI *pAPI = m_Engine.m_pAPI;
    // Graphics::Image *pImage = pAPI->GetSwapChain()->GetCurrentImage();
    // Graphics::Camera3D camera = {};

    // u32 reset = 0;

    // // ImGui::SetNextWindowPos({});
    // ImGui::SetNextWindowSize({ 1000, 130 });
    // ImGui::Begin("Camera");

    // reset |= ImGui::SliderFloat("Position X", &gPosition.x, -1000, 1000);
    // reset |= ImGui::SliderFloat("Position Y", &gPosition.y, -1000, 1000);
    // reset |= ImGui::SliderFloat("Position Z", &gPosition.z, -1000, 1000);

    // if (reset)
    // {
    //     gTime = 0;
    // }

    // ImGui::End();
}
