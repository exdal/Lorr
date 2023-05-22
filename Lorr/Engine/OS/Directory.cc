// Created on Monday May 22nd 2023 by exdal
// Last modified on Monday May 22nd 2023 by exdal

#include "Directory.hh"

#include <Windows.h>

namespace lr
{
bool OS::SetLibraryDirectory(eastl::string_view path)
{
    ZoneScoped;

    return SetDllDirectoryA(path.data());
}

void *OS::LoadDll(eastl::string_view path)
{
    ZoneScoped;

    return LoadLibraryA(path.data());
}

bool OS::GetCurrentDir(eastl::string &pathOut)
{
    ZoneScoped;

    char pData[1024] = {};
    bool status = GetCurrentDirectoryA(1024, pData);
    pathOut.append(pData, strlen(pData));

    return status;
}

}  // namespace lr