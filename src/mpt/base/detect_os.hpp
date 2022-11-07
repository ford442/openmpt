/* SPDX-License-Identifier: BSL-1.0 OR BSD-3-Clause */

#ifndef MPT_BASE_DETECT_OS_HPP
#define MPT_BASE_DETECT_OS_HPP



// The order of the checks matters!
#if defined(__DJGPP__)
#define MPT_OS_DJGPP 1
#elif defined(__EMSCRIPTEN__)
#define MPT_OS_EMSCRIPTEN 1
#if !defined(__EMSCRIPTEN_major__) || !defined(__EMSCRIPTEN_minor__) || !defined(__EMSCRIPTEN_tiny__)
#include <emscripten/version.h>
#endif
#if defined(__EMSCRIPTEN_major__) && defined(__EMSCRIPTEN_minor__)
#if (__EMSCRIPTEN_major__ > 3)
// ok
#elif (__EMSCRIPTEN_major__ == 3) && (__EMSCRIPTEN_minor__ > 1)
// ok
#elif (__EMSCRIPTEN_major__ == 3) && (__EMSCRIPTEN_minor__ == 1) && (__EMSCRIPTEN_tiny__ >= 1)
// ok
#else
#error "Emscripten >= 3.1.1 is required."
#endif
#endif
#elif defined(_WIN32)
#define MPT_OS_WINDOWS 1
#if !defined(_WIN32_WINDOWS) && !defined(WINVER)
// include modern SDK version header if not targeting Win9x
#include <sdkddkver.h>
#endif
#if defined(WINAPI_FAMILY)
#include <winapifamily.h>
#if (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#define MPT_OS_WINDOWS_WINRT 0
#else
#define MPT_OS_WINDOWS_WINRT 1
#endif
#else // !WINAPI_FAMILY
#define MPT_OS_WINDOWS_WINRT 0
#endif // WINAPI_FAMILY
#if defined(NTDDI_VERSION) || defined(_WIN32_WINNT)
#define MPT_OS_WINDOWS_WINNT 1
#define MPT_OS_WINDOWS_WIN9X 0
#define MPT_OS_WINDOWS_WIN32 0
#elif defined(_WIN32_WINDOWS)
#define MPT_OS_WINDOWS_WINNT 0
#define MPT_OS_WINDOWS_WIN9X 1
#define MPT_OS_WINDOWS_WIN32 0
#elif defined(WINVER)
#define MPT_OS_WINDOWS_WINNT 0
#define MPT_OS_WINDOWS_WIN9X 0
#define MPT_OS_WINDOWS_WIN32 1
#else
// assume modern
#define MPT_OS_WINDOWS_WINNT 1
#define MPT_OS_WINDOWS_WIN9X 0
#define MPT_OS_WINDOWS_WIN32 0
#endif
#elif defined(__APPLE__)
#define MPT_OS_MACOSX_OR_IOS 1
#include <TargetConditionals.h>
#if defined(TARGET_OS_OSX)
#if (TARGET_OS_OSX != 0)
#include <AvailabilityMacros.h>
#endif
#endif
//#if TARGET_IPHONE_SIMULATOR
//#elif TARGET_OS_IPHONE
//#elif TARGET_OS_MAC
//#else
//#endif
#elif defined(__HAIKU__)
#define MPT_OS_HAIKU 1
#elif defined(__ANDROID__) || defined(ANDROID)
#define MPT_OS_ANDROID 1
#elif defined(__linux__)
#define MPT_OS_LINUX 1
#elif defined(__DragonFly__)
#define MPT_OS_DRAGONFLYBSD 1
#elif defined(__FreeBSD__)
#define MPT_OS_FREEBSD 1
#elif defined(__OpenBSD__)
#define MPT_OS_OPENBSD 1
#elif defined(__NetBSD__)
#define MPT_OS_NETBSD 1
#elif defined(__unix__)
#define MPT_OS_GENERIC_UNIX 1
#else
#define MPT_OS_UNKNOWN 1
#endif

#ifndef MPT_OS_DJGPP
#define MPT_OS_DJGPP 0
#endif
#ifndef MPT_OS_EMSCRIPTEN
#define MPT_OS_EMSCRIPTEN 0
#endif
#ifndef MPT_OS_WINDOWS
#define MPT_OS_WINDOWS 0
#endif
#ifndef MPT_OS_WINDOWS_WINRT
#define MPT_OS_WINDOWS_WINRT 0
#endif
#ifndef MPT_OS_WINDOWS_WINNT
#define MPT_OS_WINDOWS_WINNT 0
#endif
#ifndef MPT_OS_WINDOWS_WIN9X
#define MPT_OS_WINDOWS_WIN9X 0
#endif
#ifndef MPT_OS_MACOSX_OR_IOS
#define MPT_OS_MACOSX_OR_IOS 0
#endif
#ifndef MPT_OS_HAIKU
#define MPT_OS_HAIKU 0
#endif
#ifndef MPT_OS_ANDROID
#define MPT_OS_ANDROID 0
#endif
#ifndef MPT_OS_LINUX
#define MPT_OS_LINUX 0
#endif
#ifndef MPT_OS_DRAGONFLYBSD
#define MPT_OS_DRAGONFLYBSD 0
#endif
#ifndef MPT_OS_FREEBSD
#define MPT_OS_FREEBSD 0
#endif
#ifndef MPT_OS_OPENBSD
#define MPT_OS_OPENBSD 0
#endif
#ifndef MPT_OS_NETBSD
#define MPT_OS_NETBSD 0
#endif
#ifndef MPT_OS_GENERIC_UNIX
#define MPT_OS_GENERIC_UNIX 0
#endif
#ifndef MPT_OS_UNKNOWN
#define MPT_OS_UNKNOWN 0
#endif



#define MPT_MODE_KERNEL 0



#endif // MPT_BASE_DETECT_OS_HPP
