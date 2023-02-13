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

void GIApp::Init(lr::BaseApplicationDesc &desc)
{
    ZoneScoped;

    InitBase(desc);

    Graphics::BaseAPI *pAPI = m_Engine.m_pAPI;

    /// SCENE GEOMETRY PRECOMPUTATION ///

    for (u32 i = 0; i < kSceneDataVertexCount; i++)
    {
        GeometryData &v = kSceneData[i];

        XMVECTOR E1 = XMVectorSubtract(v.Positions[1], v.Positions[0]);
        XMVECTOR E2 = XMVectorSubtract(v.Positions[2], v.Positions[0]);
        v.Normal = XMVector3Normalize(XMVector3Cross(E1, E2));
    }

    /// GPU RESOURCE CREATION ///

    m_pFullscreenVS = pAPI->CreateShader(LR_SHADER_STAGE_VERTEX, LR_MAKE_APP_PATH("fs.v.hlsl"));
    m_pFullscreenPS = pAPI->CreateShader(LR_SHADER_STAGE_PIXEL, LR_MAKE_APP_PATH("fs.p.hlsl"));
    m_pGlobalIllumCS = pAPI->CreateShader(LR_SHADER_STAGE_COMPUTE, LR_MAKE_APP_PATH("gi.c.hlsl"));

    Graphics::ImageDesc imageDesc = {
        .m_UsageFlags = LR_RESOURCE_USAGE_SHADER_RESOURCE | LR_RESOURCE_USAGE_UNORDERED_ACCESS,
        .m_Format = LR_IMAGE_FORMAT_RGBA32F,
        .m_Width = pAPI->GetSwapChain()->m_Width,
        .m_Height = pAPI->GetSwapChain()->m_Height,
    };

    m_pComputeOutput = pAPI->CreateImage(&imageDesc);

    Graphics::SamplerDesc samplerDesc = {
        .m_MinFilter = LR_FILTERING_LINEAR,
        .m_MagFilter = LR_FILTERING_LINEAR,
        .m_MipFilter = LR_FILTERING_NEAREST,
        .m_AddressU = LR_TEXTURE_ADDRESS_WRAP,
        .m_AddressV = LR_TEXTURE_ADDRESS_WRAP,
        .m_MaxLOD = 0,
    };

    m_pSampler = pAPI->CreateSampler(&samplerDesc);

    Graphics::BufferDesc bufferDesc = {
        .m_UsageFlags = LR_RESOURCE_USAGE_TRANSFER_DST | LR_RESOURCE_USAGE_SHADER_RESOURCE,
        .m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_TLSF,
        .m_Stride = sizeof(GeometryDataCS),
        .m_DataLen = kSceneDataVertexCount * sizeof(GeometryDataCS),
    };

    m_pSceneDataBuffer = pAPI->CreateBuffer(&bufferDesc);

    /// RESOURCE DATA INITIALIZATION ///

    Graphics::PipelineBarrier transitionBarrier = {
        .m_CurrentUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
        .m_CurrentStage = LR_PIPELINE_STAGE_NONE,
        .m_CurrentAccess = LR_PIPELINE_ACCESS_NONE,
        .m_NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
        .m_NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
        .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
    };

    Graphics::CommandList *pList = pAPI->GetCommandList();
    pAPI->BeginCommandList(pList);
    Graphics::BufferDesc uploadBufferDesc = {
        .m_UsageFlags = LR_RESOURCE_USAGE_TRANSFER_SRC | LR_RESOURCE_USAGE_HOST_VISIBLE,
        .m_TargetAllocator = LR_API_ALLOCATOR_BUFFER_FRAMETIME,
        .m_Stride = sizeof(GeometryData),
        .m_DataLen = kSceneDataVertexCount * sizeof(GeometryData),
    };

    Graphics::Buffer *pUploadBuffer = pAPI->CreateBuffer(&uploadBufferDesc);

    void *pMapData = nullptr;
    pAPI->MapMemory(pUploadBuffer, pMapData);
    memcpy(pMapData, kSceneData, uploadBufferDesc.m_DataLen);
    pAPI->UnmapMemory(pUploadBuffer);
    pList->CopyBuffer(pUploadBuffer, m_pSceneDataBuffer, uploadBufferDesc.m_DataLen);

    pList->SetImageBarrier(m_pComputeOutput, &transitionBarrier);
    pAPI->EndCommandList(pList);
    pAPI->ExecuteCommandList(pList, true);

    pAPI->DeleteBuffer(pUploadBuffer);

    Graphics::DescriptorSetDesc descriptorDesc = {};
    descriptorDesc.m_BindingCount = 1;
    descriptorDesc.m_pBindings[0] = {
        .m_BindingID = 0,
        .m_Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_IMAGE,
        .m_TargetShader = LR_SHADER_STAGE_PIXEL,
        .m_ArraySize = 1,
        .m_pImage = m_pComputeOutput,
    };

    m_pDescriptorSetFS = pAPI->CreateDescriptorSet(&descriptorDesc);
    pAPI->UpdateDescriptorData(m_pDescriptorSetFS, &descriptorDesc);

    descriptorDesc.m_pBindings[0] = {
        .m_BindingID = 0,
        .m_Type = LR_DESCRIPTOR_TYPE_SAMPLER,
        .m_TargetShader = LR_SHADER_STAGE_PIXEL,
        .m_ArraySize = 1,
        .m_pSampler = m_pSampler,
    };

    m_pDescriptorSetSampler = pAPI->CreateDescriptorSet(&descriptorDesc);
    pAPI->UpdateDescriptorData(m_pDescriptorSetSampler, &descriptorDesc);

    transitionBarrier = {
        .m_CurrentUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
        .m_CurrentStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
        .m_CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
        .m_NextUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
        .m_NextStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
        .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
    };

    pList = pAPI->GetCommandList();
    pAPI->BeginCommandList(pList);
    pList->SetImageBarrier(m_pComputeOutput, &transitionBarrier);
    pAPI->EndCommandList(pList);
    pAPI->ExecuteCommandList(pList, true);

    descriptorDesc.m_BindingCount = 2;
    descriptorDesc.m_pBindings[0] = {
        .m_BindingID = 0,
        .m_Type = LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE,
        .m_TargetShader = LR_SHADER_STAGE_COMPUTE,
        .m_ArraySize = 1,
        .m_pImage = m_pComputeOutput,
    };

    descriptorDesc.m_pBindings[1] = {
        .m_BindingID = 1,
        .m_Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE_BUFFER,
        .m_TargetShader = LR_SHADER_STAGE_COMPUTE,
        .m_ArraySize = 1,
        .m_pBuffer = m_pSceneDataBuffer,
    };

    m_pDescriptorSetCS = pAPI->CreateDescriptorSet(&descriptorDesc);
    pAPI->UpdateDescriptorData(m_pDescriptorSetCS, &descriptorDesc);

    Graphics::PipelineAttachment colorAttachment = {
        .m_Format = pAPI->GetSwapChainImageFormat(),
        .m_BlendEnable = false,
        .m_WriteMask = LR_COLOR_MASK_R | LR_COLOR_MASK_G | LR_COLOR_MASK_B | LR_COLOR_MASK_A,
        .m_SrcBlend = LR_BLEND_FACTOR_SRC_ALPHA,
        .m_DstBlend = LR_BLEND_FACTOR_INV_SRC_ALPHA,
        .m_SrcBlendAlpha = LR_BLEND_FACTOR_ONE,
        .m_DstBlendAlpha = LR_BLEND_FACTOR_SRC_ALPHA,
        .m_ColorBlendOp = LR_BLEND_OP_ADD,
        .m_AlphaBlendOp = LR_BLEND_OP_ADD,
    };

    Graphics::GraphicsPipelineBuildInfo graphicsBuildInfo = {
        .m_RenderTargetCount = 1,
        .m_pRenderTargets = { colorAttachment },
        .m_ShaderCount = 2,
        .m_ppShaders = { m_pFullscreenVS, m_pFullscreenPS },
        .m_DescriptorSetCount = 2,
        .m_ppDescriptorSets = { m_pDescriptorSetFS, m_pDescriptorSetSampler },
        .m_PushConstantCount = 0,
        .m_SetFillMode = LR_FILL_MODE_FILL,
        .m_SetCullMode = LR_CULL_MODE_NONE,
        .m_EnableDepthWrite = false,
        .m_EnableDepthTest = false,
        .m_DepthCompareOp = LR_COMPARE_OP_NEVER,
        .m_MultiSampleBitCount = 1,
    };

    m_pGraphicsPipeline = pAPI->CreateGraphicsPipeline(&graphicsBuildInfo);

    Graphics::PushConstantDesc computeConstantDesc = {
        .m_Stage = LR_SHADER_STAGE_COMPUTE,
        .m_Offset = 0,
        .m_Size = sizeof(PushConstantsCS),
    };

    Graphics::ComputePipelineBuildInfo computeBuildInfo = {
        .m_pShader = m_pGlobalIllumCS,
        .m_DescriptorSetCount = 1,
        .m_ppDescriptorSets = { m_pDescriptorSetCS },
        .m_PushConstantCount = 1,
        .m_pPushConstants = { computeConstantDesc },
    };

    m_pComputePipeline = pAPI->CreateComputePipeline(&computeBuildInfo);
}

