// Created on Saturday May 13th 2023 by exdal
// Last modified on Tuesday May 16th 2023 by exdal

#pragma once

#include "Crypt/FNV.hh"

#include "Graphics/SwapChain.hh"

#include "Memory/MemoryUtils.hh"

#define CONFIG_GET_VAR(name) Config::Get().name.Val
#define CONFIG_DEFINE_VAR(type, name) ConfigVar<FNV64HashOf(#name), type> name
#define CONFIG_DEFINE_VAR_DEFAULT(type, name, defVal) \
    ConfigVar<FNV64HashOf(#name), type> name = ConfigVar<FNV64HashOf(#name), type>(defVal)

namespace lr
{
struct Config
{
    static Config &Get();
    static bool Init();

    template<u64 _Hash, typename _Type>
    struct ConfigVar
    {
        ConfigVar(const _Type &v)
            : Val(v){};
        static constexpr u64 Hash = _Hash;
        _Type Val;
    };

    CONFIG_DEFINE_VAR_DEFAULT(eastl::string_view, resource_meta_dir, (""));
    CONFIG_DEFINE_VAR_DEFAULT(eastl::string_view, resource_meta_file, ("resmeta.lm"));
    CONFIG_DEFINE_VAR_DEFAULT(eastl::string_view, resource_meta_compiled_dir, (""));
    CONFIG_DEFINE_VAR_DEFAULT(eastl::string_view, resource_meta_compiled_file, ("meta0"));

    CONFIG_DEFINE_VAR_DEFAULT(u32, rm_max_allocs, (0x20000));
    CONFIG_DEFINE_VAR_DEFAULT(u32, rm_memory_mb, (1024));

    CONFIG_DEFINE_VAR_DEFAULT(u32, gpm_tlsf_allocations, (0x20000));
    CONFIG_DEFINE_VAR_DEFAULT(u32, gpm_descriptor, (Memory::MiBToBytes(16)));
    CONFIG_DEFINE_VAR_DEFAULT(u32, gpm_buffer_linear, (Memory::MiBToBytes(128)));
    CONFIG_DEFINE_VAR_DEFAULT(u32, gpm_buffer_tlsf, (Memory::MiBToBytes(1024)));
    CONFIG_DEFINE_VAR_DEFAULT(u32, gpm_buffer_tlsf_host, (Memory::MiBToBytes(128)));
    CONFIG_DEFINE_VAR_DEFAULT(u32, gpm_frametime, (Memory::MiBToBytes(128)));
    CONFIG_DEFINE_VAR_DEFAULT(u32, gpm_image_tlsf, (Memory::MiBToBytes(1024)));

    CONFIG_DEFINE_VAR_DEFAULT(u32, api_swapchain_frames, (3));
};
}  // namespace lr

#undef CONFIG_DEFINE_VAR_DEFAULT
#undef CONFIG_DEFINE_VAR