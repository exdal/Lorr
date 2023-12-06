// Created on Sunday May 21st 2023 by exdal
// Last modified on Tuesday June 27th 2023 by exdal

#pragma once

/// THIS IS A VERY WIP STACKWALKER ///
/// AND PLANNED FOR FUTURE USE ///

namespace lr
{
struct BackTrace
{
    static void Init();
    static void PrintTrace(iptr threadID);

    void *m_pDbgHelpDll = nullptr;
    void *m_pProcess = nullptr;
    bool m_SymbolsLoaded = false;
};

}  // namespace lr