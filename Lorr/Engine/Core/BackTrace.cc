#include "BackTrace.hh"
#include "BackTraceSymbols.hh"

#include "FileSystem.hh"

#include <tlhelp32.h>
#include <eathread/eathread_callstack.h>
#include <eathread/eathread_callstack_context.h>

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

static void PrintSymbol(u32 frameIdx, uptr address)
{
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
    {
        u32 err = GetLastError();
        if (err != 487)
            _snprintf(pSymGetLineFromAddrMsg, 64, "SymGetLineFromAddr64-%lu", err);
    }

    IMAGEHLP_MODULE64 module;
    module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (!lrSymGetModuleInfo64(_bt.m_pProcess, address, &module))
        _snprintf(pGetModuleInfoMsg, 64, "lrSymGetModuleInfo64-%lu", GetLastError());

    LOG_TRACE("#{}: {} {}!{}+0x{} ({}:{})", frameIdx, address, module.ModuleName, pSymbolName, lineOff, line.FileName, line.LineNumber);

    if (strlen(pGetModuleInfoMsg) || strlen(pSymFromAddrMsg) || strlen(pSymGetLineFromAddrMsg))
        LOG_TRACE("{} {} {}", pGetModuleInfoMsg, pSymFromAddrMsg, pSymGetLineFromAddrMsg);

    SetLastError(0);
}

static LONG UnhandledExceptionHandler(LPEXCEPTION_POINTERS exceptions)
{
    HANDLE pSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (pSnapshot == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;

    THREADENTRY32 threadEntry = {
        .dwSize = sizeof(THREADENTRY32),
    };

    if (!Thread32First(pSnapshot, &threadEntry))
    {
        CloseHandle(pSnapshot);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    PEXCEPTION_RECORD pRecord = exceptions->ExceptionRecord;
    LOG_TRACE("***** Unhandled Exception Record *****");
    LOG_TRACE("- Access Violation (0x{}) at address 0x{}", pRecord->ExceptionCode, pRecord->ExceptionInformation[1]);
    PrintSymbol(0, (uptr)pRecord->ExceptionAddress);

    uptr thisProc = GetCurrentProcessId();
    do
    {
        if (threadEntry.th32OwnerProcessID == thisProc)
        {
            HANDLE th = OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadEntry.th32ThreadID);
            if (th != INVALID_HANDLE_VALUE)
            {
                BackTrace::PrintTrace(threadEntry.th32ThreadID);
                SuspendThread(th);
                CloseHandle(th);
            }
        }
    } while (Thread32Next(pSnapshot, &threadEntry));
    CloseHandle(pSnapshot);
    return 1;
}

void BackTrace::Init()
{
    ZoneScoped;

    ::SetUnhandledExceptionFilter(UnhandledExceptionHandler);
    _bt.m_pProcess = GetCurrentProcess();

    _bt.m_pDbgHelpDll = fs::load_lib("dbghelp.dll");
    if (_bt.m_pDbgHelpDll == nullptr)
    {
        LOG_WARN("Could not load dbghelp.dll, crash report will not function properly.");
        return;
    }

    _BT_IMPORT_DBGHELP_SYMBOLS

    lrSymSetOptions(
        SYMOPT_CASE_INSENSITIVE | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_AUTO_PUBLICS
        | SYMOPT_NO_IMAGE_SEARCH | SYMOPT_NO_PROMPTS | SYMOPT_DEBUG);
    lrSymInitialize(_bt.m_pProcess, NULL, TRUE);
}

void BackTrace::PrintTrace(iptr threadID)
{
    ZoneScoped;

    if (!_bt.m_pDbgHelpDll)
        return;

    EA::Thread::CallstackContext context = {};
    EA::Thread::GetCallstackContext(context, threadID);

    LOG_TRACE("\n### Trace for Thread {}, named {} ###\n", threadID, EA::Thread::GetThreadName(&threadID));
    LOG_TRACE("\n--- Callstack Begin ---\n");

    void *pCallstack[32] = {};
    u32 frameCount = EA::Thread::GetCallstack(pCallstack, 32, nullptr);

    for (u32 i = 0; i < frameCount; i++)
    {
        void *pCurrentFrame = pCallstack[i];
        uptr address = (uptr)pCurrentFrame;

        PrintSymbol(i, address);
    }

    LOG_TRACE("\n--- Callstack End ---\n");
}

}  // namespace lr