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

// must be separate so that it's included first and not sorted by clang-format
#include <windows.h>

#include <tlhelp32.h>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include "common/common.h"
#include "common/threading.h"
#include "core/settings.h"
#include "hooks/hooks.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

#define VERBOSE_DEBUG_HOOK OPTION_OFF

// map from address of IAT entry, to original contents
std::map<void **, void *> s_InstalledHooks;
Threading::CriticalSection installedLock;

// Forward declarations
static void ApplyExportTableHooks();

void Win32_ManualHookModule(rdcstr modName, HMODULE module);

  bool ApplyHook(FunctionHook &hook, void **IATentry, bool &already)
{
  DWORD oldProtection = PAGE_EXECUTE;

  if(*IATentry == hook.hook)
  {
    already = true;
    return true;
  }

#if ENABLED(VERBOSE_DEBUG_HOOK)
  RDCDEBUG("Patching IAT for %s: %p to %p", hook.function.c_str(), IATentry, hook.hook);
#endif

  {
    SCOPED_LOCK(installedLock);
    if(s_InstalledHooks.find(IATentry) == s_InstalledHooks.end())
      s_InstalledHooks[IATentry] = *IATentry;
  }

  BOOL success = VirtualProtect(IATentry, sizeof(void *), PAGE_READWRITE, &oldProtection);
  if(!success)
  {
    RDCERR("Failed to make IAT entry writeable 0x%p", IATentry);
    return false;
  }

  *IATentry = hook.hook;

  success = VirtualProtect(IATentry, sizeof(void *), oldProtection, &oldProtection);
  if(!success)
  {
    RDCERR("Failed to restore IAT entry protection 0x%p", IATentry);
    return false;
  }

  return true;
}

struct DllHookset
{
  HMODULE module = NULL;
  bool hooksfetched = false;
  // if we have multiple copies of the dll loaded (unlikely), the other module handles will be
  // stored here
  rdcarray<HMODULE> altmodules;
  rdcarray<FunctionHook> FunctionHooks;
  DWORD OrdinalBase = 0;
  rdcarray<rdcstr> OrdinalNames;
  rdcarray<FunctionLoadCallback> Callbacks;
  Threading::CriticalSection ordinallock;

  void FetchOrdinalNames()
  {
    SCOPED_LOCK(ordinallock);

    // return if we already fetched the ordinals
    if(!OrdinalNames.empty())
      return;

    byte *baseAddress = (byte *)module;

#if ENABLED(VERBOSE_DEBUG_HOOK)
    RDCDEBUG("FetchOrdinalNames");
#endif

    PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER)baseAddress;

    if(dosheader->e_magic != 0x5a4d)
      return;

    char *PE00 = (char *)(baseAddress + dosheader->e_lfanew);
    PIMAGE_FILE_HEADER fileHeader = (PIMAGE_FILE_HEADER)(PE00 + 4);
    PIMAGE_OPTIONAL_HEADER optHeader =
        (PIMAGE_OPTIONAL_HEADER)((BYTE *)fileHeader + sizeof(IMAGE_FILE_HEADER));

    DWORD eatOffset = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

    IMAGE_EXPORT_DIRECTORY *exportDesc = (IMAGE_EXPORT_DIRECTORY *)(baseAddress + eatOffset);

    WORD *ordinals = (WORD *)(baseAddress + exportDesc->AddressOfNameOrdinals);
    DWORD *names = (DWORD *)(baseAddress + exportDesc->AddressOfNames);

    DWORD count = RDCMIN(exportDesc->NumberOfFunctions, exportDesc->NumberOfNames);

    WORD maxOrdinal = 0;
    for(DWORD i = 0; i < count; i++)
      maxOrdinal = RDCMAX(maxOrdinal, ordinals[i]);

    OrdinalBase = exportDesc->Base;
    OrdinalNames.resize(maxOrdinal + 1);

    for(DWORD i = 0; i < count; i++)
    {
      OrdinalNames[ordinals[i]] = (char *)(baseAddress + names[i]);

#if ENABLED(VERBOSE_DEBUG_HOOK)
      RDCDEBUG("ordinal found: '%s' %u", OrdinalNames[ordinals[i]].c_str(), (uint32_t)ordinals[i]);
#endif
    }
  }
};

struct CachedHookData
{
  bool hookAll = true;

  std::map<rdcstr, DllHookset> DllHooks;
  HMODULE ownmodule = NULL;
  Threading::CriticalSection lock;
  char lowername[512] = {};

  std::set<rdcstr> ignores;

  bool missedOrdinals = false;
  std::function<HMODULE(const rdcstr &, HANDLE, DWORD)> libraryIntercept;

  int32_t posthooking = 0;

