#pragma once

namespace lr {
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

enum class BufferID : u32 { Invalid = ~0_u32 };
enum class ImageID : u32 { Invalid = ~0_u32 };
enum class ImageViewID : u32 { Invalid = ~0_u32 };
enum class SamplerID : u32 { Invalid = ~0_u32 };
enum class SamplerHash : u64 { Invalid = ~0_u64 };
enum class ShaderID : u32 { Invalid = ~0_u32 };
enum class PipelineLayoutID : usize { None = 0 };
enum class PipelineID : u32 { Invalid = ~0_u32 };
enum class PipelineHash : u64 { Invalid = ~0_u64 };

}  // namespace lr
