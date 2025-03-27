implementing std;

public namespace std {
// Can support up to SPIR-V 1.5
public enum MemoryOrder : u32 {
    Relaxed = 0x0,
    Acquire = 0x2,
    Release = 0x4,
    AcqRel  = 0x8,
    SeqCst  = 0x10,
};
public static constexpr MemoryOrder memory_order_relaxed = MemoryOrder::Relaxed;
public static constexpr MemoryOrder memory_order_acquire = MemoryOrder::Acquire;
public static constexpr MemoryOrder memory_order_release = MemoryOrder::Release;
public static constexpr MemoryOrder memory_order_acq_rel = MemoryOrder::AcqRel;
public static constexpr MemoryOrder memory_order_seq_cst = MemoryOrder::SeqCst;

public enum MemoryLocation : u32 {
    None      = 0,
    Buffer    = 0x40,
    Subgroup  = 0x80,
    Workgroup = 0x100,
    Image     = 0x800,
};

public enum MemoryScope : u32 {
    CrossDevice = 0,
    Device = 1,
    Workgroup = 2,
    Subgroup = 3,
    Invocation = 4,
    Queue = 5,
    ShaderCall = 6,
};

// Base type for atomics. Can perform very basic operations
// where it's supported without extensions.
public struct atomic<T : __BuiltinArithmeticType> {
    T value;

    [mutating]
    public __init(T v) {
        this.store(v, std::memory_order_acq_rel);
    }

    public func load(
        MemoryOrder memory_order,
        MemoryLocation location = MemoryLocation::None,
        MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        if (__isUnsignedInt<T>() || __isSignedInt<T>() || __isFloat<T>()) {
            return spirv_asm {
                OpCapability Int64Atomics;
                result:$$T = OpAtomicLoad &this.value $scope $semantics;
            };
        } else {
            spirv_asm { "<invalid atomic<T>::load>" };
        }

        return {};
    }

    public func store(
        T desired,
        MemoryOrder memory_order,
        MemoryLocation location = MemoryLocation::None,
        MemoryScope scope = MemoryScope::Queue
    ) -> void {
        const u32 semantics = memory_order | location;
        if (__isUnsignedInt<T>() || __isSignedInt<T>() || __isFloat<T>()) {
            return spirv_asm {
                OpCapability Int64Atomics;
                OpAtomicStore &this.value $scope $semantics $desired;
            };
        } else {
            spirv_asm { "<invalid atomic<T>::store>" };
        }
    }
};

// Integer extension of atomic.
__generic<T : __BuiltinIntegerType>
public extension atomic<T> {
    public func fetch_add(
        T arg,
        constexpr MemoryOrder memory_order,
        constexpr MemoryLocation location = MemoryLocation::None,
        constexpr MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        return spirv_asm {
            OpCapability Int64Atomics;
            result:$$T = OpAtomicIAdd &this.value $scope $semantics $arg;
        };
    }

    public func fetch_sub(
        T arg,
        constexpr MemoryOrder memory_order,
        constexpr MemoryLocation location = MemoryLocation::None,
        constexpr MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        return spirv_asm {
            OpCapability Int64Atomics;
            result:$$T = OpAtomicISub &this.value $scope $semantics $arg;
        };
    }

    public func fetch_max(
        T arg,
        constexpr MemoryOrder memory_order,
        constexpr MemoryLocation location = MemoryLocation::None,
        constexpr MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        if (__isUnsignedInt<T>()) {
            return spirv_asm {
                OpCapability Int64Atomics;
                result:$$T = OpAtomicUMax &this.value $scope $semantics $arg;
            };
        } else if (__isSignedInt<T>()) {
            return spirv_asm {
                OpCapability Int64Atomics;
                result:$$T = OpAtomicSMax &this.value $scope $semantics $arg;
            };
        } else {
            spirv_asm { "<invalid atomic<T>::fetch_max>" };
        }

        return {};
    }

    public func fetch_min(
        T arg,
        constexpr MemoryOrder memory_order,
        constexpr MemoryLocation location = MemoryLocation::None,
        constexpr MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        if (__isUnsignedInt<T>()) {
            return spirv_asm {
                OpCapability Int64Atomics;
                result:$$T = OpAtomicUMin &this.value $scope $semantics $arg;
            };
        } else if (__isSignedInt<T>()) {
            return spirv_asm {
                OpCapability Int64Atomics;
                result:$$T = OpAtomicSMin &this.value $scope $semantics $arg;
            };
        } else {
            spirv_asm { "<invalid atomic<T>::fetch_min>" };
        }

        return {};
    }

    public func fetch_and(
        T arg,
        constexpr MemoryOrder memory_order,
        constexpr MemoryLocation location = MemoryLocation::None,
        constexpr MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        return spirv_asm {
            OpCapability Int64Atomics;
            result:$$T = OpAtomicAnd &this.value $scope $semantics $arg;
        };
    }

    public func fetch_or(
        T arg,
        constexpr MemoryOrder memory_order,
        constexpr MemoryLocation location = MemoryLocation::None,
        constexpr MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        return spirv_asm {
            OpCapability Int64Atomics;
            result:$$T = OpAtomicOr &this.value $scope $semantics $arg;
        };
    }

    public func fetch_xor(
        T arg,
        constexpr MemoryOrder memory_order,
        constexpr MemoryLocation location = MemoryLocation::None,
        constexpr MemoryScope scope = MemoryScope::Queue
    ) -> T {
        const u32 semantics = memory_order | location;
        return spirv_asm {
            OpCapability Int64Atomics;
            result:$$T = OpAtomicOr &this.value $scope $semantics $arg;
        };
    }
};

// Barriers ─────────────────────────────────────────────────────────

// Wait for all invocations in the scope restricted tangle to reach
// the current point of execution before executing further instructions.
//
// Execution is the scope defining the scope restricted tangle
// affected by this command.
//
// https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpControlBarrier
public func control_barrier(
    constexpr MemoryOrder memory_order,
    constexpr MemoryScope scope_execution = MemoryScope::Workgroup,
    constexpr MemoryScope scope = MemoryScope::Workgroup,
    constexpr MemoryLocation location = MemoryLocation::Workgroup
) -> void {
    const u32 semantics = memory_order | location;
    spirv_asm {
        OpControlBarrier $scope_execution $scope $semantics;
    };
}

// Good old barrier.
public func memory_barrier(
    constexpr MemoryOrder memory_order,
    constexpr MemoryScope scope = MemoryScope::Queue,
    constexpr MemoryLocation location = MemoryLocation::None
) -> void {
    const u32 semantics = memory_order | location;
    spirv_asm {
        OpMemoryBarrier $scope $semantics;
    };
}

}