void GIApp::Shutdown()
{
    ZoneScoped;
}

void GIApp::Poll(f32 deltaTime)
{
    ZoneScoped;

    Graphics::BaseAPI *pAPI = m_Engine.m_pAPI;
    Graphics::Image *pImage = pAPI->GetSwapChain()->GetCurrentImage();
    Graphics::Camera3D camera = {};

    u32 reset = 0;
    static u32 time = 1;

    // ImGui::SetNextWindowPos({});
    ImGui::SetNextWindowSize({ 1000, 130 });
    ImGui::Begin("Camera");

    XMFLOAT3 position = {};
    XMStoreFloat3(&position, camera.m_Position);

    reset |= ImGui::SliderFloat("Position X", &position.x, -1000, 1000);
    reset |= ImGui::SliderFloat("Position Y", &position.y, -1000, 1000);
    reset |= ImGui::SliderFloat("Position Z", &position.z, -1000, 1000);
    camera.SetPosition(position);

    if (reset)
    {
        time = 0;
    }

    ImGui::End();

    Graphics::CommandList *pList = pAPI->GetCommandList(Graphics::CommandListType::Compute);
    pAPI->BeginCommandList(pList);

    PushConstantsCS pushConstantData = {
        .Width = pImage->m_Width,
        .Height = pImage->m_Height,
        .Time = time++,
        .TriangleCount = kSceneDataVertexCount,
    };

    XMStoreFloat3(&pushConstantData.CameraPos, camera.m_Position);

    pList->SetComputePipeline(m_pComputePipeline);
    pList->SetComputePushConstants(m_pComputePipeline, &pushConstantData, sizeof(PushConstantsCS));
    pList->SetComputeDescriptorSets({ m_pDescriptorSetCS });
    pList->Dispatch((pImage->m_Width / 16) + 1, (pImage->m_Height / 16) + 1, 1);

    pAPI->EndCommandList(pList);
    pAPI->ExecuteCommandList(pList, true);

    pList = pAPI->GetCommandList();
    pAPI->BeginCommandList(pList);

    Graphics::PipelineBarrier transitionBarrier = {
        .m_CurrentUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
        .m_CurrentStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
        .m_CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
        .m_NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
        .m_NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
        .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
    };

    pList->SetImageBarrier(m_pComputeOutput, &transitionBarrier);

    Graphics::CommandListAttachment attachment = {};
    attachment.m_pHandle = pImage;
    attachment.m_LoadOp = LR_ATTACHMENT_OP_CLEAR;
    attachment.m_StoreOp = LR_ATTACHMENT_OP_STORE;
    attachment.m_ClearVal = Graphics::ClearValue({ 0.0, 0.0, 0.0, 1.0 });

    Graphics::CommandListBeginDesc beginDesc = {};
    beginDesc.m_RenderArea = { 0, 0, (u32)pImage->m_Width, (u32)pImage->m_Height };
    beginDesc.m_ColorAttachmentCount = 1;
    beginDesc.m_pColorAttachments[0] = attachment;
    pList->BeginPass(&beginDesc);

    pList->SetGraphicsPipeline(m_pGraphicsPipeline);
    pList->SetViewport(0, 0, 0, pImage->m_Width, pImage->m_Height);
    pList->SetScissors(0, 0, 0, pImage->m_Width, pImage->m_Height);
    pList->SetGraphicsDescriptorSets({ m_pDescriptorSetFS, m_pDescriptorSetSampler });
    pList->SetPrimitiveType(LR_PRIMITIVE_TYPE_TRIANGLE_LIST);
    pList->Draw(3);

    pList->EndPass();

    transitionBarrier = {
        .m_CurrentUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
        .m_CurrentStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
        .m_CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
        .m_NextUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
        .m_NextStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
        .m_NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
    };

    pList->SetImageBarrier(m_pComputeOutput, &transitionBarrier);

    pAPI->EndCommandList(pList);
    pAPI->ExecuteCommandList(pList, false);
}
