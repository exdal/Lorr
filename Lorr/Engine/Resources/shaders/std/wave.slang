implementing std;

namespace std {
public func subgroup_id() -> u32 {
    return spirv_asm {
        result:$$u32 = OpLoad builtin(SubgroupId:u32);
    };
}

__generic<T : __BuiltinArithmeticType, let N : int>
public func subgroup_inclusive_add(vector<T, N> expr) -> vector<T, N> {
    return spirv_asm {
        OpCapability GroupNonUniformArithmetic;
        OpGroupNonUniformFAdd $$vector<T,N> result Subgroup InclusiveScan $expr
    };
}

public func wave_shuffle_xor<T : __BuiltinType>(T value, u32 mask) -> T {
    return spirv_asm {
        OpCapability GroupNonUniformShuffle;
        OpGroupNonUniformShuffleXor $$T result Subgroup $value $mask;
    };
}

}