  void ApplyHooks(const char *modName, HMODULE module)
  {
    {
      size_t i = 0;
      while(modName[i])
      {
        lowername[i] = (char)tolower(modName[i]);
        i++;
      }
      lowername[i] = 0;
    }

    // Diagnostic logging for game executable and D3D12/DXGI modules
    if(strstr(lowername, ".exe") || strstr(lowername, "d3d12") || strstr(lowername, "dxgi"))
    {
      RDCLOG("[IAT_HOOK] Scanning module: %s (handle=0x%p)", modName, module);
    }
    
    // Apply EAT hooks when D3D12 or D3D11 DLL is detected
    static bool s_d3d12_eat_hooked = false;
    static bool s_d3d11_eat_hooked = false;
    
    if(!s_d3d12_eat_hooked && strstr(lowername, "d3d12.dll"))
    {
      s_d3d12_eat_hooked = true;
      RDCLOG("[EAT_HOOK] d3d12.dll detected in ApplyHooks, applying EAT hooks now");
      ApplyExportTableHooks();
    }
    else if(!s_d3d11_eat_hooked && strstr(lowername, "d3d11.dll"))
    {
      s_d3d11_eat_hooked = true;
      RDCLOG("[EAT_HOOK] d3d11.dll detected in ApplyHooks, applying EAT hooks now");
      ApplyExportTableHooks();
    }
    else if(strstr(lowername, "dxgi.dll"))
    {
      // Check if d3d11.dll is already loaded but EAT hooks haven't been applied yet
      HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
      if(d3d11 && !s_d3d11_eat_hooked)
      {
        s_d3d11_eat_hooked = true;
        ApplyExportTableHooks();
      }
    }

#if ENABLED(VERBOSE_DEBUG_HOOK)
    RDCDEBUG("=== ApplyHooks(%s, %p)", modName, module);
#endif

    // fraps seems to non-safely modify the assembly around the hook function, if
    // we modify its import descriptors it leads to a crash as it hooks OUR functions.
    // instead, skip modifying the import descriptors, it will hook the 'real' d3d functions
    // and we can call them and have fraps + renderdoc playing nicely together.
    // we also exclude some other overlay renderers here, such as steam's
    //
    // Also we exclude ourselves here - just in case the application has already loaded
    // renderdoc.dll, or tries to load it.
    if(strstr(lowername, "fraps") || strstr(lowername, "gameoverlayrenderer") ||
       strstr(lowername, STRINGIZE(RDOC_BASE_NAME) ".dll") == lowername)
      return;

    // set module pointer if we are hooking exports from this module
    for(auto it = DllHooks.begin(); it != DllHooks.end(); ++it)
    {
      if(!_stricmp(it->first.c_str(), modName))
      {
        if(it->second.module == NULL)
        {
          it->second.module = module;

          it->second.hooksfetched = true;

          // fetch all function hooks here, since we want to fill out the original function pointer
          // even in case nothing imports from that function (which means it would not get filled
          // out through FunctionHook::ApplyHook)
          for(FunctionHook &hook : it->second.FunctionHooks)
          {
            if(hook.orig && *hook.orig == NULL)
              *hook.orig = GetProcAddress(module, hook.function.c_str());
          }

          it->second.FetchOrdinalNames();
        }
        else if(it->second.module != module)
        {
          // if it's already in altmodules, bail
          bool already = false;

          for(size_t i = 0; i < it->second.altmodules.size(); i++)
          {
            if(it->second.altmodules[i] == module)
            {
              already = true;
              break;
            }
          }

          if(already)
            break;

          // check if the previous module is still valid
          SetLastError(0);
          char filename[MAX_PATH] = {};
          GetModuleFileNameA(it->second.module, filename, MAX_PATH - 1);
          DWORD err = GetLastError();
          char *slash = strrchr(filename, L'\\');

          rdcstr basename = slash ? strlower(rdcstr(slash + 1)) : "";

          if(err == 0 && basename == it->first)
          {
            // previous module is still loaded, add this to the alt modules list
            it->second.altmodules.push_back(module);
          }
          else
          {
            // previous module is no longer loaded or there's a new file there now, add this as the
            // new location
            RDCWARN("%s moved from %p to %p, re-initialising orig pointers", it->first.c_str(),
                    it->second.module, module);

            // we also need to re-initialise the hooks as the orig pointers are now stale
            for(FunctionHook &hook : it->second.FunctionHooks)
            {
              if(hook.orig)
                *hook.orig = GetProcAddress(module, hook.function.c_str());
            }

            it->second.module = module;
          }
        }
      }
    }

    // for safety (and because we don't need to), ignore these modules
    if(!_stricmp(modName, "kernel32.dll") || !_stricmp(modName, "powrprof.dll") ||
       !_stricmp(modName, "CoreMessaging.dll") || !_stricmp(modName, "opengl32.dll") ||
       !_stricmp(modName, "gdi32.dll") || !_stricmp(modName, "gdi32full.dll") ||
       !_stricmp(modName, "windows.storage.dll") || !_stricmp(modName, "nvoglv32.dll") ||
       !_stricmp(modName, "nvoglv64.dll") || !_stricmp(modName, "vulkan-1.dll") ||
       !_stricmp(modName, "atio6axx.dll") || !_stricmp(modName, "atioglxx.dll") ||
       !_stricmp(modName, "nvcuda.dll") || strstr(lowername, "cudart") == lowername ||
       strstr(lowername, "msvcr") == lowername || strstr(lowername, "msvcp") == lowername ||
       strstr(lowername, "nv-vk") == lowername || strstr(lowername, "amdvlk") == lowername ||
       strstr(lowername, "igvk") == lowername || strstr(lowername, "nvopencl") == lowername ||
       strstr(lowername, "nvapi") == lowername)
    {
      return;
    }

    if(ignores.find(lowername) != ignores.end())
    {
      return;
    }

    // the module could have been unloaded after our toolhelp snapshot, especially if we spent a
    // long time
    // dealing with a previous module (like adding our hooks).
    wchar_t modpath[1024] = {0};
    GetModuleFileNameW(module, modpath, 1023);
    if(modpath[0] == 0)
    {
      return;
    }

    // windows 11 and newer versions have weird hotpatch DLLs that don't act like real DLLs. The
    // LoadLibraryW below will fail for these DLLs even when using the module path provided.
    // Only check the path for DLLs that might be a windows-hotpatch but if it matches we'll skip
    // hooking these to avoid problems
    if(strstr(lowername, "hotpatch"))
    {
      wchar_t lowerpath[1024] = {};

      size_t i = 0;
      while(modpath[i])
      {
        lowerpath[i] = towlower(modpath[i]);
        i++;
      }
      lowerpath[i] = 0;

      if(wcsstr(lowerpath, L"\\windows\\winsxs\\"))
        return;
    }

    // increment the module reference count, so it doesn't disappear while we're processing it
    // there's a very small race condition here between if GetModuleFileName returns, the module is
    // unloaded then we load it again. The only way around that is inserting very scary locks
    // between here
    // and FreeLibrary that I want to avoid. Worst case, we load a dll, hook it, then unload it
    // again.
    HMODULE refcountModHandle = LoadLibraryW(modpath);
    RDCASSERTEQUAL(refcountModHandle, module);
    byte *baseAddress = (byte *)refcountModHandle;

    PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER)baseAddress;

    if(dosheader->e_magic != 0x5a4d)
    {
      RDCDEBUG("Ignoring module %s, since magic is 0x%04x not 0x%04x", modName,
               (uint32_t)dosheader->e_magic, 0x5a4dU);
      FreeLibrary(refcountModHandle);
      return;
    }

    char *PE00 = (char *)(baseAddress + dosheader->e_lfanew);
    PIMAGE_FILE_HEADER fileHeader = (PIMAGE_FILE_HEADER)(PE00 + 4);
    PIMAGE_OPTIONAL_HEADER optHeader =
        (PIMAGE_OPTIONAL_HEADER)((BYTE *)fileHeader + sizeof(IMAGE_FILE_HEADER));

    DWORD iatOffset = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

    IMAGE_IMPORT_DESCRIPTOR *importDesc = (IMAGE_IMPORT_DESCRIPTOR *)(baseAddress + iatOffset);

#if ENABLED(VERBOSE_DEBUG_HOOK)
    RDCDEBUG("=== import descriptors:");
#endif

