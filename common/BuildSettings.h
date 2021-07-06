
#pragma once
#include "mpt/base/detect_compiler.hpp"
#include "mpt/base/detect_os.hpp"
#include "mpt/base/detect_quirks.hpp"
#if MPT_OS_WINDOWS

#if !defined(WINVER) && !defined(_WIN32_WINDOWS) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601 // _WIN32_WINNT_WIN7
#endif

#ifndef WINVER
#if defined(_WIN32_WINNT)
#define WINVER _WIN32_WINNT
#elif defined(_WIN32_WINDOWS)
#define WINVER _WIN32_WINDOWS
#endif
#endif

#endif // MPT_OS_WINDOWS
#if defined(MODPLUG_TRACKER) && defined(LIBOPENMPT_BUILD)
#error "either MODPLUG_TRACKER or LIBOPENMPT_BUILD has to be defined"
#elif defined(MODPLUG_TRACKER)
#define MPT_INLINE_NS mptx
#elif defined(LIBOPENMPT_BUILD)
#define MPT_INLINE_NS mpt_libopenmpt
#else
#error "either MODPLUG_TRACKER or LIBOPENMPT_BUILD has to be defined"
#endif // MODPLUG_TRACKER || LIBOPENMPT_BUILD
#if defined(LIBOPENMPT_BUILD)
// however might be set by some build systems like autotools anyway for simplicity.
#ifdef MPT_WITH_FLAC
#undef MPT_WITH_FLAC
#endif

#endif // LIBOPENMPT_BUILD
#if defined(MPT_BUILD_MSVC)
// MSVC. Other build systems provide MPT_WITH_* macros via command-line or other
// OpenMPT and libopenmpt should compile and run successfully (albeit with
// The defaults match the bundled third-party libraries with the addition of

#if defined(MODPLUG_TRACKER)

#if MPT_OS_WINDOWS
#if !defined(MPT_BUILD_WINESUPPORT) && !defined(MPT_BUILD_SIGNTOOL)
#define MPT_WITH_MFC
#endif // !MPT_BUILD_WINESUPPORT && !MPT_BUILD_SIGNTOOL
#endif // MPT_OS_WINDOWS
#define MPT_WITH_ANCIENT
#if !defined(MPT_BUILD_RETRO) && !MPT_COMPILER_CLANG && !MPT_MSVC_BEFORE(2019,0)
// https://developercommunity.visualstudio.com/t/static-inline-variable-gets-destroyed-multiple-tim/297876
#define MPT_WITH_ASIO
#endif
#if defined(MPT_BUILD_RETRO)
#define MPT_WITH_DIRECTSOUND
#endif
#define MPT_WITH_DMO
#define MPT_WITH_LAME
#define MPT_WITH_LHASA
#define MPT_WITH_MINIZIP
#define MPT_WITH_NLOHMANNJSON
#define MPT_WITH_OPUS
#define MPT_WITH_OPUSENC
#define MPT_WITH_OPUSFILE
#define MPT_WITH_PORTAUDIO
//#define MPT_WITH_PULSEAUDIOSIMPLE
#define MPT_WITH_RTAUDIO
#define MPT_WITH_SMBPITCHSHIFT
#define MPT_WITH_UNRAR
#define MPT_WITH_VORBISENC
//#define MPT_WITH_DL
#define MPT_WITH_FLAC
#if MPT_OS_WINDOWS
#if (_WIN32_WINNT >= 0x0601)
#define MPT_WITH_MEDIAFOUNDATION
#endif
#endif
//#define MPT_WITH_MINIZ
#define MPT_WITH_MPG123
#define MPT_WITH_OGG
#define MPT_WITH_VORBIS
#define MPT_WITH_VORBISFILE
#if MPT_OS_WINDOWS
#if (_WIN32_WINNT >= 0x0A00)
#define MPT_WITH_WINDOWS10
#endif
#endif
#define MPT_WITH_ZLIB

#endif // MODPLUG_TRACKER

#if defined(LIBOPENMPT_BUILD)
#if defined(LIBOPENMPT_BUILD_FULL) && defined(LIBOPENMPT_BUILD_SMALL)
#error "only one of LIBOPENMPT_BUILD_FULL or LIBOPENMPT_BUILD_SMALL can be defined"
#endif // LIBOPENMPT_BUILD_FULL && LIBOPENMPT_BUILD_SMALL

#if defined(LIBOPENMPT_BUILD_SMALL)
//#define MPT_WITH_FLAC
//#define MPT_WITH_MEDIAFOUNDATION
#define MPT_WITH_MINIMP3
#define MPT_WITH_MINIZ
//#define MPT_WITH_OGG
#define MPT_WITH_STBVORBIS
//#define MPT_WITH_VORBISFILE

