#include "BaseAPI.hh"

namespace lr::Graphics
{
    void BaseAPI::InitCommandLists()
    {
        ZoneScoped;

        for (BaseCommandList *&pList : m_CommandListPool.m_DirectLists)
            pList = CreateCommandList(CommandListType::Direct);

        for (BaseCommandList *&pList : m_CommandListPool.m_ComputeLists)
            pList = CreateCommandList(CommandListType::Compute);

        for (BaseCommandList *&pList : m_CommandListPool.m_CopyLists)
            pList = CreateCommandList(CommandListType::Copy);
    }

    BaseCommandList *BaseAPI::GetCommandList(CommandListType type)
    {
        ZoneScoped;

        return m_CommandListPool.Acquire(type);
    }

}  // namespace lr::Graphics