    while(iatOffset && importDesc->FirstThunk)
    {
      const char *dllName = (const char *)(baseAddress + importDesc->Name);

#if ENABLED(VERBOSE_DEBUG_HOOK)
      RDCDEBUG("found IAT for %s", dllName);
#endif

      DllHookset *hookset = NULL;

      for(auto it = DllHooks.begin(); it != DllHooks.end(); ++it)
        if(!_stricmp(it->first.c_str(), dllName))
          hookset = &it->second;

      if(hookset && importDesc->OriginalFirstThunk > 0)
      {
        IMAGE_THUNK_DATA *origFirst =
            (IMAGE_THUNK_DATA *)(baseAddress + importDesc->OriginalFirstThunk);
        IMAGE_THUNK_DATA *first = (IMAGE_THUNK_DATA *)(baseAddress + importDesc->FirstThunk);

#if ENABLED(VERBOSE_DEBUG_HOOK)
        RDCDEBUG("Hooking imports for %s", dllName);
#endif

        while(origFirst->u1.AddressOfData)
        {
          void **IATentry = (void **)&first->u1.AddressOfData;

          struct hook_find
          {
            bool operator()(const FunctionHook &a, const char *b)
            {
              return strcmp(a.function.c_str(), b) < 0;
            }
          };

#if ENABLED(RDOC_X64)
          if(IMAGE_SNAP_BY_ORDINAL64(origFirst->u1.AddressOfData))
#else
          if(IMAGE_SNAP_BY_ORDINAL32(origFirst->u1.AddressOfData))
#endif
          {
            // low bits of origFirst->u1.AddressOfData contain an ordinal
            WORD ordinal = IMAGE_ORDINAL64(origFirst->u1.AddressOfData);

#if ENABLED(VERBOSE_DEBUG_HOOK)
            RDCDEBUG("Found ordinal import %u", (uint32_t)ordinal);
#endif

            if(!hookset->OrdinalNames.empty())
            {
              if(ordinal >= hookset->OrdinalBase)
              {
                // rebase into OrdinalNames index
                DWORD nameIndex = ordinal - hookset->OrdinalBase;

                // it's perfectly valid to have more functions than names, we only
                // list those with names - so ignore any others
                if(nameIndex < hookset->OrdinalNames.size())
                {
                  const char *importName = (const char *)hookset->OrdinalNames[nameIndex].c_str();

#if ENABLED(VERBOSE_DEBUG_HOOK)
                  RDCDEBUG("Located ordinal %u as %s", (uint32_t)ordinal, importName);
#endif

                  auto found =
                      std::lower_bound(hookset->FunctionHooks.begin(), hookset->FunctionHooks.end(),
                                       importName, hook_find());

                  if(found != hookset->FunctionHooks.end() &&
                     !strcmp(found->function.c_str(), importName) && ownmodule != module)
                  {
                    bool already = false;
                    bool applied;
                    {
                      SCOPED_LOCK(lock);
                      applied = ApplyHook(*found, IATentry, already);
                    }

                    // if we failed, or if it's already set and we're not doing a missedOrdinals
                    // second pass, then just bail out immediately as we've already hooked this
                    // module and there's no point wasting time re-hooking nothing
                    if(!applied || (already && !missedOrdinals))
                    {
#if ENABLED(VERBOSE_DEBUG_HOOK)
                      RDCDEBUG("Stopping hooking module, %d %d %d", (int)applied, (int)already,
                               (int)missedOrdinals);
#endif
                      FreeLibrary(refcountModHandle);
                      return;
                    }
                  }
                }
              }
              else
              {
                RDCERR("Import ordinal is below ordinal base in %s importing module %s", modName,
                       dllName);
              }
            }
            else
            {
#if ENABLED(VERBOSE_DEBUG_HOOK)
              RDCDEBUG("missed ordinals, will try again");
#endif
              // the very first time we try to apply hooks, we might apply them to a module
              // before we've looked up the ordinal names for the one it's linking against.
              // Subsequent times we're only loading one new module - and since it can't
              // link to itself we will have all ordinal names loaded.
              //
              // Setting this flag causes us to do a second pass right at the start
              missedOrdinals = true;
            }

            // continue
            origFirst++;
            first++;
            continue;
          }

          IMAGE_IMPORT_BY_NAME *import =
              (IMAGE_IMPORT_BY_NAME *)(baseAddress + origFirst->u1.AddressOfData);

          const char *importName = (const char *)import->Name;

#if ENABLED(VERBOSE_DEBUG_HOOK)
          RDCDEBUG("Found normal import %s", importName);
#endif

          // Diagnostic logging for D3D12 imports
          if(strstr(importName, "D3D12") || strstr(importName, "DXGI"))
          {
            RDCLOG("[IAT_HOOK] Found import: %s from %s in module %s", importName, dllName, modName);
          }

          auto found = std::lower_bound(hookset->FunctionHooks.begin(),
                                        hookset->FunctionHooks.end(), importName, hook_find());

          if(found != hookset->FunctionHooks.end() &&
             !strcmp(found->function.c_str(), importName) && ownmodule != module)
          {
            // Diagnostic logging for D3D12 hook application
            if(strstr(importName, "D3D12") || strstr(importName, "DXGI"))
            {
              RDCLOG("[IAT_HOOK] Attempting to hook: %s", importName);
            }
            
            bool already = false;
            bool applied;
            {
              SCOPED_LOCK(lock);
              applied = ApplyHook(*found, IATentry, already);
            }
            
            // Diagnostic logging for hook result
            if(strstr(importName, "D3D12") || strstr(importName, "DXGI"))
            {
              RDCLOG("[IAT_HOOK] Hook result for %s: applied=%d, already=%d", importName, applied, already);
            }

            // if we failed, or if it's already set and we're not doing a missedOrdinals
            // second pass, then just bail out immediately as we've already hooked this
            // module and there's no point wasting time re-hooking nothing
            if(!applied || (already && !missedOrdinals))
            {
#if ENABLED(VERBOSE_DEBUG_HOOK)
              RDCDEBUG("Stopping hooking module, %d %d %d", (int)applied, (int)already,
                       (int)missedOrdinals);
#endif
              FreeLibrary(refcountModHandle);
              return;
            }
          }

          origFirst++;
          first++;
        }
      }
      else
      {
        if(hookset)
        {
#if ENABLED(VERBOSE_DEBUG_HOOK)
          RDCDEBUG("!! Invalid IAT found for %s! %u %u", dllName, importDesc->OriginalFirstThunk,
                   importDesc->FirstThunk);
#endif
        }
      }

      importDesc++;
    }

    FreeLibrary(refcountModHandle);
  }
};

static CachedHookData *s_HookData = NULL;

#ifdef UNICODE
#undef MODULEENTRY32
#undef Module32First
#undef Module32Next
#endif

static void ForAllModules(std::function<void(const MODULEENTRY32 &me32)> callback)
{
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE;

  // up to 10 retries
  for(int i = 0; i < 10; i++)
  {
    hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());

    if(hModuleSnap == INVALID_HANDLE_VALUE)
    {
      DWORD err = GetLastError();

      RDCWARN("CreateToolhelp32Snapshot() -> 0x%08x", err);

      // retry if error is ERROR_BAD_LENGTH
      if(err == ERROR_BAD_LENGTH)
        continue;
    }

    // didn't retry, or succeeded
    break;
  }

  if(hModuleSnap == INVALID_HANDLE_VALUE)
  {
    RDCERR("Couldn't create toolhelp dump of modules in process");
    return;
  }

  MODULEENTRY32 me32;
  RDCEraseEl(me32);
  me32.dwSize = sizeof(MODULEENTRY32);

  BOOL success = Module32First(hModuleSnap, &me32);

  if(success == FALSE)
  {
    DWORD err = GetLastError();

    RDCERR("Couldn't get first module in process: 0x%08x", err);
    CloseHandle(hModuleSnap);
    return;
  }

  do
  {
    callback(me32);
  } while(Module32Next(hModuleSnap, &me32));

  CloseHandle(hModuleSnap);
}

