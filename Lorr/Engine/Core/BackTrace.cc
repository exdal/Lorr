// Created on Sunday May 21st 2023 by exdal
// Last modified on Sunday May 21st 2023 by exdal

#include "BackTrace.hh"
#include "BackTraceSymbols.hh"

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

static BOOL __stdcall SymbolRegisterCallbackFn(HANDLE pProcess, ULONG code, ULONG64 callback, ULONG64 context)
{
    PIMAGEHLP_CBA_EVENT pEvent = NULL;

    switch (code)
    {
        case CBA_EVENT:
            pEvent = (PIMAGEHLP_CBA_EVENT)callback;
            switch (pEvent->severity)
            {
                case sevInfo:
                    fprintf(stderr, "INFO: %s\n", pEvent->desc);
                    break;
                case sevProblem:
                    fprintf(stderr, "PROB: %s\n", pEvent->desc);
                    break;
                case sevAttn:
                    fprintf(stderr, "ATTN: %s\n", pEvent->desc);
                    break;
                case sevFatal:
                    fprintf(stderr, "FATAL: %s\n", pEvent->desc);
                    break;
            }
            return TRUE;
        case SSRVACTION_TRACE:
            fprintf(stderr, "DEBUG: %s\n", (PCTSTR)callback);
            return TRUE;
    }

    return FALSE;
}

static BOOL __stdcall SymbolEnumerateModulesProc64(LPCSTR moduleName, DWORD64 baseAddr, PVOID context)
{
    IMAGEHLP_MODULE64 module = { .SizeOfStruct = sizeof(IMAGEHLP_MODULE64) };

    if (!SymGetModuleInfo64(_bt.m_pProcess, baseAddr, &module))
    {
        fprintf(stderr, "lrSymGetModuleInfo64(): %lu\n", GetLastError());
        return FALSE;
    }

    char symbolType[32] = {};
    switch (module.SymType)
    {
        case SymNone:
            strcpy(symbolType, "NONE");
            break;
        case SymCoff:
            strcpy(symbolType, "COFF");
            break;
        case SymCv:
            strcpy(symbolType, "CV");
            break;
        case SymPdb:
            strcpy(symbolType, "PDB");
            break;
        case SymExport:
            strcpy(symbolType, "EXPORT");
            break;
        case SymDeferred:
            strcpy(symbolType, "DEFERRED");
            break;
        case SymSym:
            strcpy(symbolType, "SYM");
            break;
        default:
            _snprintf(symbolType, sizeof(symbolType), "symtype=%ld", (long)module.SymType);
            break;
    }

    fprintf(
        stderr,
        "LOAD: %.8I64x %.8lx %s %s %s\n",
        module.BaseOfImage,
        module.ImageSize,
        module.LoadedImageName,
        symbolType,
        module.CVData);

    return TRUE;
}

static LONG UnhandledExceptionHandler(LPEXCEPTION_POINTERS exceptions)
{
    BackTrace::PrintTrace(&exceptions);

    return 1;
}

void BackTrace::Init()
{
    ZoneScoped;

    ::SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    SetDllDirectoryA("C:\\repos\\Lorr2\\build\\bin");

    _bt.m_pDbgHelpDll = LoadLibraryA("dbghelp.dll");
    if (_bt.m_pDbgHelpDll == nullptr)
    {
        LOG_WARN(
            "Cannot load backtrace libraries<{}> library, BackTrace initialization failed.", _bt.m_pDbgHelpDll);
        return;
    }

    _BT_IMPORT_DBGHELP_SYMBOLS

    _bt.m_SymbolsLoaded = true;
}

