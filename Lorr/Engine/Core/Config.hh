// Created on Saturday May 13th 2023 by exdal
// Last modified on Friday September 15th 2023 by exdal

#pragma once

#include "Crypt/CRC.hh"
#include "Memory/MemoryUtils.hh"

#define _CONFIG_VAR_LIST                                                     \
    CONFIG_DEFINE_VAR(eastl::string, RESOURCE_META_DIR, ("."));              \
    CONFIG_DEFINE_VAR(eastl::string, RESOURCE_META_FILE, ("resmeta.lm"));    \
    CONFIG_DEFINE_VAR(eastl::string, RESOURCE_META_COMPILED_DIR, ("meta0")); \
    CONFIG_DEFINE_VAR(u32, RM_MAX_ALLOCS, (0x20000));                        \
    CONFIG_DEFINE_VAR(u32, RM_MAX_MEMORY, (1024));                           \
    CONFIG_DEFINE_VAR(u32, GPM_MAX_TLSF_ALLOCS, (0x20000));                  \
    CONFIG_DEFINE_VAR(u32, GPM_DESCRIPTOR, (Memory::MiBToBytes(8)));         \
    CONFIG_DEFINE_VAR(u32, GPM_BUFFER_LINEAR, (Memory::MiBToBytes(128)));    \
    CONFIG_DEFINE_VAR(u32, GPM_BUFFER_TLSF, (Memory::MiBToBytes(1024)));     \
    CONFIG_DEFINE_VAR(u32, GPM_BUFFER_TLSF_HOST, (Memory::MiBToBytes(128))); \
    CONFIG_DEFINE_VAR(u32, GPM_FRAMETIME, (Memory::MiBToBytes(32)));         \
    CONFIG_DEFINE_VAR(u32, GPM_IMAGE_TLSF, (Memory::MiBToBytes(1024)));      \
    CONFIG_DEFINE_VAR(u32, API_SWAPCHAIN_FRAMES, (3));                       \
    CONFIG_DEFINE_VAR(u32, JM_WORKER_COUNT, (4));

#define CONFIG_DEFINE_VAR(type, name, defVal) \
    ConfigVar<CRC32HashOf(#name), type> cfg_##name = ConfigVar<CRC32HashOf(#name), type>(defVal)

#define CONFIG_GET_VAR(name) Config::Get().cfg_##name.Val

namespace lr
{
// A simple parser for config files
// Simple usage:
// # This is how comments work
// THIS_IS_A_FLAG # This comment will also work
// THIS_IS_A_VAL_WITH_NUMBER 2
// THIS_IS_A_VAL_WITH_STRING "This will def. \"work\""
struct Config
{
    static Config &Get();
    static bool Init();

    bool ParseLine(eastl::string_view line);

    template<u64 _Hash, typename _Type>
    struct ConfigVar
    {
        ConfigVar(const _Type &v)
            : Val(v){};
        static constexpr u64 Hash = _Hash;
        _Type Val;
    };

    _CONFIG_VAR_LIST
};
}  // namespace lr

#undef CONFIG_DEFINE_VAR