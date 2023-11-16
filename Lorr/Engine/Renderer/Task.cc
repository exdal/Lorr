#include "Task.hh"

#include "TaskGraph.hh"

namespace lr::Renderer
{
Graphics::ImageView *TaskContext::View(const GenericResource &use)
{
    ZoneScoped;

    auto &imageInfo = m_pGraph->m_ImageInfos[use.m_ImageID];
    return imageInfo.m_pImageView;
}
}  // namespace lr::Renderer