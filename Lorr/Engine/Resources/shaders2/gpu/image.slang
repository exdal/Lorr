implementing gpu;

import std;
import glm;

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
public struct Image<T : ITexelElement, S : __ITextureShape, U : IUniformity = IndexUniform> {
    public typealias parent_type = _Texture<T, S, 0, 0, 0, 0, 0, 0, 0>;
    public typealias this_type = Image<T, S, U>;

    u32 index;

    [mutating]
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

    public func sample_grad<SU : IUniformity>(Sampler<SU> sampler, vector<f32, S.dimensions> tex_coords, f32 ddx, f32 ddy) -> T {
        return this.get().SampleGrad(sampler.get(), tex_coords, ddx, ddy);
    }
};

// Image1D ──────────────────────────────────────────────────────────
__generic<T : ITexelElement, U : IUniformity = IndexUniform>
public typealias Image1D = Image<T, __Shape1D, U>;

__generic<T : ITexelElement, U : IUniformity>
public extension Image1D<T, U> {
    public func load(u32 loc, u32 mip = 0) -> T {
        this.get().Load(glm::i32vec2(loc, mip));
    }
};

// Image2D ──────────────────────────────────────────────────────────
__generic<T : ITexelElement, U : IUniformity = IndexUniform>
public typealias Image2D = Image<T, __Shape2D, U>;

__generic<T : ITexelElement, U : IUniformity>
public extension Image2D<T, U> {
    public func load(glm::u32vec2 loc, u32 mip = 0) -> T {
        this.get().Load(glm::i32vec3(loc, mip));
    }
};

// Image3D ──────────────────────────────────────────────────────────
__generic<T : ITexelElement, U : IUniformity = IndexUniform>
public typealias Image3D = Image<T, __Shape3D, U>;

__generic<T : ITexelElement, U : IUniformity>
public extension Image3D<T, U> {
    public func load(glm::u32vec3 loc, u32 mip = 0) -> T {
        this.get().Load(glm::i32vec4(loc, mip));
    }
};

// StorageImage ────────────────────────────────────────────────────────────
public struct StorageImage<T : ITexelElement, S : __ITextureShape, C : ICoherency, U : IUniformity = IndexUniform> {
    public typealias parent_type = _Texture<T, S, 0, 0, 0, 1, 0, 0, 0>;
    public typealias this_type = Image<T, S, U>;

    u32 index;

    [mutating]
    public __init(u32 index) {
        this.index = index;
    }

    public func get_index() -> u32 {
        return this.index;
    }

    func get() -> parent_type {
        return textures[U.wrap_index(this.index)].as<parent_type>();
    }
};

// Incoherent StorageImage
__generic<T : ITexelElement, S : __ITextureShape, C : ICoherency = Incoherent, U : IUniformity>
public extension StorageImage<T, S, C, U> {
    public func load(
        vector<u32, S.dimensions> loc,
        constexpr std::MemoryScope scope = std::MemoryScope::Queue
    ) -> T {
        const let self = this.get();
        const u32 operand = ImageOperand::MakeTexelVisible | ImageOperand::NonPrivateTexel;
        return spirv_asm {
            OpCapability VulkanMemoryModel;
            result:$$T = OpImageRead $self $loc $operand $scope;
        };
    }

    public func store(
        vector<u32, S.dimensions> loc,
        T value,
        constexpr std::MemoryScope scope = std::MemoryScope::Queue
    ) -> void {
        const let self = this.get();
        const u32 operand = ImageOperand::MakeTexelAvailable | ImageOperand::NonPrivateTexel;
        spirv_asm {
            OpCapability VulkanMemoryModel;
            OpImageWrite $self $loc $value $operand $scope;
        };
    }
};

// Coherent StorageImage
__generic<T : ITexelElement, S : __ITextureShape, C : ICoherency = Coherent, U : IUniformity>
public extension StorageImage<T, S, C, U> {
    public func load(vector<u32, S.dimensions> loc) -> T {
        return this.get().Load(loc);
    }

    public func store(vector<u32, S.dimensions> loc, T value) -> void {
        this.get().Store(loc, value);
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

