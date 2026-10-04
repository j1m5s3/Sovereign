// Symbol export for builds that put the core in a shared library: the Unreal
// editor loads every module as a DLL, so the SovereignCore module defines
// SOV_SHARED (and SOV_BUILDING_CORE while compiling the core itself). The
// standalone CMake build and monolithic game builds leave SOV_API empty.
#pragma once

#if defined(SOV_SHARED)
#  if defined(_WIN32)
#    if defined(SOV_BUILDING_CORE)
#      define SOV_API __declspec(dllexport)
#    else
#      define SOV_API __declspec(dllimport)
#    endif
#    if defined(_MSC_VER)
// Exported classes hold standard-library members; both sides use the same compiler and runtime.
#      pragma warning(disable : 4251)
#    endif
#  else
#    define SOV_API __attribute__((visibility("default")))
#  endif
#else
#  define SOV_API
#endif
