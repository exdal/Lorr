// Created on Sunday May 21st 2023 by exdal
// Last modified on Monday May 22nd 2023 by exdal

#include "BackTrace.hh"
#include "BackTraceSymbols.hh"

#include <eathread/eathread_callstack.h>
#include <eathread/eathread_callstack_context.h>

#include "OS/Directory.hh"

/// DEFINE SYMBOLS
#define _BT_DEFINE_FUNCTION(_name) PFN_##_name lr##_name
_BT_IMPORT_DBGHELP_SYMBOLS
#undef _BT_DEFINE_FUNCTION

/// LOAD SYMBOLS
#define _BT_DEFINE_FUNCTION(_name)                                               \
    lr##_name = (PFN_##_name)GetProcAddress((HMODULE)_bt.m_pDbgHelpDll, #_name); \
    if (lr##_name == nullptr)                                                    \
    {                                                                            \
        LOG_CRITICAL("Cannot load dbghelp symbol '{}'!", #_name);                \
        FreeLibrary((HMODULE)_bt.m_pDbgHelpDll);                                 \
        _bt.m_SymbolsLoaded = false;                                             \
        return;                                                                  \
    }

namespace lr
{
static BackTrace _bt;

static LONG UnhandledExceptionHandler(LPEXCEPTION_POINTERS exceptions)
{
    BackTrace::PrintTrace(0);

    return 1;
}

void BackTrace::Init()
{
    ZoneScoped;

    ::SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    _bt.m_pProcess = GetCurrentProcess();

    _bt.m_pDbgHelpDll = OS::LoadDll("dbghelp.dll");
    if (_bt.m_pDbgHelpDll == nullptr)
    {
        LOG_WARN(
            "Cannot load backtrace libraries<{}> library, BackTrace initialization failed.", _bt.m_pDbgHelpDll);
        return;
    }

    _BT_IMPORT_DBGHELP_SYMBOLS

    lrSymSetOptions(
        SYMOPT_CASE_INSENSITIVE | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_FAIL_CRITICAL_ERRORS
        | SYMOPT_AUTO_PUBLICS | SYMOPT_NO_IMAGE_SEARCH | SYMOPT_NO_PROMPTS | SYMOPT_DEBUG);
    lrSymInitialize(_bt.m_pProcess, NULL, TRUE);
}

void BackTrace::PrintTrace(iptr threadID)
{
    ZoneScoped;

    EA::Thread::CallstackContext context = {};
    EA::Thread::GetCallstackContext(context, threadID);

    fprintf(stderr, "\n--- Callstack Begin ---\n");

    void *pCallstack[32] = {};
    u32 frameCount = EA::Thread::GetCallstack(pCallstack, 32, nullptr);

    for (u32 i = 0; i < frameCount; i++)
    {
        void *pCurrentFrame = pCallstack[i];
        uptr address = (uptr)pCurrentFrame;

        char pSymFromAddrMsg[64] = {};
        char pSymGetLineFromAddrMsg[64] = {};
        char pGetModuleInfoMsg[64] = {};

        char pSymbolName[1112] = {};
        PSYMBOL_INFO pSymbolInfo = (PSYMBOL_INFO)&pSymbolName;
        pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbolInfo->MaxNameLen = sizeof(pSymbolName);

        if (lrSymFromAddr(_bt.m_pProcess, address, 0, pSymbolInfo))
            lrUnDecorateSymbolName(pSymbolInfo->Name, pSymbolName, pSymbolInfo->MaxNameLen, UNDNAME_NAME_ONLY);
        else
            _snprintf(pSymFromAddrMsg, 64, "SymFromAddr-%lu", GetLastError());

        DWORD lineOff = 0;
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        if (!lrSymGetLineFromAddr64(_bt.m_pProcess, address, &lineOff, &line))
            _snprintf(pSymGetLineFromAddrMsg, 64, "SymGetLineFromAddr64-%lu", GetLastError());

        IMAGEHLP_MODULE64 module;
        module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
        if (!lrSymGetModuleInfo64(_bt.m_pProcess, address, &module))
            _snprintf(pGetModuleInfoMsg, 64, "lrSymGetModuleInfo64-%lu", GetLastError());

        fprintf(stderr, "#%d: %.8I64x %s!%s+0x%lx ", i, address, module.ModuleName, pSymbolName, lineOff);

        if (line.LineNumber)
            fprintf(stderr, "(%s:%lu) ", line.FileName, line.LineNumber);

        if (strlen(pGetModuleInfoMsg) || strlen(pSymFromAddrMsg) || strlen(pSymGetLineFromAddrMsg))
            fprintf(stderr, "%s %s %s", pGetModuleInfoMsg, pSymFromAddrMsg, pSymGetLineFromAddrMsg);

        fprintf(stderr, "\n");

        SetLastError(0);
    }

    fflush(stderr);
}

}  // namespace lr