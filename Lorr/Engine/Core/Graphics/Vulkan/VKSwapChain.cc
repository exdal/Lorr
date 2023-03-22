#include "VKSwapChain.hh"

namespace lr::Graphics
{
void VKSwapChain::Advance(u32 nextImage, Semaphore *pAcquireSemp)
{
    ZoneScoped;

    m_CurrentFrame = nextImage;
    m_pFrames[m_CurrentFrame].m_pAcquireSemp = pAcquireSemp;
}

SwapChainFrame *VKSwapChain::GetCurrentFrame()
{
    return &m_pFrames[m_CurrentFrame];
}

}  // namespace lr::Graphics