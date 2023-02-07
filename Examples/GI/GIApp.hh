// 
// Created on Thursday 26th January 2023 by e-erdal
// 

#pragma once

#include "Lorr/Engine/Application/Application.hh"

#include "Lorr/Engine/Core/Graphics/RHI/Base/BaseAPI.hh"

using namespace lr;

struct GIApp : Application
{
    void Init(BaseApplicationDesc &desc);
    void Shutdown() override;
    void Poll(f32 deltaTime) override;

    Graphics::Pipeline *m_pGraphicsPipeline = nullptr;
    Graphics::Pipeline *m_pComputePipeline = nullptr;

    Graphics::Shader *m_pFullscreenVS = nullptr;
    Graphics::Shader *m_pFullscreenPS = nullptr;
    Graphics::Shader *m_pGlobalIllumCS = nullptr;
    
    Graphics::Image *m_pComputeOutput = nullptr;
    Graphics::Sampler *m_pSampler = nullptr;
    Graphics::Buffer *m_pSceneDataBuffer = nullptr;

    Graphics::DescriptorSet *m_pDescriptorSetFS = nullptr;
    Graphics::DescriptorSet *m_pDescriptorSetCS = nullptr;
    Graphics::DescriptorSet *m_pDescriptorSetSampler = nullptr;
};

#define LR_APP_WORKING_DIR "../../Examples/GI/"
#define LR_MAKE_APP_PATH(path) LR_APP_WORKING_DIR path