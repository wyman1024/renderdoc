/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2025 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

/////////////////////////////////////////////////
// Option macros
// From: http://www.codersnotes.com/notes/easy-preprocessor-defines/

#define OPTION_ON +
#define OPTION_OFF -
#define ENABLED(opt) ((1 opt 1) == 2)
#define DISABLED(opt) ((1 opt 1) == 0)

/////////////////////////////////////////////////
// Build/machine configuration
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(_M_X64) ||        \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__) || \
    (defined(__riscv64) && __riscv_xlen == 64)
#define RDOC_X64 OPTION_ON
#else
#define RDOC_X64 OPTION_OFF
#endif

#if defined(RELEASE) || defined(_RELEASE)
#define RDOC_RELEASE OPTION_ON
#define RDOC_DEVEL OPTION_OFF
#else
#define RDOC_RELEASE OPTION_OFF
#define RDOC_DEVEL OPTION_ON
#endif

#if defined(_MSC_VER)
#define RDOC_MSVS OPTION_ON
#else
#define RDOC_MSVS OPTION_OFF
#endif

// translate from build system defines, so they don't have to be defined to anything in
// particular
#if defined(RENDERDOC_PLATFORM_WIN32)

#define RDOC_WIN32 OPTION_ON
#define RDOC_ANDROID OPTION_OFF
#define RDOC_LINUX OPTION_OFF
#define RDOC_APPLE OPTION_OFF
#define RDOC_POSIX OPTION_OFF
#define RDOC_SWITCH OPTION_OFF

#elif defined(RENDERDOC_PLATFORM_ANDROID)

#define RDOC_WIN32 OPTION_OFF
#define RDOC_ANDROID OPTION_ON
#define RDOC_LINUX OPTION_OFF
#define RDOC_APPLE OPTION_OFF
#define RDOC_POSIX OPTION_ON
#define RDOC_SWITCH OPTION_OFF

#elif defined(RENDERDOC_PLATFORM_LINUX)

#define RDOC_WIN32 OPTION_OFF
#define RDOC_ANDROID OPTION_OFF
#define RDOC_LINUX OPTION_ON
#define RDOC_APPLE OPTION_OFF
#define RDOC_POSIX OPTION_ON
#define RDOC_SWITCH OPTION_OFF

#elif defined(RENDERDOC_PLATFORM_APPLE)

#define RDOC_WIN32 OPTION_OFF
#define RDOC_ANDROID OPTION_OFF
#define RDOC_LINUX OPTION_OFF
#define RDOC_APPLE OPTION_ON
#define RDOC_POSIX OPTION_ON
#define RDOC_SWITCH OPTION_OFF

#elif defined(RENDERDOC_PLATFORM_SWITCH)

#define RDOC_WIN32 OPTION_OFF
#define RDOC_ANDROID OPTION_OFF
#define RDOC_LINUX OPTION_OFF
#define RDOC_APPLE OPTION_OFF
#define RDOC_POSIX OPTION_ON
#define RDOC_SWITCH OPTION_ON

#else

#error "No platform configured in build system"

#endif

// is size_t a real separate type, not just typedef'd to uint32_t or uint64_t (or equivalent)?
#if defined(RENDERDOC_PLATFORM_APPLE)
#define RDOC_SIZET_SEP_TYPE OPTION_ON
#else
#define RDOC_SIZET_SEP_TYPE OPTION_OFF
#endif

#if defined(RENDERDOC_WINDOWING_XLIB)
#define RDOC_XLIB OPTION_ON
#else
#define RDOC_XLIB OPTION_OFF
#endif

#if defined(RENDERDOC_WINDOWING_XCB)
#define RDOC_XCB OPTION_ON
#else
#define RDOC_XCB OPTION_OFF
#endif

#if defined(RENDERDOC_WINDOWING_WAYLAND)
#define RDOC_WAYLAND OPTION_ON
#else
#define RDOC_WAYLAND OPTION_OFF
#endif

/////////////////////////////////////////////////
// Global constants
enum
{
  RenderDoc_FirstTargetControlPort = 38920,
  RenderDoc_LastTargetControlPort = RenderDoc_FirstTargetControlPort + 7,
  RenderDoc_RemoteServerPort = 39920,

