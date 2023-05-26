// Created on Saturday April 22nd 2023 by exdal
// Last modified on Friday May 26th 2023 by exdal

#include "Graphics/Renderer/Pass.hh"

#include "Engine.hh"

namespace lr::Graphics
{
void AddGeometryPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct EmptyPassData
    {
    };

    auto *pPass = pGraph->CreateGraphicsPassCb<EmptyPassData>(
        name,
        [pGraph](Context *pContext, EmptyPassData &data, RenderPassBuilder &builder)
        {
            /// STORAGE BUFFER ///
            BufferDesc bufferDesc = {
                .m_UsageFlags = BufferUsage::Storage,
                .m_TargetAllocator = ResourceAllocator::BufferLinear,
                .m_Stride = sizeof(u64),
                .m_DataLen = 256,
            };
            Buffer *pDummyBuffer = pContext->CreateBuffer(&bufferDesc);
            Buffer *pDummyBuffer2 = pContext->CreateBuffer(&bufferDesc);

            void *pMapData = nullptr;
            pContext->MapMemory(pDummyBuffer, pMapData, 0, 16);
            u64 test = 31;
            memcpy(pMapData, &test, 8);
            test = 32;
            memcpy((u8 *)pMapData + 8, &test, 8);
            pContext->UnmapMemory(pDummyBuffer);

            DescriptorGetInfo bufferInfo[] = { pDummyBuffer, pDummyBuffer2 };
            builder.SetBufferDescriptor(bufferInfo);

            auto *pVertexShaderData =
                Engine::GetResourceMan()->Get<Resource::ShaderResource>("shader://bindless_test.vs");
            auto *pPixelShaderData =
                Engine::GetResourceMan()->Get<Resource::ShaderResource>("shader://bindless_test.fs");

            Shader *pVertexShader = pContext->CreateShader(pVertexShaderData);
            Shader *pPixelShader = pContext->CreateShader(pPixelShaderData);

            builder.SetColorAttachment("$backbuffer", { AttachmentOp::Load, AttachmentOp::Store });
            builder.SetBlendAttachment({ true });
            builder.SetShader(pVertexShader);
            builder.SetShader(pPixelShader);

            LOG_TRACE("Device Address: {}", pDummyBuffer->m_DeviceAddress);
            LOG_TRACE("Device Address: {}", pDummyBuffer2->m_DeviceAddress);
        },
        [](Context *pContext, EmptyPassData &data, CommandList *pList)
        {
            pList->SetViewport(0, 0, 0, 1, 1);
            pList->SetScissors(0, 0, 0, 1, 1);
            pList->SetPrimitiveType(PrimitiveType::TriangleList);
            pList->Draw(3);
        },
        [](Context *pContext)
        {
        });
}
}  // namespace lr::Graphics