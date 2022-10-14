//
// Created on Monday 9th May 2022 by e-erdal
//

#pragma once

/// Window Handle is defined in BaseWindow.hh

#if _WIN32
#include "Core/Window/Win32/Win32Window.hh"
using PlatformWindow = lr::Win32Window;
#endif
