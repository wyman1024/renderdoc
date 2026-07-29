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

// win32_libentry.cpp : Defines the entry point for the DLL
#include <tchar.h>
#include <windows.h>
#include "common/common.h"
#include "core/core.h"
#include "hooks/hooks.h"
#include "strings/string_utils.h"

static BOOL add_hooks()
{
  wchar_t curFile[512];
  GetModuleFileNameW(NULL, curFile, 512);

  rdcstr f = get_basename(strlower(StringFormat::Wide2UTF8(curFile)));

  // bail immediately if we're in a system process. We don't want to hook, log, anything -
  // this instance is being used for a shell extension.
  if(f == "dllhost.exe" || f == "explorer.exe")
  {
#if ENABLED(RDOC_RELEASE)
    OutputDebugStringA(
        "Detecting shell process! Disabling hooking in dllhost.exe or explorer.exe\n");
#endif
    return TRUE;
  }

  // Search for an exported symbol with this name, typically rendercap__replay__marker.
  //
  // We deliberately also accept upstream RenderDoc's marker. Our vulkan layer json declares no
  // enable_environment, so the loader pulls this dll into *every* vulkan process on the machine -
  // including the replay/UI process of any other RenderDoc-derived debugger the user happens to
  // have installed. Those export renderdoc__replay__marker instead of ours, so if we only looked
  // for our own name we would mistake them for a capture target, wrap their replay device and
  // crash them. Recognising the upstream marker as well makes us stand down for the whole
  // RenderDoc family rather than only for our own build.
  // Both markers are resolved in a single pass - see DetectAny(). Calling Detect() twice walks the
  // module list twice while DllMain holds the loader lock, which deadlocked capture targets (they
  // never export the first marker, so the second walk always ran).
  if(LibraryHooks::DetectAny(STRINGIZE(RDOC_BASE_NAME) "__replay__marker",
                             "renderdoc__replay__marker"))
  {
    RDCDEBUG("Not creating hooks - in replay app");

    RenderDoc::Inst().SetReplayApp(true);

    RenderDoc::Inst().Initialise();

    LibraryHooks::ReplayInitialise();

    return true;
  }

  RenderDoc::Inst().Initialise();

  RDCLOG("Loading into %ls", curFile);
  
  // Check if target DLLs are already loaded before hook installation
  HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
  HMODULE dxgi = GetModuleHandleA("dxgi.dll");
  if(d3d11 || dxgi)
  {
    RDCLOG("[HOOK_DIAG] WARNING: Target DLLs already loaded before hook installation!");
    if(d3d11)
      RDCLOG("[HOOK_DIAG]   d3d11.dll loaded at 0x%p", d3d11);
    if(dxgi)
      RDCLOG("[HOOK_DIAG]   dxgi.dll loaded at 0x%p", dxgi);
    RDCLOG("[HOOK_DIAG]   This may indicate late hook timing - some API calls may have been missed");
  }
  else
  {
    RDCLOG("[HOOK_DIAG] Target DLLs not loaded yet - hook timing is good");
  }

  LibraryHooks::RegisterHooks();

  return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  if(ul_reason_for_call == DLL_PROCESS_ATTACH)
  {
    BOOL ret = add_hooks();
    SetLastError(0);
    return ret;
  }

  return TRUE;
}
