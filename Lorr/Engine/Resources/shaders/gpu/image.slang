implementing gpu;

import std;

[[vk::binding(1, 0)]]
__DynamicResource textures[];
[[vk::binding(2, 0)]]
__DynamicResource storage_textures[];

public enum ImageOperand : u32 {
    None = 0x0,
    Bias = 0x1,
    Lod  = 0x2,
    Grad = 0x4,
    ConstOffset = 0x8,
    Offset = 0x10,
    ConstOffsets = 0x20,
    MinLod = 0x80,

    // SPIR-V 1.5 - SPV_KHR_vulkan_memory_model
    MakeTexelAvailable = 0x100,  // Requires NonPrivateTexel to also be set.
    MakeTexelVisible = 0x200,    // Requires NonPrivateTexel to also be set.
    NonPrivateTexel = 0x400,
    VolatileTexel = 0x800,
};

// Image ────────────────────────────────────────────────────────────
public struct Image<T : ITexelElement, S : __ITextureShape, U : IUniformity = Uniform> {
    public typealias parent_type = _Texture<T, S, 0, 0, 0, 0, 0, 0, 0>;
    public typealias this_type = Image<T, S, U>;

    u32 index = ~0u;

    [[mutating]]
    __init(u32 index) {
        this.index = index;
    }

    public static func from_index(u32 index) -> this_type {
        return { index };
    }

    public func get_index() -> u32 {
        return this.index;
    }

    func get() -> parent_type {
        return textures[U.wrap_index(this.index)].as<parent_type>();
    }

    public func sample<SU : IUniformity>(Sampler<SU> sampler, vector<f32, S.dimensions> tex_coords) -> T {
        return this.get().Sample(sampler.get(), tex_coords);
    }

    public func sample_mip<SU : IUniformity>(Sampler<SU> sampler, vector<f32, S.dimensions> tex_coords, f32 mip) -> T {
        return this.get().SampleLevel(sampler.get(), tex_coords, mip);
    }

    public func sample_grad<SU : IUniformity>(
        Sampler<SU> sampler,
        vector<f32, S.dimensions> tex_coords,
        vector<f32, S.dimensions> ddx,
        vector<f32, S.dimensions> ddy
    ) -> T {
        return this.get().SampleGrad(sampler.get(), tex_coords, ddx, ddy);
    }

    public func is_valid() -> bool {
        return this.index != ~0u;
    }
};

// Image1D ──────────────────────────────────────────────────────────
__generic<T : ITexelElement, U : IUniformity = Uniform>
public typealias Image1D = Image<T, __Shape1D, U>;

__generic<T : ITexelElement, U : IUniformity>
public extension Image1D<T, U> {
    public func load(u32 texel, u32 mip = 0) -> T {
        return this.get().Load(i32x2(texel, mip));
    }
};

// Image2D ──────────────────────────────────────────────────────────
__generic<T : ITexelElement, U : IUniformity = Uniform>
public typealias Image2D = Image<T, __Shape2D, U>;

public extension<T : ITexelElement, U : IUniformity> Image2D<T, U> {
    public func load(u32x2 texel, u32 mip = 0) -> T {
        return this.get().Load(i32x3(texel, mip));
    }
};

// Image3D ──────────────────────────────────────────────────────────
__generic<T : ITexelElement, U : IUniformity = Uniform>
public typealias Image3D = Image<T, __Shape3D, U>;

public extension<T : ITexelElement, U : IUniformity> Image3D<T, U> {
    public func load(u32x3 texel, u32 mip = 0) -> T {
        return this.get().Load(i32x4(texel, mip));
    }
};

// StorageImage ────────────────────────────────────────────────────────────
public struct StorageImage<T : ITexelElement, S : __ITextureShape, C : ICoherency, U : IUniformity = Uniform> {
    public typealias parent_type = _Texture<T, S, 0, 0, 0, 1, 0, 0, 0>;
    public typealias this_type = StorageImage<T, S, C, U>;

    u32 index = ~0u;

    [[mutating]]
    __init(u32 index) {
        this.index = index;
    }

    public static func from_index(u32 index) -> this_type {
        return { index };
    }

    public func get_index() -> u32 {
        return this.index;
    }

    func get() -> parent_type {
        return textures[U.wrap_index(this.index)].as<parent_type>();
    }

    public func load(
        vector<u32, S.dimensions> texel,
        constexpr std::MemoryScope scope = std::MemoryScope::Queue
    ) -> T {
        if (!C.is_coherent()) {
            const let self = this.get();
            return spirv_asm {
                OpCapability VulkanMemoryModel;
                result:$$T = OpImageRead $self $texel NonPrivateTexel | MakeTexelVisible $scope;
            };
        } else {
            return this.get()[texel];
        }
    }

    public func store(
        vector<u32, S.dimensions> texel,
        T value,
        constexpr std::MemoryScope scope = std::MemoryScope::Queue
    ) -> void {
        if (!C.is_coherent()) {
            const let self = this.get();
            spirv_asm {
                OpCapability VulkanMemoryModel;
                OpImageWrite $self $texel $value NonPrivateTexel | MakeTexelAvailable $scope;
            };
        } else {
            this.get().Store(texel, value);
        }
    }

    public func is_valid() -> bool {
        return this.index != ~0u;
    }
};

public extension<T : ITexelElement & IInteger, S : __ITextureShape, C : ICoherency, U : IUniformity> StorageImage<T, S, C, U> {
    public func atomic_add(
        vector<u32, S.dimensions> texel,
        T value,
        constexpr std::MemoryOrder memory_order,
        constexpr std::MemoryLocation location = std::MemoryLocation::None,
        constexpr std::MemoryScope scope = std::MemoryScope::Queue
    ) -> T {
        return std::atomic_add(this.get()[texel], value, memory_order, location, scope);
    }

    public func atomic_increment(
        vector<u32, S.dimensions> texel,
        constexpr std::MemoryOrder memory_order,
        constexpr std::MemoryLocation location = std::MemoryLocation::None,
        constexpr std::MemoryScope scope = std::MemoryScope::Queue
    ) -> T {
        return std::atomic_increment(this.get()[texel], memory_order, location, scope);
    }

    public func atomic_max(
        vector<u32, S.dimensions> texel,
        T value,
        constexpr std::MemoryOrder memory_order,
        constexpr std::MemoryLocation location = std::MemoryLocation::None,
        constexpr std::MemoryScope scope = std::MemoryScope::Queue
    ) -> T {
        return std::atomic_max(this.get()[texel], value, memory_order, location, scope);
    }
};

// StorageImage1D ───────────────────────────────────────────────────
__generic<T : ITexelElement>
public typealias StorageImage1D = StorageImage<T, __Shape1D, Incoherent>;

// StorageImage2D ───────────────────────────────────────────────────
__generic<T : ITexelElement>
public typealias StorageImage2D = StorageImage<T, __Shape2D, Incoherent>;

// StorageImage3D ───────────────────────────────────────────────────
__generic<T : ITexelElement>
public typealias StorageImage3D = StorageImage<T, __Shape3D, Incoherent>;

