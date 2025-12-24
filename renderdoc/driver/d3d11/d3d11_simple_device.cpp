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

#include "d3d11_simple_device.h"
#include "core/core.h"

#if ENABLED(RDOC_WIN32)
#include <windows.h>  // For InterlockedIncrement/Decrement
#endif

WRAPPED_POOL_INST(WrappedID3D11DeviceSimple);

// Implementation of WrappedID3D11DeviceSimple - simple forwarding wrapper for debugging
HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::QueryInterface(REFIID riid, void **ppvObject)
{

  // DEFINE_GUID(IID_IDirect3DDevice9, 0xd0223b96, 0xbf7a, 0x43fd, 0x92, 0xbd, 0xa4, 0x3b, 0xd,
  // 0x82, 0xb9, 0xeb);
  static const GUID IDirect3DDevice9_uuid = {
      0xd0223b96, 0xbf7a, 0x43fd, {0x92, 0xbd, 0xa4, 0x3b, 0xd, 0x82, 0xb9, 0xeb}};

  // ID3D10Device UUID {9B7E4C0F-342C-4106-A19F-4F2704F689F0}
  static const GUID ID3D10Device_uuid = {
      0x9b7e4c0f, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

  // ID3D10DeviceChild UUID {9B7E4C00-342C-4106-A19F-4F2704F689F0}
  static const GUID ID3D10DeviceChild_uuid = {
      0x9b7e4c00, 0x342c, 0x4106, {0xa1, 0x9f, 0x4f, 0x27, 0x04, 0xf6, 0x89, 0xf0}};

  // ID3D12Device UUID {189819f1-1db6-4b57-be54-1821339b85f7}
  static const GUID ID3D12Device_uuid = {
      0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};

  // ID3D11ShaderTraceFactory UUID {1fbad429-66ab-41cc-9617-667ac10e4459}
  static const GUID ID3D11ShaderTraceFactory_uuid = {
      0x1fbad429, 0x66ab, 0x41cc, {0x96, 0x17, 0x66, 0x7a, 0xc1, 0x0e, 0x44, 0x59}};

  // ID3D11On12Device UUID {85611e73-70a9-490e-9614-a9e302777904}
  static const GUID ID3D11On12Device_uuid = {
      0x85611e73, 0x70a9, 0x490e, {0x96, 0x14, 0xa9, 0xe3, 0x02, 0x77, 0x79, 0x04}};

  // RenderDoc UUID {A7AA6116-9C8D-4BBA-9083-B4D816B71B78}
  static const GUID IRenderDoc_uuid = {
      0xa7aa6116, 0x9c8d, 0x4bba, {0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78}};

  // UUID for returning unwrapped ID3D11InfoQueue {3FC4E618-3F70-452A-8B8F-A73ACCB58E3D}
  static const GUID unwrappedID3D11InfoQueue__uuid = {
      0x3fc4e618, 0x3f70, 0x452a, {0x8b, 0x8f, 0xa7, 0x3a, 0xcc, 0xb5, 0x8e, 0x3d}};

  // UUID for internal interface that breaks hooks {26C5DC23-E49C-4B0A-8F79-E7B1AC804D32}
  static const GUID D3DInternal_uuid = {
      0x26c5dc23, 0xe49c, 0x4b0a, {0x8f, 0x79, 0xe7, 0xb1, 0xac, 0x80, 0x4d, 0x32}};

  HRESULT hr = S_OK;
  

  if(ppvObject == NULL)
  {
    RDCERR("[HOOK_DIAG] QueryInterface called with NULL ppvObject!");
    return E_INVALIDARG;
  }

  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(ID3D11Device4 *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(IDXGIDevice))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice *real = (IDXGIDevice *)(*ppvObject);
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Creating WrappedIDXGIDevice4 for IDXGIDevice, real=0x%p, d3dDevice=0x%p (this)", real, this);
      *ppvObject = (IDXGIDevice *)(new WrappedIDXGIDevice4(real, this));
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Created WrappedIDXGIDevice4=0x%p", *ppvObject);
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice1))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice1 *real = (IDXGIDevice1 *)(*ppvObject);
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Creating WrappedIDXGIDevice4 for IDXGIDevice1, real=0x%p, d3dDevice=0x%p (this)", real, this);
      *ppvObject = (IDXGIDevice1 *)(new WrappedIDXGIDevice4(real, this));
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Created WrappedIDXGIDevice4=0x%p", *ppvObject);
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice2))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice2 *real = (IDXGIDevice2 *)(*ppvObject);
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Creating WrappedIDXGIDevice4 for IDXGIDevice2, real=0x%p, d3dDevice=0x%p (this)", real, this);
      *ppvObject = (IDXGIDevice2 *)(new WrappedIDXGIDevice4(real, this));
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Created WrappedIDXGIDevice4=0x%p", *ppvObject);
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice3))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice3 *real = (IDXGIDevice3 *)(*ppvObject);
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Creating WrappedIDXGIDevice4 for IDXGIDevice3, real=0x%p, d3dDevice=0x%p (this)", real, this);
      *ppvObject = (IDXGIDevice3 *)(new WrappedIDXGIDevice4(real, this));
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Created WrappedIDXGIDevice4=0x%p", *ppvObject);
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(IDXGIDevice4))
  {
    hr = m_pDevice->QueryInterface(riid, ppvObject);

    if(SUCCEEDED(hr))
    {
      IDXGIDevice4 *real = (IDXGIDevice4 *)(*ppvObject);
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Creating WrappedIDXGIDevice4 for IDXGIDevice4, real=0x%p, d3dDevice=0x%p (this)", real, this);
      *ppvObject = (IDXGIDevice4 *)(new WrappedIDXGIDevice4(real, this));
      RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface - Created WrappedIDXGIDevice4=0x%p", *ppvObject);
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return hr;
    }
  }
  else if(riid == __uuidof(ID3D11Device))
  {
    AddRef();
    *ppvObject = (ID3D11Device *)this;
    return S_OK;
  }
  else if(riid == ID3D10Device_uuid)
  {
    RDCWARN("Trying to get ID3D10Device - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == ID3D10DeviceChild_uuid)
  {
    RDCWARN("Trying to get ID3D10DeviceChild - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == ID3D12Device_uuid)
  {
    RDCWARN("Trying to get ID3D12Device - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == IDirect3DDevice9_uuid)
  {
    RDCWARN("Trying to get IDirect3DDevice9 - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == D3DInternal_uuid)
  {
    RDCWARN("Trying to get internal unsupported D3D interface - not supported.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == ID3D11ShaderTraceFactory_uuid)
  {
    RDCWARN("Trying to get ID3D11ShaderTraceFactory. Not supported at this time.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == ID3D11On12Device_uuid)
  {
    RDCWARN("Trying to get ID3D11On12Device. Not supported at this time.");
    *ppvObject = NULL;
    return E_NOINTERFACE;
  }
  else if(riid == IRenderDoc_uuid)
  {
    AddRef();
    *ppvObject = (IUnknown *)this;
    return S_OK;
  }

  // Forward all other queries to the real device
  hr = m_pDevice->QueryInterface(riid, ppvObject);
  RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::QueryInterface forwarded to m_pDevice, hr=0x%x, *ppvObject=0x%p", 
         hr, ppvObject ? *ppvObject : NULL);
  return hr;
}

ULONG STDMETHODCALLTYPE WrappedID3D11DeviceSimple::AddRef()
{
  ULONG refCount = (ULONG)InterlockedIncrement((volatile LONG *)&m_RefCount);
  RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::AddRef, this=0x%p, new refCount=%u", this, refCount);
  return refCount;
}

ULONG STDMETHODCALLTYPE WrappedID3D11DeviceSimple::Release()
{
  ULONG refCount = (ULONG)InterlockedDecrement((volatile LONG *)&m_RefCount);
  RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::Release, this=0x%p, new refCount=%u", this, refCount);
  
  if(refCount == 0)
  {
    RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple::Release - refCount reached 0, deleting object");
    delete this;
    return 0;
  }
  return refCount;
}

void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetImmediateContext(ID3D11DeviceContext **ppImmediateContext)
{

  if(ppImmediateContext)
  {
    if(m_pDevice)
    {
      m_pDevice->GetImmediateContext(ppImmediateContext);
    }
    else
    {
      RDCERR("[HOOK_DIAG] GetImmediateContext: m_pDevice is NULL!");
      *ppImmediateContext = NULL;
    }
  }
  else
  {
    RDCERR("[HOOK_DIAG] GetImmediateContext: Called with NULL ppImmediateContext!");
  }
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateBuffer(const D3D11_BUFFER_DESC *pDesc,
                                                                  const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                                  ID3D11Buffer **ppBuffer)
{

  // Tiled resources are not supported
  if(pDesc && ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED) ||
               (pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILE_POOL)))
  {
    RDCERR("[HOOK_DIAG] CreateBuffer: Tiled resources not supported");
    return DXGI_ERROR_UNSUPPORTED;
  }

  // validation, returns S_FALSE for valid params, or an error code
  if(ppBuffer == NULL)
  {
    RDCLOG("[HOOK_DIAG] CreateBuffer: ppBuffer is NULL, calling real device");
    return m_pDevice->CreateBuffer(pDesc, pInitialData, NULL);
  }

  bool intelExtensionMagic = false;
  byte intelExtensionData[28] = {0};

  // snoop to disable the absurdly implemented intel DX11 extensions.
  if(pDesc->ByteWidth == sizeof(intelExtensionData) && pDesc->Usage == D3D11_USAGE_STAGING &&
     pInitialData && pDesc->BindFlags == 0)
  {
    byte *data = (byte *)pInitialData->pSysMem;

    if(!memcmp(data, "INTCEXTN", 8))
    {
      RDCLOG("Intercepting and preventing attempt to initialise intel extensions.");

      intelExtensionMagic = true;

      // back-up the data from the user
      memcpy(intelExtensionData, data, sizeof(intelExtensionData));

      // overwrite the initial data, so the driver doesn't see the request (it just sees an empty
      // buffer). Just in case passing along the real data does something.
      memset(data, 0, sizeof(intelExtensionData));
    }
  }

  ID3D11Buffer *real = NULL;
  HRESULT ret;
  
  ret = m_pDevice->CreateBuffer(pDesc, pInitialData, &real);

  if(intelExtensionMagic)
  {
    byte *data = (byte *)pInitialData->pSysMem;

    // restore the user's data unmodified, which is the expected behaviour when the extensions
    // aren't supported.
    memcpy(data, intelExtensionData, sizeof(intelExtensionData));
  }

  if(SUCCEEDED(ret))
  {
    *ppBuffer = real;
  }
  else
  {
    RDCERR("[HOOK_DIAG] CreateBuffer: Failed with HRESULT=0x%x", ret);
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc,
                                                                     const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                                     ID3D11Texture1D **ppTexture1D)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture1D == NULL)
    return m_pDevice->CreateTexture1D(pDesc, pInitialData, NULL);

  ID3D11Texture1D *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateTexture1D(pDesc, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real texture without wrapping
    *ppTexture1D = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
                                                                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                                      ID3D11Texture2D **ppTexture2D)
{
  // Tiled resources are not supported
  if(pDesc && pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED)
    return DXGI_ERROR_UNSUPPORTED;

  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture2D == NULL)
    return m_pDevice->CreateTexture2D(pDesc, pInitialData, NULL);

  ID3D11Texture2D *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateTexture2D(pDesc, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real texture without wrapping
    *ppTexture2D = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
                                                                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                                      ID3D11Texture3D **ppTexture3D)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppTexture3D == NULL)
    return m_pDevice->CreateTexture3D(pDesc, pInitialData, NULL);

  ID3D11Texture3D *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateTexture3D(pDesc, pInitialData, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real texture without wrapping
    *ppTexture3D = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateShaderResourceView(
    ID3D11Resource *pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
    ID3D11ShaderResourceView **ppSRView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppSRView == NULL)
    return m_pDevice->CreateShaderResourceView(pResource, pDesc, NULL);

  ID3D11ShaderResourceView *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateShaderResourceView(pResource, pDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real view without wrapping
    *ppSRView = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateUnorderedAccessView(
    ID3D11Resource *pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
    ID3D11UnorderedAccessView **ppUAView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppUAView == NULL)
    return m_pDevice->CreateUnorderedAccessView(pResource, pDesc, NULL);

  ID3D11UnorderedAccessView *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateUnorderedAccessView(pResource, pDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real view without wrapping
    *ppUAView = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateRenderTargetView(
    ID3D11Resource *pResource, const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
    ID3D11RenderTargetView **ppRTView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppRTView == NULL)
    return m_pDevice->CreateRenderTargetView(pResource, pDesc, NULL);

  ID3D11RenderTargetView *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateRenderTargetView(pResource, pDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real view without wrapping
    *ppRTView = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDepthStencilView(
    ID3D11Resource *pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
    ID3D11DepthStencilView **ppDepthStencilView)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDepthStencilView == NULL)
    return m_pDevice->CreateDepthStencilView(pResource, pDesc, NULL);

  ID3D11DepthStencilView *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateDepthStencilView(pResource, pDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real view without wrapping
    *ppDepthStencilView = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements,
    const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
    ID3D11InputLayout **ppInputLayout)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppInputLayout == NULL)
    return m_pDevice->CreateInputLayout(pInputElementDescs, NumElements,
                                        pShaderBytecodeWithInputSignature, BytecodeLength, NULL);

  ID3D11InputLayout *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateInputLayout(pInputElementDescs, NumElements,
                                     pShaderBytecodeWithInputSignature, BytecodeLength, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real input layout without wrapping
    *ppInputLayout = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateVertexShader(const void *pShaderBytecode,
                                                                        SIZE_T BytecodeLength,
                                                                        ID3D11ClassLinkage *pClassLinkage,
                                                                        ID3D11VertexShader **ppVertexShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppVertexShader == NULL)
    return m_pDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, NULL);

  ID3D11VertexShader *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real shader without wrapping
    *ppVertexShader = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateGeometryShader(const void *pShaderBytecode,
                                                                           SIZE_T BytecodeLength,
                                                                           ID3D11ClassLinkage *pClassLinkage,
                                                                           ID3D11GeometryShader **ppGeometryShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppGeometryShader == NULL)
    return m_pDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, NULL);

  ID3D11GeometryShader *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real shader without wrapping
    *ppGeometryShader = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateGeometryShaderWithStreamOutput(
    const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
    UINT NumEntries, const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream,
    ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppGeometryShader == NULL)
    return m_pDevice->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
                                                           NumEntries, pBufferStrides, NumStrides,
                                                           RasterizedStream, pClassLinkage, NULL);

  ID3D11GeometryShader *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
                                                         NumEntries, pBufferStrides, NumStrides,
                                                         RasterizedStream, pClassLinkage, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real shader without wrapping
    *ppGeometryShader = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreatePixelShader(const void *pShaderBytecode,
                                                                       SIZE_T BytecodeLength,
                                                                       ID3D11ClassLinkage *pClassLinkage,
                                                                       ID3D11PixelShader **ppPixelShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppPixelShader == NULL)
    return m_pDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, NULL);

  ID3D11PixelShader *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real shader without wrapping
    *ppPixelShader = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateHullShader(const void *pShaderBytecode,
                                                                     SIZE_T BytecodeLength,
                                                                     ID3D11ClassLinkage *pClassLinkage,
                                                                     ID3D11HullShader **ppHullShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppHullShader == NULL)
    return m_pDevice->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, NULL);

  ID3D11HullShader *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real shader without wrapping
    *ppHullShader = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDomainShader(const void *pShaderBytecode,
                                                                         SIZE_T BytecodeLength,
                                                                         ID3D11ClassLinkage *pClassLinkage,
                                                                         ID3D11DomainShader **ppDomainShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDomainShader == NULL)
    return m_pDevice->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, NULL);

  ID3D11DomainShader *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real shader without wrapping
    *ppDomainShader = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateComputeShader(const void *pShaderBytecode,
                                                                           SIZE_T BytecodeLength,
                                                                           ID3D11ClassLinkage *pClassLinkage,
                                                                           ID3D11ComputeShader **ppComputeShader)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppComputeShader == NULL)
    return m_pDevice->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, NULL);

  ID3D11ComputeShader *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real shader without wrapping
    *ppComputeShader = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateClassLinkage(ID3D11ClassLinkage **ppLinkage)
{
  // get 'real' return value for NULL parameter
  if(ppLinkage == NULL)
    return m_pDevice->CreateClassLinkage(NULL);

  ID3D11ClassLinkage *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateClassLinkage(&real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real linkage without wrapping
    *ppLinkage = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
                                                                       ID3D11BlendState **ppBlendState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppBlendState == NULL)
    return m_pDevice->CreateBlendState(pBlendStateDesc, NULL);

  ID3D11BlendState *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateBlendState(pBlendStateDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real state without wrapping
    *ppBlendState = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D11DepthStencilState **ppDepthStencilState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDepthStencilState == NULL)
    return m_pDevice->CreateDepthStencilState(pDepthStencilDesc, NULL);

  ID3D11DepthStencilState *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateDepthStencilState(pDepthStencilDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real state without wrapping
    *ppDepthStencilState = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateRasterizerState(
    const D3D11_RASTERIZER_DESC *pRasterizerDesc, ID3D11RasterizerState **ppRasterizerState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppRasterizerState == NULL)
    return m_pDevice->CreateRasterizerState(pRasterizerDesc, NULL);

  ID3D11RasterizerState *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateRasterizerState(pRasterizerDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real state without wrapping
    *ppRasterizerState = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
                                                                         ID3D11SamplerState **ppSamplerState)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppSamplerState == NULL)
    return m_pDevice->CreateSamplerState(pSamplerDesc, NULL);

  ID3D11SamplerState *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateSamplerState(pSamplerDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real state without wrapping
    *ppSamplerState = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateQuery(const D3D11_QUERY_DESC *pQueryDesc,
                                                                   ID3D11Query **ppQuery)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppQuery == NULL)
    return m_pDevice->CreateQuery(pQueryDesc, NULL);

  ID3D11Query *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateQuery(pQueryDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real query without wrapping
    *ppQuery = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc,
                                                                      ID3D11Predicate **ppPredicate)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppPredicate == NULL)
    return m_pDevice->CreatePredicate(pPredicateDesc, NULL);

  ID3D11Predicate *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreatePredicate(pPredicateDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real predicate without wrapping
    *ppPredicate = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc,
                                                                    ID3D11Counter **ppCounter)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppCounter == NULL)
    return m_pDevice->CreateCounter(pCounterDesc, NULL);

  ID3D11Counter *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateCounter(pCounterDesc, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real counter without wrapping
    *ppCounter = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDeferredContext(UINT ContextFlags,
                                                                            ID3D11DeviceContext **ppDeferredContext)
{
  // validation, returns S_FALSE for valid params, or an error code
  if(ppDeferredContext == NULL)
    return m_pDevice->CreateDeferredContext(ContextFlags, NULL);

  ID3D11DeviceContext *real = NULL;
  HRESULT ret;
  ret = m_pDevice->CreateDeferredContext(ContextFlags, &real);

  if(SUCCEEDED(ret))
  {
    // For now, just return the real context without wrapping
    *ppDeferredContext = real;
  }

  return ret;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface,
                                                                          void **ppResource)
{
  if(ppResource == NULL)
    return E_INVALIDARG;

  HRESULT hr;
  hr = m_pDevice->OpenSharedResource(hResource, ReturnedInterface, ppResource);

  if(FAILED(hr))
  {
    IUnknown *unk = (IUnknown *)*ppResource;
    SAFE_RELEASE(unk);
    return hr;
  }

  // For now, just return the real resource without wrapping
  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport)
{
  if(m_pDevice)
    return m_pDevice->CheckFormatSupport(Format, pFormatSupport);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CheckMultisampleQualityLevels(DXGI_FORMAT Format,
                                                                                     UINT SampleCount,
                                                                                     UINT *pNumQualityLevels)
{
  if(m_pDevice)
    return m_pDevice->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
  return E_FAIL;
}

void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo)
{
  if(m_pDevice)
    m_pDevice->CheckCounterInfo(pCounterInfo);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CheckCounter(const D3D11_COUNTER_DESC *pDesc,
                                                                   D3D11_COUNTER_TYPE *pType,
                                                                   UINT *pActiveCounters, LPSTR szName,
                                                                   UINT *pNameLength, LPSTR szUnits,
                                                                   UINT *pUnitsLength, LPSTR szDescription,
                                                                   UINT *pDescriptionLength)
{
  if(m_pDevice)
    return m_pDevice->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength,
                                   szDescription, pDescriptionLength);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CheckFeatureSupport(D3D11_FEATURE Feature,
                                                                          void *pFeatureSupportData,
                                                                          UINT FeatureSupportDataSize)
{
  if((Feature == D3D11_FEATURE_D3D11_OPTIONS1) || (Feature == D3D11_FEATURE_D3D11_OPTIONS2))
  {
    HRESULT hr = m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);

    if(SUCCEEDED(hr))
    {
      if(Feature == D3D11_FEATURE_D3D11_OPTIONS1)
      {
        D3D11_FEATURE_DATA_D3D11_OPTIONS1 *opts =
            (D3D11_FEATURE_DATA_D3D11_OPTIONS1 *)pFeatureSupportData;
        if(FeatureSupportDataSize != sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS1))
          return E_INVALIDARG;

        // don't support tiled resources
        opts->TiledResourcesTier = D3D11_TILED_RESOURCES_NOT_SUPPORTED;

        return S_OK;
      }
      else
      {
        D3D11_FEATURE_DATA_D3D11_OPTIONS2 *opts =
            (D3D11_FEATURE_DATA_D3D11_OPTIONS2 *)pFeatureSupportData;
        if(FeatureSupportDataSize != sizeof(D3D11_FEATURE_DATA_D3D11_OPTIONS2))
          return E_INVALIDARG;

        // don't support tiled resources
        opts->TiledResourcesTier = D3D11_TILED_RESOURCES_NOT_SUPPORTED;

        return S_OK;
      }
    }

    return hr;
  }

  if(m_pDevice)
    return m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
  if(m_pDevice)
    return m_pDevice->GetPrivateData(guid, pDataSize, pData);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
  if(m_pDevice)
    return m_pDevice->SetPrivateData(guid, DataSize, pData);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
  if(m_pDevice)
    return m_pDevice->SetPrivateDataInterface(guid, pData);
  return E_FAIL;
}

D3D_FEATURE_LEVEL STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetFeatureLevel(void)
{
  if(m_pDevice)
    return m_pDevice->GetFeatureLevel();
  return D3D_FEATURE_LEVEL_9_1;
}

UINT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetCreationFlags(void)
{
  if(m_pDevice)
    return m_pDevice->GetCreationFlags();
  return 0;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetDeviceRemovedReason(void)
{
  if(m_pDevice)
    return m_pDevice->GetDeviceRemovedReason();
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::SetExceptionMode(UINT RaiseFlags)
{
  HRESULT ret;
  if(m_pDevice)
  {
    ret = m_pDevice->SetExceptionMode(RaiseFlags);
    return ret;
  }
  return E_FAIL;
}

UINT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetExceptionMode(void)
{
  if(m_pDevice)
    return m_pDevice->GetExceptionMode();
  return 0;
}

// ID3D11Device1 methods
void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext)
{
  if(m_pDevice)
  {
    ID3D11Device1 *device1 = NULL;
    if(SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&device1)))
    {
      device1->GetImmediateContext1(ppImmediateContext);
      device1->Release();
    }
  }
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDeferredContext1(UINT ContextFlags,
                                                                              ID3D11DeviceContext1 **ppDeferredContext)
{
  if(m_pDevice)
  {
    ID3D11Device1 *device1 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&device1);
    if(SUCCEEDED(hr))
    {
      hr = device1->CreateDeferredContext1(ContextFlags, ppDeferredContext);
      device1->Release();
      return hr;
    }
  }
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc,
                                                                       ID3D11BlendState1 **ppBlendState)
{
  if(m_pDevice)
  {
    ID3D11Device1 *device1 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&device1);
    if(SUCCEEDED(hr))
    {
      hr = device1->CreateBlendState1(pBlendStateDesc, ppBlendState);
      device1->Release();
      return hr;
    }
  }
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateRasterizerState1(
    const D3D11_RASTERIZER_DESC1 *pRasterizerDesc, ID3D11RasterizerState1 **ppRasterizerState)
{
  if(m_pDevice)
  {
    ID3D11Device1 *device1 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&device1);
    if(SUCCEEDED(hr))
    {
      hr = device1->CreateRasterizerState1(pRasterizerDesc, ppRasterizerState);
      device1->Release();
      return hr;
    }
  }
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDeviceContextState(
    UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
    REFIID EmulatedInterface, D3D_FEATURE_LEVEL *pChosenFeatureLevel, ID3DDeviceContextState **ppContextState)
{
  if(m_pDevice)
  {
    ID3D11Device1 *device1 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&device1);
    if(SUCCEEDED(hr))
    {
      hr = device1->CreateDeviceContextState(Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                                             EmulatedInterface, pChosenFeatureLevel, ppContextState);
      device1->Release();
      return hr;
    }
  }
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::OpenSharedResource1(HANDLE hResource, REFIID returnedInterface,
                                                                          void **ppResource)
{
  if(ppResource == NULL)
    return E_INVALIDARG;

  ID3D11Device1 *device1 = NULL;
  HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&device1);
  if(FAILED(hr))
    return hr;

  hr = device1->OpenSharedResource1(hResource, returnedInterface, ppResource);
  device1->Release();

  if(FAILED(hr))
  {
    IUnknown *unk = (IUnknown *)*ppResource;
    SAFE_RELEASE(unk);
    return hr;
  }

  // For now, just return the real resource without wrapping
  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess,
                                                                               REFIID returnedInterface, void **ppResource)
{
  if(ppResource == NULL)
    return E_INVALIDARG;

  ID3D11Device1 *device1 = NULL;
  HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&device1);
  if(FAILED(hr))
    return hr;

  hr = device1->OpenSharedResourceByName(lpName, dwDesiredAccess, returnedInterface, ppResource);
  device1->Release();

  if(FAILED(hr))
  {
    IUnknown *unk = (IUnknown *)*ppResource;
    SAFE_RELEASE(unk);
    return hr;
  }

  // For now, just return the real resource without wrapping
  return hr;
}

