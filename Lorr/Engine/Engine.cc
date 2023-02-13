#include "Engine.hh"

#include "Core/Utils/Timer.hh"

#include "Core/Graphics/RHI/D3D12/D3D12API.hh"
#include "Core/Graphics/RHI/Vulkan/VKAPI.hh"

#include "Renderer/Pass.hh"

namespace lr
{
    void Engine::Init(EngineDesc &engineDesc)
    {
        ZoneScoped;

        switch (engineDesc.m_TargetAPI)
        {
            case LR_API_TYPE_VULKAN:
                m_pAPI = new Graphics::VKAPI;
                break;
            case LR_API_TYPE_D3D12:
                m_pAPI = new Graphics::D3D12API;
                break;
            default:
                break;
        }

        Logger::Init();

        m_EventMan.Init();
        m_Window.Init(engineDesc.m_WindowDesc);
        m_ImGui.Init(m_Window.m_Width, m_Window.m_Height);

        Graphics::BaseAPIDesc apiDesc = {
            .m_Flags = LR_API_FLAG_NONE,
            .m_SwapChainFlags = LR_SWAP_CHAIN_FLAG_TRIPLE_BUFFERING,
            .m_pTargetWindow = &m_Window,
            .m_AllocatorDesc = {
                .m_MaxTLSFAllocations = 0x2000,
                .m_DescriptorMem = Memory::MiBToBytes(1),
                .m_BufferLinearMem = Memory::MiBToBytes(64),
                .m_BufferTLSFMem = Memory::MiBToBytes(128),
                .m_BufferTLSFHostMem = Memory::MiBToBytes(64),
                .m_BufferFrametimeMem = Memory::MiBToBytes(64),
                .m_ImageTLSFMem = Memory::MiBToBytes(256),
            },
        };
        m_pAPI->Init(&apiDesc);

        Renderer::RenderGraphDesc renderGraphDesc = {
            .m_pAPI = m_pAPI,
        };
        m_RenderGraph.Init(&renderGraphDesc);

        Renderer::AddImguiPass(&m_RenderGraph);
    }

    void Engine::PushEvent(Event event, EngineEventData &data)
    {
        ZoneScoped;

        m_EventMan.Push(event, data);
    }

    void Engine::DispatchEvents()
    {
        ZoneScoped;

        u32 eventID = LR_INVALID_EVENT_ID;
        while (m_EventMan.Peek(eventID))
        {
            EngineEventData data = {};
            Event event = m_EventMan.Dispatch(eventID, data);

            switch (event)
            {
                case ENGINE_EVENT_QUIT:
                {
                    m_ShuttingDown = true;

                    break;
                }

                case ENGINE_EVENT_RESIZE:
                {
                    m_Window.m_Width = data.m_SizeWidth;
                    m_Window.m_Height = data.m_SizeHeight;

                    // m_RendererMan.Resize(data.m_SizeWidth, data.m_SizeHeight);

                    break;
                }

                case ENGINE_EVENT_MOUSE_POSITION:
                {
                    ImGuiIO &io = ImGui::GetIO();
                    io.MousePos.x = data.m_MouseX;
                    io.MousePos.y = data.m_MouseY;

                    break;
                }

                case ENGINE_EVENT_MOUSE_STATE:
                {
                    ImGuiIO &io = ImGui::GetIO();
                    io.MouseDown[data.m_Mouse - 1] = !(bool)data.m_MouseState;

                    break;
                }

                case ENGINE_EVENT_CURSOR_STATE:
                {
                    m_Window.SetCursor(data.m_WindowCursor);

                    break;
                }

                default:
                    break;
            }
        }
    }

    void Engine::BeginFrame()
    {
        ZoneScoped;

        m_Window.Poll();
        DispatchEvents();

        m_ImGui.NewFrame(m_Window.m_Width, m_Window.m_Height);

        m_pAPI->BeginFrame();
    }

    void Engine::EndFrame()
    {
        ZoneScoped;

        m_ImGui.EndFrame();

        m_RenderGraph.Draw();
        m_pAPI->EndFrame();
    }

}  // namespace lr