  RenderDoc_ForwardPortBase = 38950,
  RenderDoc_ForwardTargetControlOffset = 0,
  RenderDoc_ForwardRemoteServerOffset = 9,
  RenderDoc_ForwardPortStride = 10,
};

// The layer name must satisfy two conflicting constraints:
//
//  1. It must contain "renderdoc". MuMu launches MuMuVMMHeadless.exe with
//     VK_LOADER_LAYERS_DISABLE=~implicit~ and VK_LOADER_LAYERS_ALLOW=*renderdoc*, so any layer whose
//     name does not contain that substring is filtered out by the loader. The match is a
//     case-insensitive substring test, so it does not have to be the exact upstream name.
//
//  2. It must NOT be exactly "VK_LAYER_RENDERDOC_Capture", which is what upstream RenderDoc uses.
//     The loader silently discards a layer whose name duplicates one it already loaded ("because it
//     is a duplicate of ..."), keeping whichever it found first. On any machine that also has
//     RenderDoc - or another rebranded fork - installed, sharing the name means one of the two is
//     dropped at random and capture mysteriously stops working.
//
// Keep this in sync with the "name" field in driver/vulkan/renderdoc.json.
#define RENDERDOC_VULKAN_LAYER_NAME "VK_LAYER_RENDERDOC_RenderCap_Capture"
#define RENDERDOC_VULKAN_LAYER_VAR "ENABLE_VULKAN_RENDERCAP_CAPTURE"

// The layer json deliberately declares no "enable_environment", so the implicit layer loads into
// every Vulkan process without the user having to set a machine-wide variable and reboot. That is
// required to capture processes we cannot launch ourselves - e.g. MuMu's MuMuVMMHeadless.exe, which
// is started by a service and therefore never inherits our environment.
//
// Because of that the replay app must exclude itself explicitly, otherwise the capture layer is
// loaded into our own replay device and asserts. Vulkan_CreateReplayDevice sets this variable on
// itself before creating its instance; the loader then skips the layer for that process only.
#define RENDERDOC_VULKAN_LAYER_DISABLE_VAR "DISABLE_VULKAN_RENDERCAP_CAPTURE"

#define RENDERDOC_ANDROID_LIBRARY "libVkLayer_GLES_RenderDoc.so"

// This MUST match the package name in the build process that generates per-architecture packages
#define RENDERDOC_ANDROID_PACKAGE_BASE "org.renderdoc.renderdoccmd"

/////////////////////////////////////////////////
// Debugging features configuration

// remove all logging code
#define STRIP_LOG OPTION_OFF

// remove all compile time asserts. Normally done even in release
// but this would speed up compilation
#define STRIP_COMPILE_ASSERTS OPTION_OFF

// force asserts regardless of debug/release mode
#define FORCE_ASSERTS OPTION_ON

// force debugbreaks regardless of debug/release mode
#define FORCE_DEBUGBREAK OPTION_OFF

/////////////////////////////////////////////////
// Logging configuration

// error logs trigger a breakpoint
#define DEBUGBREAK_ON_ERROR_LOG OPTION_ON

// whether to include timestamp on log lines
#define INCLUDE_TIMESTAMP_IN_LOG OPTION_ON

// whether to include file and line on log lines
#define INCLUDE_LOCATION_IN_LOG OPTION_ON

// logs go to stdout/stderr
#if ENABLED(RDOC_WIN32)

#define OUTPUT_LOG_TO_STDOUT OPTION_OFF
#define OUTPUT_LOG_TO_STDERR OPTION_OFF

#else

#define OUTPUT_LOG_TO_STDOUT OPTION_OFF
#define OUTPUT_LOG_TO_STDERR OPTION_OFF

#endif

// logs go to debug output (visual studio output window)
#define OUTPUT_LOG_TO_DEBUG_OUT OPTION_ON

// logs go to disk
#define OUTPUT_LOG_TO_DISK OPTION_ON

// normally only in a debug build do we
// include debug logs. This prints them all the time
#define FORCE_DEBUG_LOGS OPTION_ON
// this strips them completely
#define STRIP_DEBUG_LOGS OPTION_OFF

// disable unit tests on android and *BSD
#if ENABLED(RDOC_ANDROID) || defined(__FreeBSD__)

#define ENABLE_UNIT_TESTS OPTION_OFF

#else

// otherwise, enable them in development builds
#define ENABLE_UNIT_TESTS RDOC_DEVEL

#endif
