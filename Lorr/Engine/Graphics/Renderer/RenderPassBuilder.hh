// Created on Friday July 14th 2023 by exdal
// Last modified on Monday July 31st 2023 by exdal

#pragma once

#include "RenderGraph.hh"
#include "RenderPass.hh"

namespace lr::Graphics
{
struct DescriptorBufferInfo
{
    DescriptorSetLayout *m_pLayout = nullptr;
    u32 m_BindingID = 0;

    u8 *m_pData = nullptr;
    u64 m_Offset = 0;
    u64 m_DataSize = 0;
};

//
// struct PassData
// {
//     Image *m_pBackBuffer;
//     Image *m_pTestImage;
// };
//
// ...Prepare()...
// {
//     data.m_pBackBuffer = renderGraph.ImageIn("backbuffer")
//
//     ... do stuff ...
//
//     data.m_pTestImage = renderGraph.CreateImage("test_image", ...)
//
//     ... do stuff ...
//
//     renderGraph.ImageOut("test_image", MemoryAccess::ShaderRead)
// }
//

template<typename _PassData>
struct GraphicsRenderPassBuilder
{
    using RenderPassCb = GraphicsRenderPassCallback<_PassData>;

    GraphicsRenderPassBuilder(RenderPassCb *pPass);

    GraphicsPipelineBuildInfo m_PipelineInfo = {};
    RenderPassCb *m_pPass = nullptr;
};

struct RenderPassBuilder
{
    RenderPassBuilder(APIContext *pContext);
    ~RenderPassBuilder();

    void InitDescriptorType(u32 binding, BufferUsage usage, DescriptorSetLayout *pLayout, u64 memSize);
    DescriptorBufferInfo *GetDescriptorBuffer(u32 binding);
    u64 GetDescriptorSize(DescriptorType type);

    void SetDescriptors(eastl::span<DescriptorGetInfo> elements);

    u64 GetResourceBufferSize();
    u64 GetSamplerBufferSize();
    void GetResourceDescriptorOffsets(u64 *pOffsetsOut, u32 *pDescriptorSizeOut);

    Buffer *CreateResourceDescriptorBuffer();
    Buffer *CreateSamplerDescriptorBuffer();

    Memory::LinearAllocator m_DescriptorAllocator = {};
    eastl::vector<DescriptorBufferInfo> m_ResourceDescriptors = {};
    DescriptorBufferInfo m_SamplerDescriptor = {};
    PipelineLayout *m_pPipelineLayout = nullptr;

    APIContext *m_pContext = nullptr;
};
}  // namespace lr::Graphics