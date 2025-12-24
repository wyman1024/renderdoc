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

#include "d3d11_hooks.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "hooks/hooks.h"
#include "d3d11_device.h"
#include "d3d11_simple_device.h"
#include "common/hook_logger.h"
#include "core/core.h"

ID3DDevice *GetD3D11DeviceIfAlloc(IUnknown *dev)
{
  RDCLOG("[HOOK_DIAG] GetD3D11DeviceIfAlloc called with dev=0x%p", dev);
  
  if(dev == NULL)
  {
    RDCLOG("[HOOK_DIAG] GetD3D11DeviceIfAlloc: dev is NULL");
    return NULL;
  }
  
  // Check if it's a wrapped device
  if(WrappedID3D11Device::IsAlloc(dev))
  {
    RDCLOG("[HOOK_DIAG] GetD3D11DeviceIfAlloc: dev=0x%p is a WrappedID3D11Device", dev);
    return (WrappedID3D11Device *)dev;
  }
  
  // Check if it's a WrappedID3D11DeviceSimple (simple wrapper used for debugging)
  if(WrappedID3D11DeviceSimple::IsAlloc(dev))
  {
    RDCLOG("[HOOK_DIAG] GetD3D11DeviceIfAlloc: dev=0x%p is a WrappedID3D11DeviceSimple", dev);
    return (WrappedID3D11DeviceSimple *)dev;
  }
  
  // Try to identify what type of object this is
  ID3D11Device *d3d11dev = NULL;
  HRESULT hr = dev->QueryInterface(__uuidof(ID3D11Device), (void **)&d3d11dev);
  if(SUCCEEDED(hr) && d3d11dev)
  {
    RDCLOG("[HOOK_DIAG] GetD3D11DeviceIfAlloc: dev=0x%p is an ID3D11Device but NOT wrapped (IsAlloc returned false)", dev);
    RDCLOG("[HOOK_DIAG] GetD3D11DeviceIfAlloc: This means the device was created outside RenderDoc's hooks");
    SAFE_RELEASE(d3d11dev);
  }
  else
  {
    RDCLOG("[HOOK_DIAG] GetD3D11DeviceIfAlloc: dev=0x%p is NOT an ID3D11Device (QueryInterface failed, hr=0x%x)", dev, hr);
  }

  return NULL;
}

// Debug flags to isolate issues in Create_Internal
// Set to false to disable wrapping and test basic functionality
static const bool ENABLE_DEVICE_WRAPPING = true;
static const bool ENABLE_SWAPCHAIN_WRAPPING = true;
static const bool ENABLE_CONTEXT_WRAPPING = true;
static const bool ENABLE_DXGI_QUERIES = true;  // Disable QueryInterface/GetParent in WrappedID3D11Device constructor

// Use simple forwarding wrapper instead of full WrappedID3D11Device for debugging
// Set to true to use WrappedID3D11DeviceSimple (only forwards calls, no wrapping logic)
static const bool USE_SIMPLE_WRAPPER = false;

class D3D11Hook : LibraryHook
{
public:
  void RegisterHooks()
  {
    RDCLOG("Registering D3D11 hooks");

    WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D11DeviceIfAlloc);

    // also require d3dcompiler_??.dll
    if(GetD3DCompiler() == NULL)
    {
      RDCERR("Failed to load d3dcompiler_??.dll - not inserting D3D11 hooks.");
      return;
    }

    LibraryHooks::RegisterLibraryHook("d3d11.dll", NULL);

    CreateDevice.Register("d3d11.dll", "D3D11CreateDevice", D3D11CreateDevice_hook);
    CreateDeviceAndSwapChain.Register("d3d11.dll", "D3D11CreateDeviceAndSwapChain",
                                      D3D11CreateDeviceAndSwapChain_hook);

    m_RecurseSlot = Threading::AllocateTLSSlot();
    Threading::SetTLSValue(m_RecurseSlot, NULL);
  }

