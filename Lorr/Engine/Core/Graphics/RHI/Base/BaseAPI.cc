#include "BaseAPI.hh"

namespace lr::Graphics
{
    CommandList *BaseAPI::GetCommandList(CommandListType type)
    {
        ZoneScoped;

        return m_CommandListPool.Acquire(type);
    }

}  // namespace lr::Graphics