static void HookAllModules()
{
  if(!s_HookData->hookAll)
    return;

  ForAllModules(
      [](const MODULEENTRY32 &me32) { s_HookData->ApplyHooks(me32.szModule, me32.hModule); });

  // check if we're already in this section of code, and if so don't go in again.
  int32_t prev = Atomic::CmpExch32(&s_HookData->posthooking, 0, 1);

  if(prev != 0)
    return;

  // for all loaded modules, call callbacks now
  for(auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
  {
    if(it->second.module == NULL)
      continue;

    if(!it->second.hooksfetched)
    {
      it->second.hooksfetched = true;

      // fetch all function hooks here, if we didn't above (perhaps because this library was
      // late-loaded)
      for(FunctionHook &hook : it->second.FunctionHooks)
      {
        if(hook.orig && *hook.orig == NULL)
          *hook.orig = GetProcAddress(it->second.module, hook.function.c_str());
      }
    }

    rdcarray<FunctionLoadCallback> callbacks;
    // don't call callbacks next time
    callbacks.swap(it->second.Callbacks);

    for(FunctionLoadCallback cb : callbacks)
      if(cb)
        cb(it->second.module, it->first.c_str());
  }

  Atomic::CmpExch32(&s_HookData->posthooking, 1, 0);
}

static bool IsAPISet(const wchar_t *filename)
{
  if(wcschr(filename, L'/') != 0 || wcschr(filename, L'\\') != 0)
    return false;

  wchar_t match[] = L"api-ms-win";

  if(wcslen(filename) < ARRAY_COUNT(match) - 1)
    return false;

  for(size_t i = 0; i < ARRAY_COUNT(match) - 1; i++)
    if(towlower(filename[i]) != match[i])
      return false;

  return true;
}

static bool IsAPISet(const char *filename)
{
  size_t len = strlen(filename);
  rdcwstr wfn(len);

  // assume ASCII not UTF, just upcast plainly to wchar_t
  for(size_t i = 0; i < len; i++)
    wfn[i] = wchar_t(filename[i]);

  return IsAPISet(wfn.c_str());
}

HMODULE WINAPI Hooked_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE fileHandle, DWORD flags)
{
  bool dohook = true;

  if(s_HookData->libraryIntercept)
  {
    HMODULE ret = s_HookData->libraryIntercept(lpLibFileName, fileHandle, flags);
    if(ret)
      return ret;
    dohook = false;
  }

  if(flags == 0 && GetModuleHandleA(lpLibFileName))
    dohook = false;

  SetLastError(S_OK);

  // we can use the function naked, as when setting up the hook for LoadLibraryExA, our own module
  // was excluded from IAT patching
  HMODULE mod = LoadLibraryExA(lpLibFileName, fileHandle, flags);

#if ENABLED(VERBOSE_DEBUG_HOOK)
  RDCDEBUG("LoadLibraryA(%s)", lpLibFileName);
#endif

  DWORD err = GetLastError();

  if(dohook && mod && !IsAPISet(lpLibFileName))
  {
    HookAllModules();
    
    // If this is d3d12.dll, d3d11.dll, or dxgi.dll, apply EAT hooks immediately
    if(lpLibFileName)
    {
      rdcstr libName = strlower(rdcstr(lpLibFileName));
      if(libName.contains("d3d12.dll") || libName.contains("d3d11.dll") || libName.contains("dxgi.dll"))
      {
        RDCLOG("[EAT_HOOK] Graphics DLL loaded dynamically: %s, applying EAT hooks", lpLibFileName);
        ApplyExportTableHooks();
      }
    }
  }

  SetLastError(err);

  return mod;
}

HMODULE WINAPI Hooked_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE fileHandle, DWORD flags)
{
  bool dohook = true;

  if(s_HookData->libraryIntercept)
  {
    HMODULE ret =
        s_HookData->libraryIntercept(StringFormat::Wide2UTF8(lpLibFileName), fileHandle, flags);
    if(ret)
      return ret;
    dohook = false;
  }

  DWORD flagsExcludingSearchOrders = flags;

  // if this is a pure "filename.dll" load, don't care about search-order flags since loaded DLLs are
  // always returned first regardless of the search order and so we can detect the DLL is already loaded
  if(wcschr(lpLibFileName, L'\\') == 0 && wcschr(lpLibFileName, L'/') == 0)
  {
    flagsExcludingSearchOrders &= ~(LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 |
                                    LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_WITH_ALTERED_SEARCH_PATH);

#ifdef LOAD_LIBRARY_SAFE_CURRENT_DIRS
    flagsExcludingSearchOrders &= ~LOAD_LIBRARY_SAFE_CURRENT_DIRS;
#endif
  }

  // if there are no flags (possibly with search path flags excluded) and we already have the
  // library loaded, don't hook anything
  if(flagsExcludingSearchOrders == 0 && GetModuleHandleW(lpLibFileName))
    dohook = false;

  if(flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE))
    dohook = false;

  SetLastError(S_OK);

#if ENABLED(VERBOSE_DEBUG_HOOK)
  RDCDEBUG("LoadLibraryW(%ls)", lpLibFileName);
#endif

  rdcstr utf8Name = StringFormat::Wide2UTF8(lpLibFileName);

  // we can use the function naked, as when setting up the hook for LoadLibraryExA, our own module
  // was excluded from IAT patching
  HMODULE mod = LoadLibraryExW(lpLibFileName, fileHandle, flags);

  DWORD err = GetLastError();

  if(dohook && mod && !IsAPISet(lpLibFileName))
  {
    HookAllModules();
    
    // Apply EAT hooks if this is a D3D or DXGI DLL
    rdcstr libName = strlower(utf8Name);
    if(libName.contains("d3d12.dll") || libName.contains("d3d11.dll") || libName.contains("dxgi.dll"))
    {
      RDCLOG("[EAT_HOOK] Applying EAT hooks to newly loaded module: %s", utf8Name.c_str());
      ApplyExportTableHooks();
    }
  }

  SetLastError(err);

  return mod;
}

HMODULE WINAPI Hooked_LoadLibraryA(LPCSTR lpLibFileName)
{
  return Hooked_LoadLibraryExA(lpLibFileName, NULL, 0);
}

HMODULE WINAPI Hooked_LoadLibraryW(LPCWSTR lpLibFileName)
{
  return Hooked_LoadLibraryExW(lpLibFileName, NULL, 0);
}

static bool OrdinalAsString(void *func)
{
  return uint64_t(func) <= 0xffff;
}