void BackTrace::PrintTrace(void *pEP, u32 frameCount)
{
    ZoneScoped;

    HANDLE hThread = GetCurrentThread();
    CONTEXT context = *((EXCEPTION_POINTERS *)pEP)->ContextRecord;

    /// BEGIN FRAME ///

    DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentProcess(),
        GetCurrentProcess(),
        &_bt.m_pProcess,
        0,
        false,
        DUPLICATE_SAME_ACCESS);

    IMAGEHLP_MODULE64 module = {};
    module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

    SymGetModuleInfo64(_bt.m_pProcess, NULL, &module);

    /// SEARCH AVAILABLE SYMBOLS ///
    eastl::string searchPath = "srv*http://msdl.microsoft.com/download/symbols;";

    char pDirectoryData[1024] = {};
    if (GetCurrentDirectoryA(1024, pDirectoryData))
    {
        searchPath.append(pDirectoryData, strlen(pDirectoryData));
        searchPath += ';';
    }

    if (GetModuleFileNameA(NULL, pDirectoryData, 1024))
    {
        eastl::string_view name(pDirectoryData, strlen(pDirectoryData));
        uptr namePos = name.find_last_of("\\/");
        name = name.substr(namePos + 1, name.length() - namePos);
        searchPath.append(name.data(), name.length());
    }

    DWORD options = 0;
    options |= SYMOPT_CASE_INSENSITIVE;
    options |= SYMOPT_LOAD_LINES;
    options |= SYMOPT_OMAP_FIND_NEAREST;
    options |= SYMOPT_FAIL_CRITICAL_ERRORS;
    options |= SYMOPT_AUTO_PUBLICS;
    options |= SYMOPT_NO_IMAGE_SEARCH;
    options |= SYMOPT_DEBUG;
    options |= SYMOPT_NO_PROMPTS;
    SymSetOptions(options);
    SymInitialize(_bt.m_pProcess, searchPath.c_str(), TRUE);
    SymRegisterCallback64(_bt.m_pProcess, SymbolRegisterCallbackFn, (ULONG64)_bt.m_pProcess);

    char searchPathT[1024] = {};
    LPAPI_VERSION version = lrImagehlpApiVersion();
    SymGetSearchPath(_bt.m_pProcess, searchPathT, 1024);

    fprintf(
        stderr,
        "Debugger Engine: %d.%d.%d.%d\n",
        version->MajorVersion,
        version->MinorVersion,
        version->Revision,
        version->Reserved);
    fprintf(stderr, "Symbol Lookups: '%s'\n", searchPathT);

    SymEnumerateModules64(_bt.m_pProcess, SymbolEnumerateModulesProc64, NULL);

    fprintf(stderr, "\n--- Registers ---\n");

    fprintf(
        stderr,
        "rax=%.16llx rbx=%.16llx rcx=%.16llx rdx=%.16llx rsi=%.16llx rdi=%.16llx\n",
        context.Rax,
        context.Rbx,
        context.Rcx,
        context.Rdx,
        context.Rsi,
        context.Rdi);

    fprintf(
        stderr,
        "r8=%.16llx r9=%.16llx r10=%.16llx r11=%.16llx r12=%.16llx r13=%.16llx\n",
        context.R8,
        context.R9,
        context.R10,
        context.R11,
        context.R12,
        context.R13);

    fprintf(
        stderr,
        "r14=%.16llx r15=%.16llx rip=%.16llx rsp=%.16llx rbp=%.16llx\n",
        context.R14,
        context.R15,
        context.Rip,
        context.Rsp,
        context.Rbp);

    fprintf(
        stderr,
        "cs=%.4x  ss=%.4x  ds=%.4x  es=%.4x  fs=%.4x  gs=%.4x efl=%.8lx\n\n",
        context.SegCs,
        context.SegSs,
        context.SegDs,
        context.SegEs,
        context.SegFs,
        context.SegGs,
        context.EFlags);

    fprintf(stderr, "--- Callstack ---\n");

    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;

    ULONG64 symbolData[261] = {};
    PSYMBOL_INFO pSymbolInfo = (PSYMBOL_INFO)symbolData;

    pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbolInfo->MaxNameLen = MAX_SYM_NAME;

    for (u32 frameID = 0; frameID < frameCount; frameID++)
    {
        if (!StackWalk(
                IMAGE_FILE_MACHINE_AMD64,
                _bt.m_pProcess,
                hThread,
                &frame,
                &context,
                NULL,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                NULL))
            return;

        if (frame.AddrPC.Offset == 0)
        {
            fprintf(stderr, "#%d: NONE\n", frameID);
            continue;
        }

        char pGetModuleInfoMsg[64] = {};
        IMAGEHLP_MODULE64 module = { .SizeOfStruct = sizeof(IMAGEHLP_MODULE64) };
        if (!SymGetModuleInfo64(_bt.m_pProcess, frame.AddrPC.Offset, &module))
            _snprintf(pGetModuleInfoMsg, 64, "SymGetModuleInfo64(): GetLastError = '%lu'", GetLastError());

        char pSymFromAddrMsg[64] = {};
        DWORD64 symbolOff = 0;
        char symbolName[MAX_SYM_NAME] = {};
        if (SymFromAddr(_bt.m_pProcess, frame.AddrPC.Offset, &symbolOff, pSymbolInfo))
            UnDecorateSymbolName(pSymbolInfo->Name, symbolName, MAX_SYM_NAME, UNDNAME_NAME_ONLY);
        else
            _snprintf(pSymFromAddrMsg, 64, "SymFromAddr(): GetLastError = '%lu'", GetLastError());

        char pSymGetLineFromAddrMsg[64] = {};
        DWORD lineOff = 0;
        IMAGEHLP_LINE64 line = { .SizeOfStruct = sizeof(IMAGEHLP_LINE64) };
        if (!SymGetLineFromAddr64(_bt.m_pProcess, frame.AddrPC.Offset, &lineOff, &line))
            _snprintf(
                pSymGetLineFromAddrMsg, 64, "SymGetLineFromAddr64(): GetLastError = '%lu'", GetLastError());

        fprintf(
            stderr,
            "#%d: %.8I64x %.8I64x %.8I64x %.8I64x %.8I64x %.8I64x %s!%s+0x%lx ",
            frameID,
            frame.AddrFrame.Offset,
            frame.AddrReturn.Offset,
            frame.Params[0],
            frame.Params[1],
            frame.Params[2],
            frame.Params[3],
            module.ModuleName,
            symbolName,
            lineOff);

        if (line.LineNumber)
            fprintf(stderr, "(%s:%lu) ", line.FileName, line.LineNumber);

        if (strlen(pGetModuleInfoMsg) || strlen(pSymFromAddrMsg) || strlen(pSymGetLineFromAddrMsg))
        {
            fprintf(
                stderr,
                "%s %s %s Address = '%.8I64x'",
                pGetModuleInfoMsg,
                pSymFromAddrMsg,
                pSymGetLineFromAddrMsg,
                frame.AddrPC.Offset);
        }

        fprintf(stderr, "\n");

        if (frame.AddrReturn.Offset == 0)
        {
            SetLastError(0);
            break;
        }
    }

    fflush(stderr);
    CloseHandle(hThread);
}

bool BackTrace::Initialized()
{
    ZoneScoped;

    return _bt.m_pDbgHelpDll && _bt.m_SymbolsLoaded;
}

}  // namespace lr

#undef _BT_DEFINE_FUNCTION