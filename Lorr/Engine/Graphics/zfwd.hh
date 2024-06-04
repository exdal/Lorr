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
LR_HANDLE(SamplerHash, u64);
LR_HANDLE(ShaderID, u32);
enum class PipelineLayoutID : usize { None = 0 };
LR_HANDLE(PipelineID, u32);
LR_HANDLE(PipelineHash, u64);

}  // namespace lr::graphics