// ID3D11Device2 methods
void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext)
{
  if(m_pDevice)
  {
    ID3D11Device2 *device2 = NULL;
    if(SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&device2)))
    {
      device2->GetImmediateContext2(ppImmediateContext);
      device2->Release();
    }
  }
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDeferredContext2(UINT ContextFlags,
                                                                              ID3D11DeviceContext2 **ppDeferredContext)
{
  if(m_pDevice)
  {
    ID3D11Device2 *device2 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&device2);
    if(SUCCEEDED(hr))
    {
      hr = device2->CreateDeferredContext2(ContextFlags, ppDeferredContext);
      device2->Release();
      return hr;
    }
  }
  return E_FAIL;
}

void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetResourceTiling(ID3D11Resource *pTiledResource,
                                                                     UINT *pNumTilesForEntireResource,
                                                                     D3D11_PACKED_MIP_DESC *pPackedMipDesc,
                                                                     D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
                                                                     UINT *pNumSubresourceTilings,
                                                                     UINT FirstSubresourceTilingToGet,
                                                                     D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
  if(m_pDevice)
  {
    ID3D11Device2 *device2 = NULL;
    if(SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&device2)))
    {
      device2->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc,
                                 pStandardTileShapeForNonPackedMips, pNumSubresourceTilings,
                                 FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
      device2->Release();
    }
  }
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount,
                                                                                     UINT Flags, UINT *pNumQualityLevels)
{
  if(m_pDevice)
  {
    ID3D11Device2 *device2 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&device2);
    if(SUCCEEDED(hr))
    {
      hr = device2->CheckMultisampleQualityLevels1(Format, SampleCount, Flags, pNumQualityLevels);
      device2->Release();
      return hr;
    }
  }
  return E_FAIL;
}