#else // !LIBOPENMPT_BUILD_SMALL
//#define MPT_WITH_FLAC
//#define MPT_WITH_MEDIAFOUNDATION
//#define MPT_WITH_MINIZ
#define MPT_WITH_MPG123
#define MPT_WITH_OGG
#define MPT_WITH_VORBIS
#define MPT_WITH_VORBISFILE
#define MPT_WITH_ZLIB

#endif // LIBOPENMPT_BUILD_SMALL

#endif // LIBOPENMPT_BUILD

#endif // MPT_BUILD_MSVC
#if defined(MPT_BUILD_XCODE)

#if defined(MODPLUG_TRACKER)

#endif // MODPLUG_TRACKER

#if defined(LIBOPENMPT_BUILD)
//#define MPT_WITH_FLAC
//#define MPT_WITH_MEDIAFOUNDATION
//#define MPT_WITH_MINIZ
#define MPT_WITH_MPG123
#define MPT_WITH_OGG
#define MPT_WITH_VORBIS
#define MPT_WITH_VORBISFILE
#define MPT_WITH_ZLIB

#endif // LIBOPENMPT_BUILD

#endif // MPT_BUILD_XCODE
#if defined(MODPLUG_TRACKER)

#define MPT_UPDATE_LEGACY 1
#if defined(MPT_BUILD_DEBUG) || defined(MPT_BUILD_CHECKED)
#define ENABLE_TESTS
#endif
//#define MODPLUG_NO_FILESAVE
#if !defined(MPT_BUILD_DEBUG) && !defined(MPT_BUILD_CHECKED) && !defined(MPT_BUILD_WINESUPPORT)
#define MPT_LOG_GLOBAL_LEVEL_STATIC
#define MPT_LOG_GLOBAL_LEVEL 0
#endif
//#define MPT_ALL_LOGGING
#if !defined(MPT_BUILD_DEBUG) && !defined(MPT_BUILD_CHECKED) && !defined(MPT_BUILD_WINESUPPORT)
#define NO_ASSERTS
#endif
#define MPT_COMPONENT_MANAGER 1
#define MPT_EXTERNAL_SAMPLES
#define MPT_ENABLE_CHARSET_LOCALE
#define ENABLE_ASM

#if !defined(MPT_BUILD_RETRO)
#define MPT_ENABLE_UPDATE
#endif // !MPT_BUILD_RETRO
//#define NO_ARCHIVE_SUPPORT
//#define NO_REVERB
//#define NO_DSP
//#define NO_EQ
//#define NO_AGC
//#define NO_VST
//#define NO_PLUGINS

#endif // MODPLUG_TRACKER
#if defined(LIBOPENMPT_BUILD)

#if (defined(_DEBUG) || defined(DEBUG)) && !defined(MPT_BUILD_DEBUG)
#define MPT_BUILD_DEBUG
#endif

#if defined(LIBOPENMPT_BUILD_TEST)
#define ENABLE_TESTS
#else
#define MODPLUG_NO_FILESAVE
#endif
#if defined(MPT_BUILD_ANALZYED) || defined(MPT_BUILD_DEBUG) || defined(MPT_BUILD_CHECKED) || defined(ENABLE_TESTS)
#else
#define NO_ASSERTS
#endif
#define MPT_COMPONENT_MANAGER 0
#if defined(ENABLE_TESTS) || defined(MPT_BUILD_HACK_ARCHIVE_SUPPORT)
#define MPT_ENABLE_CHARSET_LOCALE
#else
#endif
//#define ENABLE_ASM
#if defined(MPT_BUILD_HACK_ARCHIVE_SUPPORT)
#else
#define NO_ARCHIVE_SUPPORT
#endif
#define NO_DSP
#define NO_EQ
#define NO_AGC
#define NO_VST

#endif // LIBOPENMPT_BUILD
#if MPT_OS_WINDOWS

#ifndef MPT_ENABLE_CHARSET_LOCALE
#define MPT_ENABLE_CHARSET_LOCALE
#endif

#elif MPT_OS_LINUX
#elif MPT_OS_ANDROID
#elif MPT_OS_EMSCRIPTEN
#elif MPT_OS_MACOSX_OR_IOS
#elif MPT_OS_DJGPP
#endif
#if (MPT_COMPILER_MSVC && !defined(MPT_USTRING_MODE_UTF8_FORCE)) || defined(MODPLUG_TRACKER)
// microsoft platforms.
    // Required by the tracker to ease interfacing with WinAPI.
#define MPT_WSTRING_FORMAT 1

