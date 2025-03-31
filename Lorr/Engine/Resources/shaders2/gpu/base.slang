implementing gpu;

import std;

public interface IUniformity {
    static func wrap_index(u32 index) -> u32;
};

public struct Uniform : IUniformity {
    [[ForceInline]]
    public static func wrap_index(u32 index) -> u32 {
        return index;
    }
};

public struct NonUniform : IUniformity {
    [[ForceInline]]
    public static func wrap_index(u32 index) -> u32 {
        return NonUniformResourceIndex(index);
    }
};

public interface ICoherency {
    static func is_coherent() -> bool;
};

public struct Coherent : ICoherency {
    [[ForceInline]]
    public static func is_coherent() -> bool {
        return true;
    }
};

public struct Incoherent : ICoherency {
    [[ForceInline]]
    public static func is_coherent() -> bool {
        return false;
    }
}