// ID3D11Device3 methods
void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::GetImmediateContext3(ID3D11DeviceContext3 **ppImmediateContext)
{
  if(m_pDevice)
  {
    ID3D11Device3 *device3 = NULL;
    if(SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&device3)))
    {
      device3->GetImmediateContext3(ppImmediateContext);
      device3->Release();
    }
  }
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateDeferredContext3(UINT ContextFlags,
                                                                              ID3D11DeviceContext3 **ppDeferredContext)
{
  if(m_pDevice)
  {
    ID3D11Device3 *device3 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&device3);
    if(SUCCEEDED(hr))
    {
      hr = device3->CreateDeferredContext3(ContextFlags, ppDeferredContext);
      device3->Release();
      return hr;
    }
  }
  return E_FAIL;
}

void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                                                                      const D3D11_BOX *pDstBox, const void *pSrcData,
                                                                      UINT SrcRowPitch, UINT SrcDepthPitch)
{
  if(m_pDevice)
  {
    ID3D11Device3 *device3 = NULL;
    if(SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&device3)))
    {
      device3->WriteToSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
      device3->Release();
    }
  }
}

void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch,
                                                                       ID3D11Resource *pSrcResource, UINT SrcSubresource,
                                                                       const D3D11_BOX *pSrcBox)
{
  if(m_pDevice)
  {
    ID3D11Device3 *device3 = NULL;
    if(SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&device3)))
    {
      device3->ReadFromSubresource(pDstData, DstRowPitch, DstDepthPitch, pSrcResource, SrcSubresource, pSrcBox);
      device3->Release();
    }
  }
}