#else
#define MPT_WSTRING_FORMAT 0
#endif
#if (MPT_COMPILER_MSVC && !defined(MPT_USTRING_MODE_UTF8_FORCE)) || MPT_OS_WINDOWS || MPT_WSTRING_FORMAT
// Required on Windows by mpt::PathString.
    // Required by MPT_WSTRING_FORMAT because of std::string<->std::wstring conversion in mpt::ToAString and mpt::ToWString.
#define MPT_WSTRING_CONVERT 1

#else
#define MPT_WSTRING_CONVERT 0
#endif
#if defined(MPT_BUILD_ANALYZED) || defined(MPT_BUILD_CHECKED)
#ifdef NO_ASSERTS
#undef NO_ASSERTS // static or dynamic analyzers want assertions on
#endif
#endif
#if defined(MPT_BUILD_FUZZER)
#ifndef MPT_FUZZ_TRACKER
#define MPT_FUZZ_TRACKER
#endif
#endif
#if !MPT_COMPILER_MSVC && defined(ENABLE_ASM)
#undef ENABLE_ASM // inline assembly requires MSVC compiler
#endif
#if defined(ENABLE_ASM)
#if MPT_COMPILER_MSVC && defined(_M_IX86)

#define ENABLE_CPUID
#define ENABLE_X86
#define ENABLE_MMX
#define ENABLE_SSE
#define ENABLE_SSE2
#define ENABLE_SSE3
#define ENABLE_SSE4
#define ENABLE_AVX
#define ENABLE_AVX2

#elif MPT_COMPILER_MSVC && defined(_M_X64)

#define ENABLE_CPUID
#define ENABLE_AMD64
#define ENABLE_SSE
#define ENABLE_SSE2
#define ENABLE_SSE3
#define ENABLE_SSE4
#define ENABLE_AVX
#define ENABLE_AVX2

#endif // arch
#endif // ENABLE_ASM
#if defined(ENABLE_TESTS) && defined(MODPLUG_NO_FILESAVE)
#undef MODPLUG_NO_FILESAVE // tests recommend file saving
#endif
#if defined(MPT_WITH_ZLIB) && defined(MPT_WITH_MINIZ)
#undef MPT_WITH_MINIZ
#endif
#if !MPT_OS_WINDOWS && defined(MPT_WITH_MEDIAFOUNDATION)
#undef MPT_WITH_MEDIAFOUNDATION // MediaFoundation requires Windows
#endif
#if !MPT_COMPILER_MSVC && !MPT_COMPILER_CLANG && defined(MPT_WITH_MEDIAFOUNDATION)
#undef MPT_WITH_MEDIAFOUNDATION // MediaFoundation requires MSVC or Clang due to ATL (no MinGW support)
#endif
#if (defined(MPT_WITH_MPG123) || defined(MPT_WITH_MINIMP3)) && !defined(MPT_ENABLE_MP3_SAMPLES)
#define MPT_ENABLE_MP3_SAMPLES
#endif
#if defined(ENABLE_TESTS)
#define MPT_ENABLE_FILEIO // Test suite requires PathString for file loading.
#endif
#if defined(MODPLUG_TRACKER) && !defined(MPT_ENABLE_FILEIO)
#define MPT_ENABLE_FILEIO // Tracker requires disk file io
#endif
#if defined(MPT_EXTERNAL_SAMPLES) && !defined(MPT_ENABLE_FILEIO)
#define MPT_ENABLE_FILEIO // External samples require disk file io
#endif
#if defined(NO_PLUGINS)
#define NO_VST
#endif
#if defined(MODPLUG_TRACKER) && !defined(MPT_BUILD_WINESUPPORT) && !defined(MPT_BUILD_WINESUPPORT_WRAPPER)
#ifndef MPT_NO_NAMESPACE
#define MPT_NO_NAMESPACE
#endif
#endif
#if defined(MPT_NO_NAMESPACE)

#ifdef OPENMPT_NAMESPACE
#undef OPENMPT_NAMESPACE
#endif
#define OPENMPT_NAMESPACE

#ifdef OPENMPT_NAMESPACE_BEGIN
#undef OPENMPT_NAMESPACE_BEGIN
#endif
#define OPENMPT_NAMESPACE_BEGIN

#ifdef OPENMPT_NAMESPACE_END
#undef OPENMPT_NAMESPACE_END
#endif
#define OPENMPT_NAMESPACE_END

