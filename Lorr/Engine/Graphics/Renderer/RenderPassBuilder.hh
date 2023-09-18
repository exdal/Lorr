// Created on Friday July 14th 2023 by exdal
// Last modified on Wednesday August 9th 2023 by exdal

#pragma once

#include "RenderGraph.hh"
#include "RenderPass.hh"

namespace lr::Graphics
{
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

    GraphicsRenderPassBuilder(RenderPassCb *pPass) {}

    GraphicsPipelineBuildInfo m_PipelineInfo = {};
    RenderPassCb *m_pPass = nullptr;
};

// Note: Please also set the indexes of enum to help debugging.
enum class DescriptorSetIndex : u32
{
    Buffer = 0,
    Image = 1,
    StorageImage = 2,
    Sampler = 3,
};

struct DescriptorBuilder
{
    enum class DescriptorInfoType
    {
        Resource,
        Sampler,
        BDA,
    };

    struct DescriptorInfo
    {
        DescriptorInfoType m_Type = DescriptorInfoType::Resource;
        DescriptorSetIndex m_Set = DescriptorSetIndex::Buffer;
        Memory::LinearAllocator m_Allocator = {};
    };

    DescriptorBuilder(APIContext *pContext);
    ~DescriptorBuilder();

    DescriptorInfo *GetDescriptorInfo(DescriptorSetIndex set);
    u64 GetDescriptorSize(DescriptorType type);

    void SetDescriptors(eastl::span<DescriptorGetInfo> elements);

    /// Returns size of descriptor buffer, not the actual buffer. Ex: BDA returns size of
    /// stroage buffer
    u64 GetBufferSize(DescriptorInfoType type);
    /// Returns offset of descriptor buffer, not the actual buffer. Ex: BDA returns size
    /// of stroage buffer
    void GetDescriptorOffsets(
        DescriptorInfoType type, u64 *pOffsetsOut, u32 *pDescriptorSizeOut);

    eastl::vector<DescriptorInfo> m_DescriptorInfos = {};

    PipelineLayout *m_pPipelineLayout = nullptr;
    APIContext *m_pContext = nullptr;
};

//         // ------------------------ //
// al(u64) //      BDA Mem    (F0)     // Array of u64's
//         // - - - - - - - - - - - -  //
// al(u64) //      BDA Mem    (F1)     //
//         // - - - - - - - - - - - -  //
// al(u64) //      BDA Mem    (Fn)     //
//         // ------------------------ //
// al(256) // BDA Desc. |              //
//         //    Resource Mem (F0)     // Address of BDA(Fn) + other resources
//         //                          //
//         // - - - - - - - - - - - -  //
// al(256) // BDA Desc. |              //
//         //    Resource Mem (F1)     //
//         //                          //
//         // - - - - - - - - - - - -  //
// al(256) // BDA Desc. |              //
//         //    Resource Mem (Fn)     //
//         //                          //
//         // ------------------------ //
// al(256) //    Sampler Mem  (F0)     // Samplers
//         // - - - - - - - - - - - -  //
// al(256) //    Sampler Mem  (F1)     //
//         // - - - - - - - - - - - -  //
// al(256) //    Sampler Mem  (Fn)     //
//         // ------------------------ //

}  // namespace lr::Graphics