// ID3D11Device4 methods
HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::RegisterDeviceRemovedEvent(HANDLE hEvent, DWORD *pdwCookie)
{
  if(m_pDevice)
  {
    ID3D11Device4 *device4 = NULL;
    HRESULT hr = m_pDevice->QueryInterface(__uuidof(ID3D11Device4), (void **)&device4);
    if(SUCCEEDED(hr))
    {
      hr = device4->RegisterDeviceRemovedEvent(hEvent, pdwCookie);
      device4->Release();
      return hr;
    }
  }
  return E_FAIL;
}

void STDMETHODCALLTYPE WrappedID3D11DeviceSimple::UnregisterDeviceRemoved(DWORD dwCookie)
{
  if(m_pDevice)
  {
    ID3D11Device4 *device4 = NULL;
    if(SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Device4), (void **)&device4)))
    {
      device4->UnregisterDeviceRemoved(dwCookie);
      device4->Release();
    }
  }
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::OpenSharedFence(HANDLE hFence, REFIID returnedInterface, void **ppFence)
{
  if(m_pDevice5)
    return m_pDevice5->OpenSharedFence(hFence, returnedInterface, ppFence);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags,
                                                                 REFIID returnedInterface, void **ppFence)
{
  if(m_pDevice5)
    return m_pDevice5->CreateFence(InitialValue, Flags, returnedInterface, ppFence);
  return E_FAIL;
}

