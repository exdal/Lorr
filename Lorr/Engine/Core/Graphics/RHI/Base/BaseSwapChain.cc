#include "BaseSwapChain.hh"

namespace lr::Graphics
{
    void BaseSwapChain::NextFrame()
    {
        ZoneScoped;

        m_CurrentFrame = (m_CurrentFrame + 1) % m_FrameCount;
    }

}  // namespace lr::Graphics