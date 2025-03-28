implementing gpu;

import std;

public interface IUniformity {
    static func wrap_index(u32 index) -> u32;
};

public struct IndexUniform : IUniformity {
    [ForceInline]
    public static func wrap_index(u32 index) -> u32 {
        return index;
    }
};

public struct IndexNonUniform : IUniformity {
    [ForceInline]
    public static func wrap_index(u32 index) -> u32 {
        return NonUniformResourceIndex(index);
    }
};

public interface ICoherency {};
public struct Coherent : ICoherency {};
public struct Incoherent : ICoherency {};

