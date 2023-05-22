// Created on Monday May 22nd 2023 by exdal
// Last modified on Monday May 22nd 2023 by exdal
#include <Windows.h>
#include <DbgHelp.h>

#ifdef SearchPath
#undef SearchPath
#endif

/// dbghelp.dll
typedef LPAPI_VERSION(__stdcall *PFN_ImagehlpApiVersion)(void);
typedef BOOL(__stdcall *PFN_SymCleanup)(HANDLE hProcess);
typedef BOOL(__stdcall *PFN_SymEnumerateModules64)(
    HANDLE hProcess, PSYM_ENUMMODULES_CALLBACK64 EnumModulesCallback, PVOID UserContext);
typedef PVOID(__stdcall *PFN_SymFunctionTableAccess64)(HANDLE hProcess, DWORD64 AddrBase);
typedef BOOL(__stdcall *PFN_SymGetLineFromAddr64)(
    HANDLE hProcess, DWORD64 dwAddr, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line);
typedef DWORD64(__stdcall *PFN_SymGetModuleBase64)(HANDLE hProcess, DWORD64 dwAddr);
typedef BOOL(__stdcall *PFN_SymGetModuleInfo64)(HANDLE hProcess, DWORD64 dwAddr, PIMAGEHLP_MODULE64 ModuleInfo);
typedef DWORD(__stdcall *PFN_SymGetOptions)(void);
typedef BOOL(__stdcall *PFN_SymGetSearchPath)(HANDLE hProcess, PTSTR SearchPath, DWORD SearchPathLength);
typedef BOOL(__stdcall *PFN_SymFromAddr)(
    HANDLE hProcess, DWORD64 dwAddr, PDWORD64 pdwDisplacement, PSYMBOL_INFO Symbol);
typedef BOOL(__stdcall *PFN_SymInitialize)(HANDLE hProcess, PCTSTR UserSearchPath, BOOL fInvadeProcess);
typedef DWORD64(__stdcall *PFN_SymLoadModuleEx)(
    HANDLE hProcess, HANDLE hFile, PCSTR ImageName, PCSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);
typedef BOOL(__stdcall *PFN_SymRegisterCallback64)(
    HANDLE hProcess, PSYMBOL_REGISTERED_CALLBACK64 CallbackFunction, ULONG64 UserContext);
typedef DWORD(__stdcall *PFN_SymSetOptions)(DWORD SymOptions);
typedef BOOL(__stdcall *PFN_StackWalk)(
    DWORD MachineType,
    HANDLE hProcess,
    HANDLE hThread,
    LPSTACKFRAME64 StackFrame,
    PVOID ContextRecord,
    PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
    PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
    PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
    PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
typedef DWORD(__stdcall *PFN_UnDecorateSymbolName)(
    PCSTR DecoratedName, PSTR UnDecoratedName, DWORD UndecoratedLength, DWORD Flags);

/// version.dll
typedef BOOL(__stdcall *PFN_GetFileVersionInfoSize)(LPCSTR lptstrFilename, LPDWORD lpdwHandle);
typedef BOOL(__stdcall *PFN_GetFileVersionInfo)(
    LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
typedef BOOL(__stdcall *PFN_VerQueryValue)(
    const LPVOID pBlock, LPTSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen);

#define _BT_DEFINE_FUNCTION(_name) extern PFN_##_name lr##_name

#define _BT_IMPORT_DBGHELP_SYMBOLS                 \
    _BT_DEFINE_FUNCTION(ImagehlpApiVersion);       \
    _BT_DEFINE_FUNCTION(SymCleanup);               \
    _BT_DEFINE_FUNCTION(SymEnumerateModules64);    \
    _BT_DEFINE_FUNCTION(SymFunctionTableAccess64); \
    _BT_DEFINE_FUNCTION(SymGetLineFromAddr64);     \
    _BT_DEFINE_FUNCTION(SymGetModuleBase64);       \
    _BT_DEFINE_FUNCTION(SymGetModuleInfo64);       \
    _BT_DEFINE_FUNCTION(SymGetOptions);            \
    _BT_DEFINE_FUNCTION(SymGetSearchPath);         \
    _BT_DEFINE_FUNCTION(SymFromAddr);              \
    _BT_DEFINE_FUNCTION(SymInitialize);            \
    _BT_DEFINE_FUNCTION(SymLoadModuleEx);          \
    _BT_DEFINE_FUNCTION(SymRegisterCallback64);    \
    _BT_DEFINE_FUNCTION(SymSetOptions);            \
    _BT_DEFINE_FUNCTION(StackWalk);                \
    _BT_DEFINE_FUNCTION(UnDecorateSymbolName);

_BT_IMPORT_DBGHELP_SYMBOLS

#undef _BT_DEFINE_FUNCTION