implementing gpu;

[[vk::binding(0, 0)]]
__DynamicResource<__DynamicResourceKind.Sampler> samplers[];

public struct Sampler<U : IUniformity = Uniform> {
    u32 index;

    [[mutating]]
    public __init(u32 index) {
        this.index = index;
    }

    public static func from_index(u32 index) -> Sampler<U> {
        return { index };
    }

    public func get() -> SamplerState {
        return samplers[U.wrap_index(this.index)].as<SamplerState>();
    }
}
