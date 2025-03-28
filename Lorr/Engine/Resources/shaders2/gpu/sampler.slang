implementing gpu;

[vk::binding(0, 0)]
SamplerState samplers[];

public struct Sampler<U : IUniformity = IndexUniform> {
    u32 index;

    public func get() -> SamplerState {
        return samplers[U.wrap_index(this.index)];
    }
}
