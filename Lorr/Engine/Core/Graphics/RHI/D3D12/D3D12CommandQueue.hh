//
// Created on Tuesday 18th October 2022 by e-erdal
//

#pragma once

#include "D3D12Sym.hh"

#include "D3D12CommandList.hh"

namespace lr::Graphics
{
    struct D3D12CommandQueue
    {
        void ExecuteCommandList(D3D12CommandList *pList);

        ID3D12CommandQueue *m_pHandle = nullptr;
    };

}  // namespace lr::Graphics