private:
  static D3D11Hook d3d11hooks;

  HookedFunction<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN> CreateDeviceAndSwapChain;
  HookedFunction<PFN_D3D11_CREATE_DEVICE> CreateDevice;

  // re-entrancy detection (can happen in rare cases with e.g. fraps)
  uint64_t m_RecurseSlot = 0;

  void EndRecurse() { Threading::SetTLSValue(m_RecurseSlot, NULL); }
  bool CheckRecurse()
  {
    if(Threading::GetTLSValue(m_RecurseSlot) == NULL)
    {
      Threading::SetTLSValue(m_RecurseSlot, (void *)1);
      return false;
    }

    return true;
  }

  friend HRESULT CreateD3D11_Internal(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                                      D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                                      __in_ecount_opt(FeatureLevels)
                                          CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                                      UINT FeatureLevels, UINT SDKVersion,
                                      __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                                      __out_opt IDXGISwapChain **ppSwapChain,
                                      __out_opt ID3D11Device **ppDevice,
                                      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                                      __out_opt ID3D11DeviceContext **ppImmediateContext);



    HRESULT Create_Internal(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                          D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                          __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                          UINT FeatureLevels, UINT SDKVersion,
                          __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                          __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
                          __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                          __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    // if we're already inside a wrapped create, then DON'T do anything special. Just call onwards
    if(CheckRecurse())
    {
      return real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                  pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    }

    RDCDEBUG("Call to Create_Internal Flags %x", Flags);

    // we should no longer go through here in the replay application
    RDCASSERT(!RenderDoc::Inst().IsReplayApp());

    if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
      Flags |= D3D11_CREATE_DEVICE_DEBUG;
    else
      Flags &= ~D3D11_CREATE_DEVICE_DEBUG;

    DXGI_SWAP_CHAIN_DESC swapDesc;
    DXGI_SWAP_CHAIN_DESC *pUsedSwapDesc = NULL;

    if(pSwapChainDesc)
    {
      swapDesc = *pSwapChainDesc;
      pUsedSwapDesc = &swapDesc;
    }

    if(pUsedSwapDesc && !RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    {
      pUsedSwapDesc->Windowed = TRUE;
    }

    RDCDEBUG("Calling real createdevice...");

    // Hack for D3DGear which crashes if ppDevice is NULL
    ID3D11Device *dummydev = NULL;
    bool dummyUsed = false;
    if(ppDevice == NULL)
    {
      ppDevice = &dummydev;
      dummyUsed = true;
    }

    HRESULT ret = real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                       SDKVersion, pUsedSwapDesc, ppSwapChain, ppDevice, pFeatureLevel, NULL);

    SAFE_RELEASE(dummydev);
    if(dummyUsed)
      ppDevice = NULL;

    RDCDEBUG("Called real createdevice...");

    bool suppress = false;

    suppress = (Flags & D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY) != 0;

    if(suppress)
    {
      RDCLOG("Application requested not to be hooked.");
    }
    else if(SUCCEEDED(ret) && ppDevice)
    {
      RDCDEBUG("succeeded and hooking.");

      if(!WrappedID3D11Device::IsAlloc(*ppDevice))
      {
        D3D11InitParams params;
        params.DriverType = DriverType;
        params.Flags = Flags;
        params.SDKVersion = SDKVersion;
        params.NumFeatureLevels = FeatureLevels;
        if(FeatureLevels > 0)
          memcpy(params.FeatureLevels, pFeatureLevels, sizeof(D3D_FEATURE_LEVEL) * FeatureLevels);

        if(USE_SIMPLE_WRAPPER)
        {
          WrappedID3D11DeviceSimple *wrap = new WrappedID3D11DeviceSimple(*ppDevice);

          RDCDEBUG("created wrapped device.");

          *ppDevice = wrap;

          wrap->GetImmediateContext(ppImmediateContext);

          if(ppSwapChain && *ppSwapChain)
            *ppSwapChain = new WrappedIDXGISwapChain4(
                *ppSwapChain, pSwapChainDesc ? pSwapChainDesc->OutputWindow : NULL, wrap);
        }
        else
        {
          WrappedID3D11Device *wrap = new WrappedID3D11Device(*ppDevice, params);

          RDCDEBUG("created wrapped device.");

          *ppDevice = wrap;

          wrap->GetImmediateContext(ppImmediateContext);

          if(ppSwapChain && *ppSwapChain)
            *ppSwapChain = new WrappedIDXGISwapChain4(
                *ppSwapChain, pSwapChainDesc ? pSwapChainDesc->OutputWindow : NULL, wrap);
        }
      }
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D11 device.");
    }
    else
    {
      RDCDEBUG("failed. HRESULT: %s", ToStr(ret).c_str());
    }

    EndRecurse();

    return ret;
  }

  HRESULT Create_Internal_Simple(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                          D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                          __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                          UINT FeatureLevels, UINT SDKVersion,
                          __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                          __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
                          __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                          __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    RDCLOG("[HOOK_DIAG] Create_Internal called with flags: ENABLE_DEVICE_WRAPPING=%d, ENABLE_SWAPCHAIN_WRAPPING=%d, ENABLE_CONTEXT_WRAPPING=%d, ENABLE_DXGI_QUERIES=%d",
           ENABLE_DEVICE_WRAPPING, ENABLE_SWAPCHAIN_WRAPPING, ENABLE_CONTEXT_WRAPPING, ENABLE_DXGI_QUERIES);
    
    // if we're already inside a wrapped create, then DON'T do anything special. Just call onwards
    if(CheckRecurse())
    {
      RDCLOG("[HOOK_DIAG] RECURSIVE CALL DETECTED! This is a nested Create_Internal call.");
      // Check if we need to use trampoline for EAT-hooked functions
      extern void *GetTrampolineForHookedFunction(void *hookedAddress);
      const PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN* pFunc = 
          real.target<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>();
      if(pFunc)
      {
        PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createFunc = *pFunc;  // Dereference to get actual function pointer
        void *trampoline = GetTrampolineForHookedFunction((void *)createFunc);
        if(trampoline != (void *)createFunc)
        {
          RDCLOG("[HOOK_DIAG] Recursive call: using EAT trampoline 0x%p instead of 0x%p", trampoline, createFunc);
          createFunc = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)trampoline;
          return createFunc(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                      pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
        }
      }
      
      return real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                  pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    }
    
    RDCLOG("[HOOK_DIAG] First-time call to Create_Internal (not recursive)");

    RDCDEBUG("Call to Create_Internal Flags %x", Flags);

    // we should no longer go through here in the replay application
    RDCASSERT(!RenderDoc::Inst().IsReplayApp());

    if(RenderDoc::Inst().GetCaptureOptions().apiValidation)
      Flags |= D3D11_CREATE_DEVICE_DEBUG;
    else
      Flags &= ~D3D11_CREATE_DEVICE_DEBUG;

    DXGI_SWAP_CHAIN_DESC swapDesc;
    DXGI_SWAP_CHAIN_DESC *pUsedSwapDesc = NULL;

    if(pSwapChainDesc)
    {
      swapDesc = *pSwapChainDesc;
      pUsedSwapDesc = &swapDesc;
    }

    if(pUsedSwapDesc && !RenderDoc::Inst().GetCaptureOptions().allowFullscreen)
    {
      pUsedSwapDesc->Windowed = TRUE;
    }

    // Hack for D3DGear which crashes if ppDevice is NULL
    ID3D11Device *dummydev = NULL;
    bool dummyUsed = false;
    if(ppDevice == NULL)
    {
      ppDevice = &dummydev;
      dummyUsed = true;
    }

    // Extract function pointer from std::function and check if we need trampoline
    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createFunc = NULL;
    const PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN* pFunc = 
        real.target<PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN>();
    if(pFunc)
    {
      createFunc = *pFunc;  // Dereference to get actual function pointer
      extern void *GetTrampolineForHookedFunction(void *hookedAddress);
      void *trampoline = GetTrampolineForHookedFunction((void *)createFunc);
      if(trampoline != (void *)createFunc)
      {
        RDCLOG("[HOOK_DIAG] Using EAT trampoline: original=0x%p, trampoline=0x%p", createFunc, trampoline);
        createFunc = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)trampoline;
      }
    }
    else
    {
      RDCWARN("[HOOK_DIAG] Failed to extract function pointer from std::function, using std::function directly");
    }

    RDCLOG("[HOOK_DIAG] Calling real D3D11 createdevice NOW... (createFunc=0x%p, pAdapter=0x%p, Flags=0x%x)", 
           createFunc, pAdapter, Flags);
    RDCLOG("[HOOK_DIAG] Parameters: ppSwapChain=0x%p, ppDevice=0x%p, ppImmediateContext=0x%p", 
           ppSwapChain, ppDevice, ppImmediateContext);
    HRESULT ret = E_FAIL;
    if(createFunc)
    {
      RDCLOG("[HOOK_DIAG] Calling trampoline function at 0x%p", createFunc);
      ret = createFunc(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                       SDKVersion, pUsedSwapDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
      RDCLOG("[HOOK_DIAG] Trampoline function returned, HRESULT=0x%x", ret);
      RDCLOG("[HOOK_DIAG] After trampoline: ppSwapChain=0x%p (*=0x%p), ppDevice=0x%p (*=0x%p), ppImmediateContext=0x%p (*=0x%p)",
             ppSwapChain, ppSwapChain ? *ppSwapChain : NULL, 
             ppDevice, ppDevice ? *ppDevice : NULL,
             ppImmediateContext, ppImmediateContext ? *ppImmediateContext : NULL);
    }
    else
    {
      RDCLOG("[HOOK_DIAG] Calling std::function directly (no trampoline)");
      ret = real(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                 SDKVersion, pUsedSwapDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
      RDCLOG("[HOOK_DIAG] std::function returned, HRESULT=0x%x", ret);
      RDCLOG("[HOOK_DIAG] After std::function: ppSwapChain=0x%p (*=0x%p), ppDevice=0x%p (*=0x%p), ppImmediateContext=0x%p (*=0x%p)",
             ppSwapChain, ppSwapChain ? *ppSwapChain : NULL, 
             ppDevice, ppDevice ? *ppDevice : NULL,
             ppImmediateContext, ppImmediateContext ? *ppImmediateContext : NULL);
    }
    RDCLOG("[HOOK_DIAG] Real D3D11 createdevice returned! HRESULT=0x%x", ret);

    SAFE_RELEASE(dummydev);
    if(dummyUsed)
      ppDevice = NULL;
    

    RDCLOG("[HOOK_DIAG] After dummydev cleanup: ppDevice=0x%p (*=0x%p)", 
           ppDevice, ppDevice ? *ppDevice : NULL);

    // Validate pointers after trampoline call
    if(SUCCEEDED(ret))
    {
      if(ppDevice && *ppDevice)
      {
        // Validate device pointer is not obviously invalid
        if((uintptr_t)*ppDevice == 0xffffffffffffffffULL || 
           (uintptr_t)*ppDevice < 0x10000)
        {
          RDCERR("[HOOK_DIAG] Invalid device pointer returned: 0x%p", *ppDevice);
          ret = E_FAIL;
        }
      }
      if(ppSwapChain && *ppSwapChain)
      {
        // Validate swap chain pointer is not obviously invalid
        if((uintptr_t)*ppSwapChain == 0xffffffffffffffffULL || 
           (uintptr_t)*ppSwapChain < 0x10000)
        {
          RDCERR("[HOOK_DIAG] Invalid swap chain pointer returned: 0x%p", *ppSwapChain);
          if(ppSwapChain)
            *ppSwapChain = NULL;
        }
      }
      if(ppImmediateContext && *ppImmediateContext)
      {
        // Validate context pointer is not obviously invalid
        if((uintptr_t)*ppImmediateContext == 0xffffffffffffffffULL || 
           (uintptr_t)*ppImmediateContext < 0x10000)
        {
          RDCERR("[HOOK_DIAG] Invalid context pointer returned: 0x%p", *ppImmediateContext);
          if(ppImmediateContext)
            *ppImmediateContext = NULL;
        }
      }
    }

    RDCLOG("[HOOK_DIAG] Finished processing D3D11 createdevice return");

    bool suppress = false;

    suppress = (Flags & D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY) != 0;

    if(suppress)
    {
      RDCLOG("Application requested not to be hooked.");
    }
    else if(SUCCEEDED(ret) && ppDevice)
    {
      RDCDEBUG("succeeded and hooking.");

      if(!WrappedID3D11Device::IsAlloc(*ppDevice))
      {
        if(ENABLE_DEVICE_WRAPPING)
        {
          if(USE_SIMPLE_WRAPPER)
          {
            RDCLOG("[HOOK_DIAG] Using SIMPLE wrapper (WrappedID3D11DeviceSimple) for debugging");
            try
            {
              RDCLOG("[HOOK_DIAG] About to create WrappedID3D11DeviceSimple, *ppDevice=0x%p", *ppDevice);
              WrappedID3D11DeviceSimple *wrap = NULL;
              
              try
              {
                wrap = new WrappedID3D11DeviceSimple(*ppDevice);
                RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple created successfully, wrap=0x%p", wrap);
              }
              catch(...)
              {
                RDCERR("[HOOK_DIAG] C++ EXCEPTION caught while creating WrappedID3D11DeviceSimple!");
                wrap = NULL;
              }
              
              if(wrap == NULL)
              {
                RDCERR("[HOOK_DIAG] Failed to create WrappedID3D11DeviceSimple!");
              }
              else
              {
                RDCLOG("[HOOK_DIAG] About to set *ppDevice = wrap (SIMPLE). Before: *ppDevice=0x%p, wrap=0x%p", 
                       *ppDevice, wrap);
                *ppDevice = wrap;
                RDCLOG("[HOOK_DIAG] Successfully set *ppDevice = wrap (SIMPLE). After: *ppDevice=0x%p", *ppDevice);
              }
            }
            catch(...)
            {
              RDCERR("[HOOK_DIAG] Exception caught while creating WrappedID3D11DeviceSimple!");
            }
          }
          else
          {
            RDCLOG("[HOOK_DIAG] Device wrapping ENABLED, creating WrappedID3D11Device");
            D3D11InitParams params;
            params.DriverType = DriverType;
            params.Flags = Flags;
            params.SDKVersion = SDKVersion;
            params.NumFeatureLevels = FeatureLevels;
            if(FeatureLevels > 0)
              memcpy(params.FeatureLevels, pFeatureLevels, sizeof(D3D_FEATURE_LEVEL) * FeatureLevels);

            try
            {
              RDCLOG("[HOOK_DIAG] About to create WrappedID3D11Device, *ppDevice=0x%p", *ppDevice);
              WrappedID3D11Device *wrap = NULL;
              
              try
              {
                wrap = new WrappedID3D11Device(*ppDevice, params);
                RDCLOG("[HOOK_DIAG] WrappedID3D11Device created successfully, wrap=0x%p", wrap);
              }
              catch(...)
              {
                RDCERR("[HOOK_DIAG] C++ EXCEPTION caught while creating WrappedID3D11Device!");
                wrap = NULL;
              }
              
              // Validate the wrapped device object
              if(wrap == NULL)
              {
                RDCERR("[HOOK_DIAG] Failed to create WrappedID3D11Device - new returned NULL or exception occurred!");
              }
              else
              {
              RDCLOG("[HOOK_DIAG] About to set *ppDevice = wrap. Before: *ppDevice=0x%p (real device), wrap=0x%p (WrappedID3D11Device*)", 
                     *ppDevice, wrap);
              
              // Verify that wrap can be cast to ID3D11Device*
              ID3D11Device *testCast = (ID3D11Device *)wrap;
              RDCLOG("[HOOK_DIAG] Cast test: wrap=0x%p, (ID3D11Device*)wrap=0x%p", wrap, testCast);
              
              // Test vtable access before assignment
              try
              {
                ULONG refCountBefore = wrap->AddRef();
                wrap->Release();
                RDCLOG("[HOOK_DIAG] Vtable test passed: AddRef/Release works, refCount=%u", refCountBefore);
              }
              catch(...)
              {
                RDCERR("[HOOK_DIAG] C++ EXCEPTION caught while testing vtable before *ppDevice=wrap!");
              }
              
              // Test QueryInterface before assignment
              try
              {
                ID3D11Device *testDevice = (ID3D11Device *)wrap;
                IUnknown *testUnknown = NULL;
                HRESULT testHr = testDevice->QueryInterface(__uuidof(IUnknown), (void **)&testUnknown);
                RDCLOG("[HOOK_DIAG] QueryInterface test before assignment: HRESULT=0x%x, testUnknown=0x%p", 
                       testHr, testUnknown);
                if(testUnknown)
                {
                  testUnknown->Release();
                }
              }
              catch(...)
              {
                RDCERR("[HOOK_DIAG] C++ EXCEPTION caught while testing QueryInterface before *ppDevice=wrap!");
              }
              
              // Now perform the assignment
              *ppDevice = wrap;
              RDCLOG("[HOOK_DIAG] Successfully set *ppDevice = wrap. After: *ppDevice=0x%p", *ppDevice);
              
              // Verify the assignment worked correctly
              if(*ppDevice != wrap)
              {
                RDCERR("[HOOK_DIAG] CRITICAL: *ppDevice != wrap after assignment! *ppDevice=0x%p, wrap=0x%p", 
                       *ppDevice, wrap);
              }
              
              // Test if the wrapped device can be used immediately after assignment
              try
              {
                ID3D11Device *testDevice = *ppDevice;
                if(testDevice != wrap)
                {
                  RDCERR("[HOOK_DIAG] CRITICAL: *ppDevice != wrap! *ppDevice=0x%p, wrap=0x%p", testDevice, wrap);
                }
                
                IUnknown *testUnknown = NULL;
                HRESULT testHr = testDevice->QueryInterface(__uuidof(IUnknown), (void **)&testUnknown);
                RDCLOG("[HOOK_DIAG] QueryInterface test after *ppDevice=wrap: HRESULT=0x%x, testUnknown=0x%p", 
                       testHr, testUnknown);
                if(testUnknown)
                {
                  testUnknown->Release();
                }
                
                // Test GetImmediateContext
                ID3D11DeviceContext *testContext = NULL;
                testDevice->GetImmediateContext(&testContext);
                RDCLOG("[HOOK_DIAG] GetImmediateContext test after *ppDevice=wrap: testContext=0x%p", testContext);
                if(testContext)
                {
                  testContext->Release();
                }
              }
              catch(...)
              {
                RDCERR("[HOOK_DIAG] C++ EXCEPTION caught while testing wrapped device after *ppDevice=wrap!");
              }
              }
            }
            catch(...)
            {
              RDCERR("[HOOK_DIAG] Exception caught in Create_Internal while creating WrappedID3D11Device!");
              // Don't wrap on exception, return original device
            }
          }
        }
        else
        {
          RDCLOG("[HOOK_DIAG] Device wrapping DISABLED, returning original device without wrapping");
        }
      }
      else
      {
        RDCLOG("[HOOK_DIAG] Device is already wrapped, skipping");
      }

      RDCDEBUG("created wrapped device.");

             if(ENABLE_CONTEXT_WRAPPING)
             {
               RDCLOG("[HOOK_DIAG] Context wrapping ENABLED");
               RDCLOG("[HOOK_DIAG] Before GetImmediateContext: ppImmediateContext=0x%p (*=0x%p)", 
                      ppImmediateContext, ppImmediateContext ? *ppImmediateContext : NULL);
              
               // If trampoline already filled ppImmediateContext, release it first
               // because wrap->GetImmediateContext will set it to the wrapped context
               // This is necessary because trampoline returns the raw (unwrapped) context,
               // but we need to return the wrapped context to the application
               if(ppImmediateContext && *ppImmediateContext)
               {
                 RDCLOG("[HOOK_DIAG] Releasing trampoline-returned context 0x%p", *ppImmediateContext);
                 SAFE_RELEASE(*ppImmediateContext);
                 RDCLOG("[HOOK_DIAG] Context released, *ppImmediateContext=0x%p", 
                        ppImmediateContext ? *ppImmediateContext : NULL);
               }

               RDCLOG("[HOOK_DIAG] Calling wrap->GetImmediateContext(ppImmediateContext=0x%p)", ppImmediateContext);
               (*ppDevice)->GetImmediateContext(ppImmediateContext);
               RDCLOG("[HOOK_DIAG] After GetImmediateContext: *ppImmediateContext=0x%p", 
                      ppImmediateContext ? *ppImmediateContext : NULL);
             }
             else
             {
               RDCLOG("[HOOK_DIAG] Context wrapping DISABLED, leaving context as-is");
             }

             if(ENABLE_SWAPCHAIN_WRAPPING && ppSwapChain && *ppSwapChain)
             {
               RDCLOG("[HOOK_DIAG] Swap chain wrapping ENABLED, wrapping swap chain 0x%p", *ppSwapChain);
             //  *ppSwapChain = new WrappedIDXGISwapChain4(
              //     *ppSwapChain, pSwapChainDesc ? pSwapChainDesc->OutputWindow : NULL, ppDevice);
               RDCLOG("[HOOK_DIAG] Swap chain wrapped, new *ppSwapChain=0x%p", *ppSwapChain);
             }
             else if(ppSwapChain && *ppSwapChain)
             {
               RDCLOG("[HOOK_DIAG] Swap chain wrapping DISABLED, leaving swap chain as-is");
             }
    }
    else if(SUCCEEDED(ret))
    {
      RDCLOG("Created wrapped D3D11 device.");
    }
    else
    {
      RDCDEBUG("failed. HRESULT: %s", ToStr(ret).c_str());
    }

    EndRecurse();

    return ret;
  }

  static HRESULT WINAPI D3D11CreateDevice_hook(
      __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, __out_opt ID3D11Device **ppDevice,
      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    const CaptureOptions &opts = RenderDoc::Inst().GetCaptureOptions();
    
    if(opts.logOnlyMode)
    {
      HookLogger::LogFunctionCall("D3D11CreateDevice");
      
      PFN_D3D11_CREATE_DEVICE createFunc = d3d11hooks.CreateDevice();
      
      if(createFunc == NULL)
      {
        createFunc = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(
            GetModuleHandleA("d3d11.dll"), "D3D11CreateDevice");
      }
      
      if(createFunc == NULL)
      {
        return E_UNEXPECTED;
      }
      
      // Directly call original function without any wrapping
      HRESULT hr = createFunc(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                               SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
      
      // Notify RenderDoc that D3D11 API is active (for UI display), but don't create wrapper objects
      if(SUCCEEDED(hr) && ppDevice && *ppDevice)
      {
        RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D11, false);
      }
      
      return hr;
    }
    
    RDCLOG("[HOOK_DIAG] D3D11CreateDevice_hook called! DriverType=%d, Flags=0x%x, FeatureLevels=%d", 
           DriverType, Flags, FeatureLevels);
    
    // just forward the call with NULL swapchain parameters
    HRESULT hr = D3D11CreateDeviceAndSwapChain_hook(pAdapter, DriverType, Software, Flags, pFeatureLevels,
                                              FeatureLevels, SDKVersion, NULL, NULL, ppDevice,
                                              pFeatureLevel, ppImmediateContext);
    
    RDCLOG("[HOOK_DIAG] D3D11CreateDevice result: %s, device=0x%p", ToStr(hr).c_str(), 
           ppDevice ? *ppDevice : NULL);
    
    return hr;
  }

  static HRESULT WINAPI D3D11CreateDeviceAndSwapChain_hook(
      __in_opt IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels,
      UINT SDKVersion, __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
      __out_opt IDXGISwapChain **ppSwapChain, __out_opt ID3D11Device **ppDevice,
      __out_opt D3D_FEATURE_LEVEL *pFeatureLevel, __out_opt ID3D11DeviceContext **ppImmediateContext)
  {
    static int callCount = 0;
    callCount++;
    RDCLOG("[HOOK_DIAG] D3D11CreateDeviceAndSwapChain_hook called (call #%d)! DriverType=%d, Flags=0x%x, FeatureLevels=%d, pAdapter=0x%p",
           callCount, DriverType, Flags, FeatureLevels, pAdapter);
    
    const CaptureOptions &opts = RenderDoc::Inst().GetCaptureOptions();

    PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN createFunc = d3d11hooks.CreateDeviceAndSwapChain();

    if(createFunc == NULL)
    {
      createFunc = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(
          GetModuleHandleA("d3d11.dll"), "D3D11CreateDeviceAndSwapChain");
    }

    if(createFunc == NULL)
    {
      return E_UNEXPECTED;
    }

    if(opts.logOnlyMode)
    {
      HookLogger::LogFunctionCall("D3D11CreateDeviceAndSwapChain");


      // Directly call original function without any wrapping
      HRESULT hr = createFunc(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                               SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel,
                               ppImmediateContext);
      
      // Notify RenderDoc that D3D11 API is active (for UI display), but don't create wrapper objects
      if(SUCCEEDED(hr) && ppDevice && *ppDevice)
      {
        RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D11, pSwapChainDesc != NULL);
      }
      
      return hr;
    }
    //
    //// Check if this function has been EAT-hooked and get the trampoline
    //extern void *GetTrampolineForHookedFunction(void *hookedAddress);
    //void *trampoline = GetTrampolineForHookedFunction((void *)createFunc);
    //if(trampoline != (void *)createFunc)
    //{
    //  RDCLOG("[HOOK_DIAG] Using EAT trampoline: original=0x%p, trampoline=0x%p", createFunc, trampoline);
    //  createFunc = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)trampoline;
    //}

    //// shouldn't ever get here, we should either have it from procaddress or the hook function, but
    //// let's be safe.
    //if(createFunc == NULL)
    //{
    //  RDCERR("Something went seriously wrong with the hooks!");
    //  RDCLOG("[HOOK_DIAG] FAILED: d3d11.dll not loaded or D3D11CreateDeviceAndSwapChain not found!");
    //  return E_UNEXPECTED;
    //}

    RDCLOG("[HOOK_DIAG] Calling Create_Internal to wrap D3D11 device...");
    HRESULT hr = d3d11hooks.Create_Internal(createFunc, pAdapter, DriverType, Software, Flags,
                                      pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc,
                                      ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
    RDCLOG("[HOOK_DIAG] D3D11CreateDeviceAndSwapChain result: %s, device=0x%p, swapchain=0x%p", 
           ToStr(hr).c_str(), ppDevice ? *ppDevice : NULL, ppSwapChain ? *ppSwapChain : NULL);
   
    // Notify RenderDoc that D3D11 API is active (for UI display), but don't create wrapper objects
    if(SUCCEEDED(hr) && ppDevice && *ppDevice)
    {
      RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D11, pSwapChainDesc != NULL);
    }
    return hr;
  }
};

D3D11Hook D3D11Hook::d3d11hooks;

HRESULT CreateD3D11_Internal(RealD3D11CreateFunction real, __in_opt IDXGIAdapter *pAdapter,
                             D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
                             __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *pFeatureLevels,
                             UINT FeatureLevels, UINT SDKVersion,
                             __in_opt CONST DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
                             __out_opt IDXGISwapChain **ppSwapChain,
                             __out_opt ID3D11Device **ppDevice,
                             __out_opt D3D_FEATURE_LEVEL *pFeatureLevel,
                             __out_opt ID3D11DeviceContext **ppImmediateContext)
{
  return D3D11Hook::d3d11hooks.Create_Internal(
      real, pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
      pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
}
