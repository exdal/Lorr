// Created on Sunday May 21st 2023 by exdal
// Last modified on Sunday May 21st 2023 by exdal

#pragma once

namespace lr
{
struct BackTrace
{
    static void Init();
    static void PrintTrace(void *pEP, u32 frameCount = 32);

    static bool Initialized();

    void *m_pDbgHelpDll = nullptr;
    void *m_pProcess = nullptr;

    bool m_SymbolsLoaded = false;
};

}  // namespace lr