// ID3D11Device3 methods
HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *pDesc1,
                                                                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                                      ID3D11Texture2D1 **ppTexture2D)
{
  if(m_pDevice3)
    return m_pDevice3->CreateTexture2D1(pDesc1, pInitialData, ppTexture2D);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *pDesc1,
                                                                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                                                                      ID3D11Texture3D1 **ppTexture3D)
{
  if(m_pDevice3)
    return m_pDevice3->CreateTexture3D1(pDesc1, pInitialData, ppTexture3D);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc,
                                                                             ID3D11RasterizerState2 **ppRasterizerState)
{
  if(m_pDevice3)
    return m_pDevice3->CreateRasterizerState2(pRasterizerDesc, ppRasterizerState);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateShaderResourceView1(ID3D11Resource *pResource,
                                                                                 const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc1,
                                                                                 ID3D11ShaderResourceView1 **ppSRView1)
{
  if(m_pDevice3)
    return m_pDevice3->CreateShaderResourceView1(pResource, pDesc1, ppSRView1);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateUnorderedAccessView1(ID3D11Resource *pResource,
                                                                                  const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1,
                                                                                  ID3D11UnorderedAccessView1 **ppUAView1)
{
  if(m_pDevice3)
    return m_pDevice3->CreateUnorderedAccessView1(pResource, pDesc1, ppUAView1);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateRenderTargetView1(ID3D11Resource *pResource,
                                                                              const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1,
                                                                              ID3D11RenderTargetView1 **ppRTView1)
{
  if(m_pDevice3)
    return m_pDevice3->CreateRenderTargetView1(pResource, pDesc1, ppRTView1);
  return E_FAIL;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11DeviceSimple::CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc1,
                                                                    ID3D11Query1 **ppQuery1)
{
  if(m_pDevice3)
    return m_pDevice3->CreateQuery1(pQueryDesc1, ppQuery1);
  return E_FAIL;
}

