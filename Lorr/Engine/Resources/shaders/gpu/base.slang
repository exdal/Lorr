implementing gpu;

import std;

public struct DispatchIndirectCommand {
    public u32 x;
    public u32 y;
    public u32 z;
};

public struct DrawIndexedIndirectCommand {
    public u32 index_count;
    public u32 instance_count;
    public u32 first_index;
    public i32 vertex_offset;
    public u32 first_instance;
};

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
