// Created on Saturday April 22nd 2023 by exdal
// Last modified on Monday June 26th 2023 by exdal

#include "Graphics/Renderer/Pass.hh"

#include "Engine.hh"

namespace lr::Graphics
{
void AddGeometryPass(RenderGraph *pGraph, eastl::string_view name)
{
    struct GeometryPassData
    {
        Buffer *m_pBuffer;
    };

    auto *pPass = pGraph->CreateGraphicsPassCb<GeometryPassData>(
        name,
        [pGraph](Context *pContext, GeometryPassData &data, RenderPassBuilder &builder)
        {
            /// STORAGE BUFFER ///
            BufferDesc bufferDesc = {
                .m_UsageFlags = BufferUsage::Storage,
                .m_TargetAllocator = ResourceAllocator::BufferLinear,
                .m_Stride = sizeof(u32),
                .m_DataSize = 256,
            };
            Buffer *pDummyBuffer = data.m_pBuffer = pContext->CreateBuffer(&bufferDesc);

            void *pMapData = nullptr;
            pContext->MapBuffer(pDummyBuffer, pMapData, 0, 8);
            u32 test = 31;
            memcpy(pMapData, &test, 4);
            test = 32;
            memcpy((u8 *)pMapData + 4, &test, 4);
            pContext->UnmapBuffer(pDummyBuffer);
            pContext->SetObjectName(pDummyBuffer, "DummyBuffer");

            ImageDesc imageDesc = {
                .m_UsageFlags = ImageUsage::Sampled | ImageUsage::TransferDst,
                .m_Format = ImageFormat::RGBA8F,
                .m_TargetAllocator = ResourceAllocator::ImageTLSF,
                .m_Width = 128,
                .m_Height = 128,
                .m_ArraySize = 1,
                .m_MipMapLevels = 1,
            };
            Image *pImage = pGraph->CreateImage("bindless_test", imageDesc);

            DescriptorGetInfo bufferInfo(pDummyBuffer);
            builder.SetDescriptor(DescriptorType::UniformBuffer, bufferInfo);
            DescriptorGetInfo imageInfo(pImage);
            builder.SetDescriptor(DescriptorType::SampledImage, imageInfo);

            auto *pVertexShaderData =
                Engine::GetResourceMan()->Get<Resource::ShaderResource>("shader://bindless_test.vs");
            auto *pPixelShaderData =
                Engine::GetResourceMan()->Get<Resource::ShaderResource>("shader://bindless_test.fs");

            Shader *pVertexShader = pContext->CreateShader(pVertexShaderData);
            Shader *pPixelShader = pContext->CreateShader(pPixelShaderData);

            Engine::GetResourceMan()->Delete<Resource::ShaderResource>("shader://bindless_test.vs");
            Engine::GetResourceMan()->Delete<Resource::ShaderResource>("shader://bindless_test.fs");

            builder.SetColorAttachment("$backbuffer", { AttachmentOp::Load, AttachmentOp::Store });
            builder.SetBlendAttachment({ true });
            builder.SetShader(pVertexShader);
            builder.SetShader(pPixelShader);
        },
        [](Context *pContext, GeometryPassData &data, CommandList *pList)
        {
            // pList->SetViewport(0, 0, 0, 1, 1);
            // pList->SetScissors(0, 0, 0, 1, 1);
            // pList->SetPrimitiveType(PrimitiveType::TriangleList);
            // pList->SetBindlessLayout({ { 0, data.m_pBuffer } });
            // pList->Draw(3);
        },
        [](Context *pContext)
        {
        });
}
}  // namespace lr::Graphics