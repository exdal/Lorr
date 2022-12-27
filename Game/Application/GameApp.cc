#include "GameApp.hh"

#include <imgui_internal.h>

using namespace lr::Graphics;

BaseImage *pImage = nullptr;
BaseDescriptorSet *pSet = nullptr;
BaseDescriptorSet *pComputeSet = nullptr;

void GameApp::Init(lr::ApplicationDesc &desc)
{
    ZoneScoped;

    m_Title = desc.Title;

    m_Engine.Init(desc.Engine);

    /// STYLE INITIALIZATION ///
    ImGuiStyle &style = ImGui::GetStyle();

    /// Shape
    style.FramePadding = ImVec2(6, 4);

    style.WindowRounding = 0;
    style.ScrollbarRounding = 0;
    style.FrameRounding = 0;
    style.GrabRounding = 0;
    style.PopupRounding = 0;
    style.TabRounding = 0;
    style.WindowTitleAlign = ImVec2(0, 0);
    style.FrameBorderSize = 0;
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
}

void GameApp::Shutdown()
{
    ZoneScoped;
}

void GameApp::Poll(f32 deltaTime)
{
    ZoneScoped;

    BaseAPI *pAPI = m_Engine.m_RendererMan.m_pAPI;

    constexpr ImGuiWindowFlags kDockWindowStyle = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                                                  | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
                                                  | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    constexpr ImGuiWindowFlags kWindowStyle = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                                              | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

    ImGuiIO &io = ImGui::GetIO();
    ImGuiViewport *pViewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(pViewport->WorkPos);
    ImGui::SetNextWindowSize(pViewport->WorkSize);
    ImGui::SetNextWindowViewport(pViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("DockSpace", nullptr, kDockWindowStyle);
    {
        ImGui::PopStyleVar();

        u32 dockID = ImGui::GetID("dockSpace");
        if (!ImGui::DockBuilderGetNode(dockID))
        {
            ImGui::DockBuilderRemoveNode(dockID);
            ImGui::DockBuilderAddNode(dockID, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockID, pViewport->WorkSize);

            u32 leftID = 0;
            u32 rightDockID = ImGui::DockBuilderSplitNode(dockID, ImGuiDir_Right, 0.25, nullptr, &leftID);
            ImGui::DockBuilderDockWindow("Properties", rightDockID);
            ImGui::DockBuilderDockWindow("Scene View", leftID);
        }
        ImGui::DockSpace(dockID, ImVec2(0, 0), ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoTabBar);

        ImGui::Begin("Scene View", nullptr, kWindowStyle);

        ImVec2 workingArea = ImGui::GetContentRegionAvail();

        if (!pImage)
        {
            ImageDesc imageDesc = {};
            imageDesc.Format = LR_RESOURCE_FORMAT_RGBA8F;
            imageDesc.UsageFlags = LR_RESOURCE_USAGE_UNORDERED_ACCESS | LR_RESOURCE_USAGE_SHADER_RESOURCE;
            imageDesc.ArraySize = 1;
            imageDesc.MipMapLevels = 1;

            ImageData imageData = {};
            imageData.TargetAllocator = AllocatorType::ImageTLSF;
            imageData.Width = workingArea.x;
            imageData.Height = workingArea.y;

            pImage = pAPI->CreateImage(&imageDesc, &imageData);

            BaseCommandList *pList = pAPI->GetCommandList();
            pAPI->BeginCommandList(pList);

            PipelineBarrier transitionBarrier = {
                .CurrentUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
                .CurrentStage = LR_PIPELINE_STAGE_NONE,
                .CurrentAccess = LR_PIPELINE_ACCESS_NONE,
                .NextUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
                .NextStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
                .NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
            };

            pList->SetImageBarrier(pImage, &transitionBarrier);
            pAPI->EndCommandList(pList);
            pAPI->ExecuteCommandList(pList, true);

            DescriptorSetDesc descriptorDesc = {};
            descriptorDesc.BindingCount = 1;

            descriptorDesc.pBindings[0] = {
                .BindingID = 0,
                .Type = LR_DESCRIPTOR_TYPE_UNORDERED_ACCESS_IMAGE,
                .TargetShader = LR_SHADER_STAGE_COMPUTE,
                .ArraySize = 1,
                .pImage = pImage,
            };

            pComputeSet = pAPI->CreateDescriptorSet(&descriptorDesc);
            pAPI->UpdateDescriptorData(pComputeSet);

            pList = pAPI->GetCommandList();
            pAPI->BeginCommandList(pList);

            transitionBarrier = {
                .CurrentUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
                .CurrentStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
                .CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
                .NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
                .NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
                .NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
            };

            pList->SetImageBarrier(pImage, &transitionBarrier);
            pAPI->EndCommandList(pList);
            pAPI->ExecuteCommandList(pList, true);

            descriptorDesc.pBindings[0] = {
                .BindingID = 0,
                .Type = LR_DESCRIPTOR_TYPE_SHADER_RESOURCE,
                .TargetShader = LR_SHADER_STAGE_PIXEL,
                .ArraySize = 1,
                .pImage = pImage,
            };

            BaseShader *pComputeShader = pAPI->CreateShader(LR_SHADER_STAGE_COMPUTE, "basic.c.hlsl");

            ComputePipelineBuildInfo *pBuildInfo = pAPI->BeginComputePipelineBuildInfo();
            pBuildInfo->SetShader(pComputeShader, "main");
            pBuildInfo->SetDescriptorSets({ pComputeSet }, nullptr);

            BasePipeline *pPipeline = pAPI->EndComputePipelineBuildInfo(pBuildInfo);

            pSet = pAPI->CreateDescriptorSet(&descriptorDesc);
            pAPI->UpdateDescriptorData(pSet);

            pList = pAPI->GetCommandList();
            pAPI->BeginCommandList(pList);

            transitionBarrier = {
                .CurrentUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
                .CurrentStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
                .CurrentAccess = LR_PIPELINE_ACCESS_SHADER_READ,
                .NextUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
                .NextStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
                .NextAccess = LR_PIPELINE_ACCESS_SHADER_WRITE,
            };
            pList->SetImageBarrier(pImage, &transitionBarrier);

            pList->SetComputePipeline(pPipeline);
            pList->SetComputeDescriptorSets({ pComputeSet });

            pList->Dispatch(workingArea.x / 16, workingArea.y / 16, 1);

            transitionBarrier = {
                .CurrentUsage = LR_RESOURCE_USAGE_UNORDERED_ACCESS,
                .CurrentStage = LR_PIPELINE_STAGE_COMPUTE_SHADER,
                .CurrentAccess = LR_PIPELINE_ACCESS_SHADER_WRITE,
                .NextUsage = LR_RESOURCE_USAGE_SHADER_RESOURCE,
                .NextStage = LR_PIPELINE_STAGE_PIXEL_SHADER,
                .NextAccess = LR_PIPELINE_ACCESS_SHADER_READ,
            };
            pList->SetImageBarrier(pImage, &transitionBarrier);

            pAPI->EndCommandList(pList);
            pAPI->ExecuteCommandList(pList, true);
        }

        ImGui::Image(pSet, workingArea);
        ImGui::End();

        ImGui::Begin("Properties", nullptr, kWindowStyle);
        ImGui::End();
    }
    ImGui::End();
}