#else
#ifndef OPENMPT_NAMESPACE
#define OPENMPT_NAMESPACE OpenMPT
#endif
#ifndef OPENMPT_NAMESPACE_BEGIN
#define OPENMPT_NAMESPACE_BEGIN namespace OPENMPT_NAMESPACE {
#endif
#ifndef OPENMPT_NAMESPACE_END
#define OPENMPT_NAMESPACE_END   }
#endif
#endif
#ifdef MPT_WITH_MFC
#define _CSTRING_DISABLE_NARROW_WIDE_CONVERSION
#endif // MPT_WITH_MFC
#if defined(MODPLUG_TRACKER)
#if MPT_OS_WINDOWS
#if !defined(MPT_BUILD_WINESUPPORT)
#ifndef MPT_MFC_FULL
#define _AFX_NO_MFC_CONTROLS_IN_DIALOGS	// Do not include support for MFC controls in dialogs (reduces binary bloat; remove this #define if you want to use MFC controls)
#endif // !MPT_MFC_FULL
#endif // !MPT_BUILD_WINESUPPORT
#endif // MPT_OS_WINDOWS
#endif // MODPLUG_TRACKER
#if MPT_OS_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMINMAX          // Macros min(a,b) and max(a,b)
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOPROFILER        // Profiler interface.
#define NOMCX             // Modem Configuration Extensions
#define MMNODRV
//#define MMNOWAVE
#define MMNOAUX
#define MMNOMIXER
#define MMNOJOY
#define MMNOMCI
//#define MMNOMMSYSTEM
#define NOMMIDS
#define NONEWRIFF
#define NOJPEGDIB
#define NONEWIC
#define NOBITMAP

#endif // MPT_OS_WINDOWS
#if MPT_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#endif
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#define __STDC_LIMIT_MACROS
#define _USE_MATH_DEFINES
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#if MPT_COMPILER_CLANG
#pragma clang diagnostic pop
#endif
#if MPT_COMPILER_MSVC

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#pragma warning(default:4800) // Implicit conversion from 'int' to bool. Possible information loss

#pragma warning(disable:4355) // 'this' : used in base member initializer list
#pragma warning(disable:4512) // assignment operator could not be generated

#pragma warning(error:4309) // Treat "truncation of constant value"-warning as error.
#pragma warning(error:4463) // Treat overflow; assigning value to bit-field that can only hold values from low_value to high_value"-warning as error.

#ifdef MPT_BUILD_ANALYZED
//#pragma warning(disable:6246)
#pragma warning(disable:6297) // 32-bit value is shifted, then cast to 64-bit value.  Results might not be an expected value.
#pragma warning(disable:6326) // Potential comparison of a constant with another constant
//#pragma warning(disable:6386)
#endif // MPT_BUILD_ANALYZED

#endif // MPT_COMPILER_MSVC
#if MPT_COMPILER_CLANG

#if defined(MPT_BUILD_MSVC)
#pragma clang diagnostic warning "-Wimplicit-fallthrough"
#endif // MPT_BUILD_MSVC

#if defined(MODPLUG_TRACKER)
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif // MODPLUG_TRACKER

#endif // MPT_COMPILER_CLANG
#if MPT_OS_WINDOWS
#ifndef UNICODE
#define MPT_CHECK_WINDOWS_IGNORE_WARNING_NO_UNICODE
#endif // !UNICODE
#endif // MPT_OS_WINDOWS
#ifdef MPT_WITH_ANCIENT
#ifdef MPT_BUILD_MSVC_SHARED
#define ANCIENT_API_DECLSPEC_DLLIMPORT
#endif
#endif
#ifdef MPT_WITH_FLAC
#ifdef MPT_BUILD_MSVC_STATIC
#define FLAC__NO_DLL
#endif
#endif
#ifdef MPT_WITH_SMBPITCHSHIFT
#ifdef MPT_BUILD_MSVC_SHARED
#define SMBPITCHSHIFT_USE_DLL
#endif
#endif
#ifdef MPT_WITH_STBVORBIS
#define STB_VORBIS_HEADER_ONLY
#ifndef STB_VORBIS_NO_PULLDATA_API
#define STB_VORBIS_NO_PULLDATA_API
#endif
#ifndef STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_STDIO
#endif
#endif
#ifdef MPT_WITH_VORBISFILE
#ifndef OV_EXCLUDE_STATIC_CALLBACKS
#define OV_EXCLUDE_STATIC_CALLBACKS
#endif
#endif
#ifdef MPT_WITH_ZLIB
#ifdef MPT_BUILD_MSVC_SHARED
#define ZLIB_DLL
#endif
#endif
#ifdef __cplusplus

#include "mpt/base/namespace.hpp"

OPENMPT_NAMESPACE_BEGIN

namespace mpt {

#ifndef MPT_NO_NAMESPACE
using namespace ::mpt;
#endif

} // namespace mpt

OPENMPT_NAMESPACE_END

#endif