FARPROC WINAPI Hooked_GetProcAddress(HMODULE mod, LPCSTR func)
{
  if(mod == NULL || func == NULL || mod == s_HookData->ownmodule)
    return GetProcAddress(mod, func);

#if ENABLED(VERBOSE_DEBUG_HOOK)
  if(OrdinalAsString((void *)func))
    RDCDEBUG("Hooked_GetProcAddress(%p, %p)", mod, func);
  else
    RDCDEBUG("Hooked_GetProcAddress(%p, %s)", mod, func);
#endif

  for(auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
  {
    if(it->second.module == NULL)
    {
      it->second.module = GetModuleHandleA(it->first.c_str());
      if(it->second.module)
      {
        // fetch all function hooks here, since we want to fill out the original function pointer
        // even in case nothing imports from that function (which means it would not get filled
        // out through FunctionHook::ApplyHook)
        for(FunctionHook &hook : it->second.FunctionHooks)
        {
          if(hook.orig && *hook.orig == NULL)
            *hook.orig = GetProcAddress(it->second.module, hook.function.c_str());
        }

        it->second.FetchOrdinalNames();
      }
    }

    bool match = (mod == it->second.module);

    if(!match && !it->second.altmodules.empty())
    {
      for(size_t i = 0; !match && i < it->second.altmodules.size(); i++)
        match = (mod == it->second.altmodules[i]);
    }

    if(match)
    {
#if ENABLED(VERBOSE_DEBUG_HOOK)
      RDCDEBUG("Located module %s", it->first.c_str());
#endif

      if(OrdinalAsString((void *)func))
      {
#if ENABLED(VERBOSE_DEBUG_HOOK)
        RDCDEBUG("Ordinal hook");
#endif

        uint32_t ordinal = (uint16_t)(uintptr_t(func) & 0xffff);

        if(ordinal < it->second.OrdinalBase)
        {
          RDCERR("Unexpected ordinal - lower than ordinalbase %u for %s",
                 (uint32_t)it->second.OrdinalBase, it->first.c_str());

          SetLastError(S_OK);
          return GetProcAddress(mod, func);
        }

        ordinal -= it->second.OrdinalBase;

        if(ordinal >= it->second.OrdinalNames.size())
        {
          RDCERR("Unexpected ordinal - higher than fetched ordinal names (%u) for %s",
                 (uint32_t)it->second.OrdinalNames.size(), it->first.c_str());

          SetLastError(S_OK);
          return GetProcAddress(mod, func);
        }

        func = it->second.OrdinalNames[ordinal].c_str();

#if ENABLED(VERBOSE_DEBUG_HOOK)
        RDCDEBUG("found ordinal %s", func);
#endif
      }

      FunctionHook search(func, NULL, NULL);

      auto found =
          std::lower_bound(it->second.FunctionHooks.begin(), it->second.FunctionHooks.end(), search);
      if(found != it->second.FunctionHooks.end() && !(search < *found))
      {
        FARPROC realfunc = GetProcAddress(mod, func);

        // Diagnostic logging for D3D12/DXGI hooked functions
        if(!OrdinalAsString((void *)func))
        {
          const char *funcName = (const char *)func;
          if(strstr(funcName, "D3D12") || strstr(funcName, "DXGI") || strstr(funcName, "CreateDevice"))
          {
            RDCLOG("[GetProcAddress] ✓ Returning HOOKED function: %s (hook=0x%p, real=0x%p)", 
                   funcName, found->hook, realfunc);
          }
        }

#if ENABLED(VERBOSE_DEBUG_HOOK)
        RDCDEBUG("Found hooked function, returning hook pointer %p", found->hook);
#endif

        SetLastError(S_OK);

        if(realfunc == NULL)
          return NULL;

        return (FARPROC)found->hook;
      }
    }
  }

#if ENABLED(VERBOSE_DEBUG_HOOK)
  RDCDEBUG("No matching hook found, returning original");
#endif

  SetLastError(S_OK);

  return GetProcAddress(mod, func);
}
static void InitHookData()
{
  if(!s_HookData)
  {
    s_HookData = new CachedHookData;

    RDCASSERT(s_HookData->DllHooks.empty());
    s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
        FunctionHook("LoadLibraryA", NULL, &Hooked_LoadLibraryA));
    s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
        FunctionHook("LoadLibraryW", NULL, &Hooked_LoadLibraryW));
    s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
        FunctionHook("LoadLibraryExA", NULL, &Hooked_LoadLibraryExA));
    s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
        FunctionHook("LoadLibraryExW", NULL, &Hooked_LoadLibraryExW));
    s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
        FunctionHook("GetProcAddress", NULL, &Hooked_GetProcAddress));

    for(const char *apiset :
        {"api-ms-win-core-libraryloader-l1-1-0.dll", "api-ms-win-core-libraryloader-l1-1-1.dll",
         "api-ms-win-core-libraryloader-l1-1-2.dll", "api-ms-win-core-libraryloader-l1-2-0.dll",
         "api-ms-win-core-libraryloader-l1-2-1.dll"})
    {
      s_HookData->DllHooks[apiset].FunctionHooks.push_back(
          FunctionHook("LoadLibraryA", NULL, &Hooked_LoadLibraryA));
      s_HookData->DllHooks[apiset].FunctionHooks.push_back(
          FunctionHook("LoadLibraryW", NULL, &Hooked_LoadLibraryW));
      s_HookData->DllHooks[apiset].FunctionHooks.push_back(
          FunctionHook("LoadLibraryExA", NULL, &Hooked_LoadLibraryExA));
      s_HookData->DllHooks[apiset].FunctionHooks.push_back(
          FunctionHook("LoadLibraryExW", NULL, &Hooked_LoadLibraryExW));
      s_HookData->DllHooks[apiset].FunctionHooks.push_back(
          FunctionHook("GetProcAddress", NULL, &Hooked_GetProcAddress));
    }

    GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCTSTR)&s_HookData, &s_HookData->ownmodule);
  }
}

void LibraryHooks::RegisterFunctionHook(const char *libraryName, const FunctionHook &hook)
{
  if(!_stricmp(libraryName, "kernel32.dll"))
  {
    if(hook.function == "LoadLibraryA" || hook.function == "LoadLibraryW" ||
       hook.function == "LoadLibraryExA" || hook.function == "LoadLibraryExW" ||
       hook.function == "GetProcAddress")
    {
      RDCERR("Cannot hook LoadLibrary* or GetProcAddress, as these are hooked internally");
      return;
    }
  }
  s_HookData->DllHooks[strlower(rdcstr(libraryName))].FunctionHooks.push_back(hook);
}

void LibraryHooks::RegisterLibraryHook(const char *libraryName, FunctionLoadCallback loadedCallback)
{
  s_HookData->DllHooks[strlower(rdcstr(libraryName))].Callbacks.push_back(loadedCallback);
}

void LibraryHooks::IgnoreLibrary(const char *libraryName)
{
  rdcstr lowername = libraryName;

  for(size_t i = 0; i < lowername.size(); i++)
    lowername[i] = (char)tolower(lowername[i]);

  s_HookData->ignores.insert(lowername);
}

void LibraryHooks::BeginHookRegistration()
{
  InitHookData();
}

