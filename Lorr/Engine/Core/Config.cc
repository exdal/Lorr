// Created on Saturday May 13th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal

#include "Config.hh"
#include "LM/Parser.hh"

#define CONFIG_ASSIGN_VAL(type, name)                                        \
    {                                                                        \
        lm::VariableData *pData = category.FindVariable(FNV64HashOf(#name)); \
        if (pData)                                                           \
            Config::Get().name.Val = pData->As<type>();                      \
    }

namespace lr
{
Config &Config::Get()
{
    static Config g_Config;
    return g_Config;
}

bool Config::Init()
{
    ZoneScoped;

    lm::Result result = {};
    lm::Init(&result);
    if (!lm::ParseFromFile(&result, "config.lm"))
    {
        LOG_CRITICAL("Config file not found!");
        return false;
    }

    lm::Category &category = result.m_GlobalCategory;

    CONFIG_ASSIGN_VAL(char *, resource_meta_dir);
    CONFIG_ASSIGN_VAL(char *, resource_meta_file);
    CONFIG_ASSIGN_VAL(char *, resource_meta_compiled_dir);
    CONFIG_ASSIGN_VAL(char *, resource_meta_compiled_file);
    
    CONFIG_ASSIGN_VAL(u32, gpm_tlsf_allocations);
    CONFIG_ASSIGN_VAL(u32, gpm_descriptor);
    CONFIG_ASSIGN_VAL(u32, gpm_buffer_linear);
    CONFIG_ASSIGN_VAL(u32, gpm_buffer_tlsf);
    CONFIG_ASSIGN_VAL(u32, gpm_buffer_tlsf_host);
    CONFIG_ASSIGN_VAL(u32, gpm_frametime);
    CONFIG_ASSIGN_VAL(u32, gpm_image_tlsf);
    
    CONFIG_ASSIGN_VAL(u32, api_swapchain_frames);

    CONFIG_ASSIGN_VAL(u32, jm_worker_count);

    return true;
}
}  // namespace lr