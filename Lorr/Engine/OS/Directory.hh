// Created on Monday May 22nd 2023 by exdal
// Last modified on Monday May 22nd 2023 by exdal

#pragma once

namespace lr::OS
{
bool SetLibraryDirectory(eastl::string_view path);
void *LoadDll(eastl::string_view path);
bool GetCurrentDir(eastl::string &pathOut);
}  // namespace lr::OS