// hook all functions for currently loaded modules.
// some of these hooks (as above) will hook LoadLibrary/GetProcAddress, to protect
void LibraryHooks::EndHookRegistration()
{
  for(auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
    std::sort(it->second.FunctionHooks.begin(), it->second.FunctionHooks.end());

#if ENABLED(VERBOSE_DEBUG_HOOK)
  RDCDEBUG("Applying hooks");
#endif

  HookAllModules();

  if(s_HookData->missedOrdinals)
  {
#if ENABLED(VERBOSE_DEBUG_HOOK)
    RDCDEBUG("Missed ordinals - applying hooks again");
#endif

    // we need to do a second pass now that we know ordinal names to finally hook
    // some imports by ordinal only.
    HookAllModules();

    s_HookData->missedOrdinals = false;
  }

  // Apply Export Address Table hooks for D3D11/D3D12
  // This catches calls that bypass IAT (e.g., direct GetProcAddress before our hooks are active)
  ApplyExportTableHooks();
}

void LibraryHooks::Refresh()
{
  // don't need to refresh on windows
}

void LibraryHooks::ReplayInitialise()
{
}

void LibraryHooks::RemoveHooks()
{
  LibraryHooks::RemoveHookCallbacks();

  for(auto it = s_InstalledHooks.begin(); it != s_InstalledHooks.end(); ++it)
  {
    DWORD oldProtection = PAGE_EXECUTE;

    void **IATentry = it->first;

    BOOL success = VirtualProtect(IATentry, sizeof(void *), PAGE_READWRITE, &oldProtection);
    if(!success)
    {
      RDCERR("Failed to make IAT entry writeable 0x%p", IATentry);
      continue;
    }

    *IATentry = it->second;

    success = VirtualProtect(IATentry, sizeof(void *), oldProtection, &oldProtection);
    if(!success)
    {
      RDCERR("Failed to restore IAT entry protection 0x%p", IATentry);
      continue;
    }
  }
}

bool LibraryHooks::Detect(const char *identifier)
{
  bool ret = false;
  ForAllModules([&ret, identifier](const MODULEENTRY32 &me32) {
    if(GetProcAddress(me32.hModule, identifier) != NULL)
      ret = true;
  });
  return ret;
}

// Export Address Table (EAT) Hook support
// This hooks the DLL's export table directly, catching calls that bypass IAT
static std::map<void *, void *> s_Trampolines;  // original -> trampoline
static volatile bool s_EATHookInProgress = false;  // Prevent re-hooking during function execution

// Simple x64 instruction length decoder to find instruction boundaries
// This ensures we don't truncate instructions when copying to trampoline
// This is a simplified decoder focusing on common function prologue instructions
static size_t GetX64InstructionLength(const BYTE *code, size_t maxBytes)
{
  if(!code || maxBytes == 0)
    return 0;
  
  size_t offset = 0;
  bool hasREX = false;
  
  // Skip instruction prefixes (can be up to 4 prefixes)
  int prefixCount = 0;
  while(offset < maxBytes && prefixCount < 4)
  {
    BYTE b = code[offset];
    // Check for prefix bytes
    if(b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3 ||
       b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 || b == 0x64 || b == 0x65)
    {
      offset++;
      prefixCount++;
      continue;
    }
    // Check for REX prefix (0x40-0x4F)
    if((b & 0xF0) == 0x40)
    {
      hasREX = true;
      offset++;
      continue;
    }
    break;
  }
  
  if(offset >= maxBytes)
    return maxBytes;
  
  BYTE opcode = code[offset];
  size_t opcodeStart = offset;
  offset++;
  
  // Handle two-byte opcodes (0x0F prefix)
  bool isTwoByteOpcode = false;
  if(opcode == 0x0F)
  {
    if(offset >= maxBytes)
      return maxBytes;
    isTwoByteOpcode = true;
    opcode = code[offset];
    offset++;
    
    // Handle three-byte opcodes (0x0F 0x38/0x3A)
    if(opcode == 0x38 || opcode == 0x3A)
    {
      if(offset >= maxBytes)
        return maxBytes;
      opcode = code[offset];
      offset++;
    }
  }
  
  // Handle common single-byte opcodes without ModR/M
  if(!isTwoByteOpcode)
  {
    // PUSH reg (0x50-0x57), POP reg (0x58-0x5F)
    if((opcode >= 0x50 && opcode <= 0x5F))
    {
      return offset;
    }
    // RET (0xC3), RET imm16 (0xC2)
    if(opcode == 0xC3)
    {
      return offset;
    }
    if(opcode == 0xC2)
    {
      if(offset + 2 > maxBytes)
        return maxBytes;
      return offset + 2;  // RET imm16
    }
    // NOP (0x90)
    if(opcode == 0x90)
    {
      return offset;
    }
    // INT3 (0xCC)
    if(opcode == 0xCC)
    {
      return offset;
    }
    // PUSH imm32 (0x68)
    if(opcode == 0x68)
    {
      if(offset + 4 > maxBytes)
        return maxBytes;
      return offset + 4;
    }
    // PUSH imm8 (0x6A)
    if(opcode == 0x6A)
    {
      if(offset + 1 > maxBytes)
        return maxBytes;
      return offset + 1;
    }
    // CALL rel32 (0xE8), JMP rel32 (0xE9)
    if(opcode == 0xE8 || opcode == 0xE9)
    {
      if(offset + 4 > maxBytes)
        return maxBytes;
      return offset + 4;
    }
    // JMP rel8 (0xEB)
    if(opcode == 0xEB)
    {
      if(offset + 1 > maxBytes)
        return maxBytes;
      return offset + 1;
    }
    // MOV reg, imm64 (0x48 0xB8+reg) - special case for x64
    if(hasREX && (opcode & 0xF8) == 0xB8)
    {
      if(offset + 8 > maxBytes)
        return maxBytes;
      return offset + 8;
    }
  }
  
  // Instructions with ModR/M byte
  if(offset >= maxBytes)
    return maxBytes;
  
  BYTE modRM = code[offset];
  offset++;
  
  BYTE mod = (modRM >> 6) & 0x03;
  BYTE rm = modRM & 0x07;
  
  // Check for SIB byte
  if(mod != 3 && rm == 4)
  {
    if(offset >= maxBytes)
      return maxBytes;
    offset++;  // Skip SIB byte
  }
  
  // Determine displacement size
  int displacementSize = 0;
  if(mod == 0 && rm == 5)
  {
    displacementSize = 4;  // [RIP+disp32] or [disp32]
  }
  else if(mod == 1)
  {
    displacementSize = 1;  // [reg+disp8]
  }
  else if(mod == 2)
  {
    displacementSize = 4;  // [reg+disp32]
  }
  
  // Determine immediate size based on opcode
  int immediateSize = 0;
  if(!isTwoByteOpcode)
  {
    BYTE baseOpcode = code[opcodeStart];
    if(baseOpcode == 0x83 || baseOpcode == 0xC6 || baseOpcode == 0x80)
    {
      immediateSize = 1;  // ADD/SUB/... reg, imm8
    }
    else if(baseOpcode == 0x81 || baseOpcode == 0xC7)
    {
      immediateSize = 4;  // ADD/SUB/... reg, imm32
    }
    else if((baseOpcode & 0xF0) == 0xB0 && !hasREX)
    {
      immediateSize = 1;  // MOV reg8, imm8
    }
    else if((baseOpcode & 0xF8) == 0xB8 && !hasREX)
    {
      immediateSize = 4;  // MOV reg32, imm32
    }
  }
  else
  {
    // Two-byte opcodes - most don't have immediate, but some do
    // For safety, we'll handle common cases
    if(opcode == 0xAE || opcode == 0xAF)  // Some SSE/AVX instructions
    {
      // These might have immediate, but for function prologues, unlikely
      immediateSize = 0;
    }
  }
  
  offset += displacementSize + immediateSize;
  
  return (offset > maxBytes) ? maxBytes : offset;
}

// Check if a function is already hooked (detects common hook patterns)
// Returns true if function appears to be already hooked
static bool IsFunctionAlreadyHooked(const BYTE *funcBytes)
{
  // Check for common hook patterns:
  // 1. JMP [RIP+0] pattern: 0xFF 0x25 0x00 0x00 0x00 0x00 (x64 long jump)
  if(funcBytes[0] == 0xFF && funcBytes[1] == 0x25)
  {
    DWORD offset = *(DWORD *)(funcBytes + 2);
    // Check if it's a RIP-relative jump (offset is usually 0 or small)
    if(offset == 0 || offset < 0x1000)
    {
      return true;
    }
  }
  
  // 2. Short JMP: 0xE9 (JMP rel32) - less common on x64 but possible
  if(funcBytes[0] == 0xE9)
  {
    return true;
  }
  
  // 3. JMP rel8: 0xEB - very short jump, less likely but possible
  if(funcBytes[0] == 0xEB)
  {
    return true;
  }
  
  return false;
}

// Calculate safe bytes to copy ensuring we don't truncate instructions
// Returns the number of bytes to copy (at least minBytes, aligned to instruction boundaries)
static size_t CalculateSafeCopySize(const BYTE *funcBytes, size_t minBytes, size_t maxBytes)
{
  size_t totalBytes = 0;
  size_t bytesRemaining = maxBytes;
  
  // Keep decoding instructions until we have at least minBytes
  while(totalBytes < minBytes && bytesRemaining > 0)
  {
    size_t instLen = GetX64InstructionLength(funcBytes + totalBytes, bytesRemaining);
    if(instLen == 0)
    {
      // Failed to decode, fall back to minBytes
      RDCLOG("[EAT_HOOK] Warning: Failed to decode instruction at offset %zu, using minimum size", totalBytes);
      return minBytes;
    }
    
    totalBytes += instLen;
    bytesRemaining -= instLen;
    
    // Safety limit: don't copy more than 32 bytes
    if(totalBytes >= 32)
      break;
  }
  
  return totalBytes;
}

static bool HookExportFunction(HMODULE module, const char *functionName, void *hookFunc, void **outOriginal)
{
  if(!module || !functionName || !hookFunc)
    return false;

  // Get DOS header
  IMAGE_DOS_HEADER *dosHeader = (IMAGE_DOS_HEADER *)module;
  if(dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    return false;

  // Get NT headers
  IMAGE_NT_HEADERS *ntHeaders = (IMAGE_NT_HEADERS *)((BYTE *)module + dosHeader->e_lfanew);
  if(ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    return false;

  // Get export directory
  IMAGE_DATA_DIRECTORY *exportDir = &ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if(exportDir->VirtualAddress == 0)
    return false;

  IMAGE_EXPORT_DIRECTORY *exports = (IMAGE_EXPORT_DIRECTORY *)((BYTE *)module + exportDir->VirtualAddress);
  
  DWORD *nameRVAs = (DWORD *)((BYTE *)module + exports->AddressOfNames);
  DWORD *funcRVAs = (DWORD *)((BYTE *)module + exports->AddressOfFunctions);
  WORD *ordinals = (WORD *)((BYTE *)module + exports->AddressOfNameOrdinals);

  // Find the function
  for(DWORD i = 0; i < exports->NumberOfNames; i++)
  {
    const char *name = (const char *)((BYTE *)module + nameRVAs[i]);
    if(strcmp(name, functionName) == 0)
    {
      // Found it!
      WORD ordinal = ordinals[i];
      void *originalFunc = (void *)((BYTE *)module + funcRVAs[ordinal]);
      
      // Check if already hooked
      if(s_Trampolines.find(originalFunc) != s_Trampolines.end())
      {
        RDCLOG("[EAT_HOOK] Function %s already hooked (original=0x%p, trampoline=0x%p), skipping re-hook", 
               functionName, originalFunc, s_Trampolines[originalFunc]);
        if(outOriginal)
          *outOriginal = s_Trampolines[originalFunc];
        return true;
      }
      
      RDCLOG("[EAT_HOOK] Hooking function %s at address 0x%p", functionName, originalFunc);

      BYTE *funcBytes = (BYTE *)originalFunc;
      
      // Check if function is already hooked (by another hooking library)
      if(IsFunctionAlreadyHooked(funcBytes))
      {
        RDCLOG("[EAT_HOOK] Warning: Function %s at 0x%p appears to be already hooked, proceeding anyway", 
               functionName, originalFunc);
      }

      // Allocate memory for trampoline (original bytes + JMP back)
      // We copy more bytes to be safe (32 bytes + 14 for JMP = 46 bytes, round to 64)
      void *trampoline = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
      if(!trampoline)
      {
        RDCERR("Failed to allocate trampoline for %s", functionName);
        return false;
      }

      BYTE *trampolineBytes = (BYTE *)trampoline;

      // We need to copy at least 14 bytes (size of JMP instruction), but we must ensure
      // we don't break any instructions. Use instruction boundary analysis to calculate
      // safe copy size that aligns to complete instruction boundaries.
      const size_t minBytes = 14;  // Minimum bytes needed for JMP instruction (0xFF 0x25 [32-bit offset] [64-bit target])
      const size_t maxBytes = 32;  // Safety limit: don't copy more than 32 bytes
      size_t bytesToCopy = CalculateSafeCopySize(funcBytes, minBytes, maxBytes);
      memcpy(trampolineBytes, funcBytes, bytesToCopy);
      
      // Log the first bytes of original function for debugging
      rdcstr byteStr;
      const size_t logBytes = RDCMIN(bytesToCopy, (size_t)32);
      for(size_t j = 0; j < logBytes; j++)
      {
        if(j > 0)
          byteStr += " ";
        char buf[4];
        sprintf_s(buf, sizeof(buf), "%02X", funcBytes[j]);
        byteStr += buf;
      }
      RDCLOG("[EAT_HOOK] Original function %s first %zu bytes: %s", functionName, bytesToCopy, byteStr.c_str());

      // Add JMP back to original function + bytesToCopy (continuing execution after our hook)
      // Format: JMP [RIP+0] followed by 64-bit target address
      // 0xFF 0x25 [32-bit offset=0] [64-bit target address]
      trampolineBytes[bytesToCopy] = 0xFF;  // JMP [RIP+offset]
      trampolineBytes[bytesToCopy + 1] = 0x25;  // ModR/M byte for [RIP+offset]
      *(DWORD *)(trampolineBytes + bytesToCopy + 2) = 0;  // 32-bit offset (0 = next instruction)
      void *targetAddr = (void *)((BYTE *)originalFunc + bytesToCopy);
      *(void **)(trampolineBytes + bytesToCopy + 6) = targetAddr;  // 64-bit target address
      
      // Flush instruction cache for trampoline to ensure CPU sees the new code
      FlushInstructionCache(GetCurrentProcess(), trampoline, bytesToCopy + 14);
      
      // Verify the JMP instruction was written correctly
      RDCLOG("[EAT_HOOK] Created trampoline for %s: copied %zu bytes, JMP back to 0x%p", 
             functionName, bytesToCopy, targetAddr);
      RDCLOG("[EAT_HOOK] Trampoline JMP instruction: 0x%02X 0x%02X [offset=0x%08X] [target=0x%p]", 
             trampolineBytes[bytesToCopy], trampolineBytes[bytesToCopy + 1], 
             *(DWORD *)(trampolineBytes + bytesToCopy + 2), 
             *(void **)(trampolineBytes + bytesToCopy + 6));

      // Now hook the original function
      // IMPORTANT: Make the function writable only when needed, and restore protection immediately
      // This minimizes the time window where the code section appears writable (anti-cheat detection)
      DWORD oldProtect;
      if(!VirtualProtect(originalFunc, bytesToCopy, PAGE_EXECUTE_READWRITE, &oldProtect))
      {
        RDCERR("Failed to make function writable for %s", functionName);
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
      }

      // Prepare hook bytes in a local buffer first to minimize modification time
      BYTE hookBytes[32];
      memset(hookBytes, 0x90, sizeof(hookBytes));  // Fill with NOPs first
      
      // Write JMP to our hook (only need 14 bytes for JMP instruction)
      hookBytes[0] = 0xFF;  // JMP [RIP+0]
      hookBytes[1] = 0x25;
      *(DWORD *)(hookBytes + 2) = 0;
      *(void **)(hookBytes + 6) = hookFunc;
      
      // Copy prepared bytes atomically (minimize visible modification window)
      memcpy(funcBytes, hookBytes, bytesToCopy);

      // Ensure memory writes are visible before flushing instruction cache
      MemoryBarrier();  // Memory barrier to ensure all writes are committed
      
      // Flush instruction cache to ensure CPU sees the new code
      FlushInstructionCache(GetCurrentProcess(), originalFunc, bytesToCopy);
      
      // Restore memory protection IMMEDIATELY after hooking
      // This is critical for anti-cheat evasion - code section should not remain writable
      VirtualProtect(originalFunc, bytesToCopy, oldProtect, &oldProtect);

      // NOTE: Export table RVA modification is DISABLED to avoid crashes
      // Modifying export table RVA may break exception handling information (.pdata section)
      // Inline hook will still work for all function calls
      RDCLOG("[EAT_HOOK] Export table RVA modification disabled for %s (hook=0x%p), using inline hook only", 
             functionName, hookFunc);

      // Store trampoline mapping
      s_Trampolines[originalFunc] = trampoline;
      
      if(outOriginal)
        *outOriginal = trampoline;  // Return trampoline address

      RDCLOG("[EAT_HOOK] Successfully hooked %s in module 0x%p (original=0x%p, hook=0x%p, trampoline=0x%p)", 
             functionName, module, originalFunc, hookFunc, trampoline);
      return true;
    }
  }

  return false;
}

static void ApplyExportTableHooks()
{
  // Prevent re-hooking during function execution to avoid corrupting running code
  if(s_EATHookInProgress)
  {
    RDCLOG("[EAT_HOOK] EAT hook application already in progress, skipping to avoid corruption");
    return;
  }
  
  RDCLOG("[EAT_HOOK] Applying Export Address Table hooks...");

  // Hook d3d12.dll exports
  HMODULE d3d12 = GetModuleHandleA("d3d12.dll");
  if(d3d12)
  {
    RDCLOG("[EAT_HOOK] Found d3d12.dll at 0x%p", d3d12);
    
    // Get the D3D12CreateDevice hook function from our registered hooks
    for(auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
    {
      if(_stricmp(it->first.c_str(), "d3d12.dll") == 0)
      {
        for(const FunctionHook &hook : it->second.FunctionHooks)
        {
          if(hook.function == "D3D12CreateDevice")
          {
            void *original = NULL;
            if(HookExportFunction(d3d12, "D3D12CreateDevice", hook.hook, &original))
            {
              // Update the original function pointer
              if(hook.orig && original)
                *hook.orig = original;
              
              RDCLOG("[EAT_HOOK] ✓ Hooked D3D12CreateDevice export");
            }
            else
            {
              RDCWARN("[EAT_HOOK] ✗ Failed to hook D3D12CreateDevice export");
            }
          }
        }
      }
    }
  }
  else
  {
    RDCLOG("[EAT_HOOK] d3d12.dll not loaded yet, EAT hooks will be applied when it loads");
  }

  // Hook d3d11.dll exports
  HMODULE d3d11 = GetModuleHandleA("d3d11.dll");
  if(d3d11)
  {
    RDCLOG("[EAT_HOOK] Found d3d11.dll at 0x%p", d3d11);

    for(auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
    {
      if(_stricmp(it->first.c_str(), "d3d11.dll") == 0)
      {
        for(const FunctionHook &hook : it->second.FunctionHooks)
        {
          if(hook.function == "D3D11CreateDevice" || hook.function == "D3D11CreateDeviceAndSwapChain")
          {
            void *original = NULL;
            if(HookExportFunction(d3d11, hook.function.c_str(), hook.hook, &original))
            {
              if(hook.orig && original)
                *hook.orig = original;
              
              RDCLOG("[EAT_HOOK] ✓ Hooked %s export", hook.function.c_str());
            }
            else
            {
              RDCWARN("[EAT_HOOK] ✗ Failed to hook %s export", hook.function.c_str());
            }
          }
        }
      }
    }
  }
  else
  {
    RDCLOG("[EAT_HOOK] d3d11.dll not loaded yet");
  }
  
  // Hook dxgi.dll exports
  HMODULE dxgi = GetModuleHandleA("dxgi.dll");
  if(dxgi)
  {
    RDCLOG("[EAT_HOOK] Found dxgi.dll at 0x%p", dxgi);
    
    // Apply EAT hooks for dxgi.dll exports to catch direct calls that bypass IAT
    for(auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
    {
      if(_stricmp(it->first.c_str(), "dxgi.dll") == 0)
      {
        for(const FunctionHook &hook : it->second.FunctionHooks)
        {
          // Hook all CreateDXGIFactory variants and debug interfaces
          if(hook.function == "CreateDXGIFactory" || 
             hook.function == "CreateDXGIFactory1" || 
             hook.function == "CreateDXGIFactory2" ||
             hook.function == "DXGIGetDebugInterface" ||
             hook.function == "DXGIGetDebugInterface1")
          {
            void *original = NULL;
            if(HookExportFunction(dxgi, hook.function.c_str(), hook.hook, &original))
            {
              if(hook.orig && original)
                *hook.orig = original;
              
              RDCLOG("[EAT_HOOK] ✓ Hooked %s export", hook.function.c_str());
            }
            else
            {
              RDCWARN("[EAT_HOOK] ✗ Failed to hook %s export", hook.function.c_str());
            }
          }
        }
      }
    }
  }
  else
  {
    RDCLOG("[EAT_HOOK] dxgi.dll not loaded yet");
  }
}

void Win32_RegisterManualModuleHooking()
{
  InitHookData();

  s_HookData->hookAll = false;
}

void Win32_InterceptLibraryLoads(std::function<HMODULE(const rdcstr &, HANDLE, DWORD)> callback)
{
  s_HookData->libraryIntercept = callback;
}

void Win32_ManualHookModule(rdcstr modName, HMODULE module)
{
  for(auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
    std::sort(it->second.FunctionHooks.begin(), it->second.FunctionHooks.end());

  modName = strlower(modName);

  s_HookData->DllHooks[modName].module = module;

  for(FunctionHook &hook : s_HookData->DllHooks[modName].FunctionHooks)
  {
    if(hook.orig)
      *hook.orig = GetProcAddress(module, hook.function.c_str());
  }

  s_HookData->ApplyHooks(modName.c_str(), module);
}

// android only hooking functions, not used on win32
ScopedSuppressHooking::ScopedSuppressHooking()
{
}

ScopedSuppressHooking::~ScopedSuppressHooking()
{
}
