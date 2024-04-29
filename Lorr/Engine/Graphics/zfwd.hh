#pragma once

namespace lr::graphics {
// CommandList.hh
struct Semaphore;
struct CommandQueue;
struct CommandAllocator;
struct CommandList;
struct CommandBatcher;

// Descriptor.hh
struct DescriptorPool;
struct DescriptorSetLayout;
struct DescriptorSet;

// Device.hh
struct Device;

// Instance.hh
struct Instance;

// Pipeline.hh
struct PipelineLayout;
struct Pipeline;

// Resource.hh
struct Buffer;
struct Image;
struct ImageView;
struct Sampler;

// Shader.hh
struct Shader;

// SwapChain.hh
struct SwapChain;

LR_HANDLE(BufferID, u32);
LR_HANDLE(ImageID, u32);
LR_HANDLE(ImageViewID, u32);
LR_HANDLE(SamplerID, u32);
LR_HANDLE(ShaderID, u32);
LR_HANDLE(PipelineID, u32);

template<typename T>
struct Unique;

template<typename T>
struct UniqueResult;

}  // namespace lr::graphics
