#include "SwapChain.hh"

namespace lr::Graphics
{
void SwapChain::Advance(u32 nextImage, Semaphore *pAcquireSemp)
{
    ZoneScoped;

    m_CurrentFrame = nextImage;
    m_Frames[m_CurrentFrame].m_pAcquireSemp = pAcquireSemp;
}

SwapChainFrame *SwapChain::GetCurrentFrame()
{
    return &m_Frames[m_CurrentFrame];
}

}  // namespace lr::Graphics