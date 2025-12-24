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

#include "d3d11_common.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_wrapped.h"

// Forward declarations for device context interfaces
struct ID3D11DeviceContext4;
struct ID3D11DeviceContext5;

// Simple wrapper that only forwards calls to m_pDevice, used for debugging
// This class is used for binary search debugging to isolate issues in the full WrappedID3D11Device
class WrappedID3D11DeviceSimple : public IFrameCapturer, public ID3DDevice, public ID3D11Device5
{
private:
  ID3D11Device *m_pDevice;
  ID3D11Device1 *m_pDevice1;
  ID3D11Device2 *m_pDevice2;
  ID3D11Device3 *m_pDevice3;
  ID3D11Device4 *m_pDevice4;
  ID3D11Device5 *m_pDevice5;
  int32_t m_RefCount;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11DeviceSimple);
  WrappedID3D11DeviceSimple(ID3D11Device *realDevice) : m_pDevice(realDevice), m_RefCount(1)
  {
    if(m_pDevice)
      m_pDevice->AddRef();
    
    // Query for higher version interfaces
    m_pDevice->QueryInterface(__uuidof(ID3D11Device1), (void **)&m_pDevice1);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device2), (void **)&m_pDevice2);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device3), (void **)&m_pDevice3);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device4), (void **)&m_pDevice4);
    m_pDevice->QueryInterface(__uuidof(ID3D11Device5), (void **)&m_pDevice5);
    
    RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple created, this=0x%p, m_pDevice=0x%p", this, m_pDevice);
  }

  virtual ~WrappedID3D11DeviceSimple()
  {
    RDCLOG("[HOOK_DIAG] WrappedID3D11DeviceSimple destroyed, this=0x%p", this);
    SAFE_RELEASE(m_pDevice5);
    SAFE_RELEASE(m_pDevice4);
    SAFE_RELEASE(m_pDevice3);
    SAFE_RELEASE(m_pDevice2);
    SAFE_RELEASE(m_pDevice1);
    SAFE_RELEASE(m_pDevice);
  }
  
  // IFrameCapturer interface
  RDCDriver GetFrameCaptureDriver() { return RDCDriver::D3D11; }
  void StartFrameCapture(DeviceOwnedWindow devWnd) { /* Simple wrapper - no capture */ }
  bool EndFrameCapture(DeviceOwnedWindow devWnd) { return false; }
  bool DiscardFrameCapture(DeviceOwnedWindow devWnd) { return false; }
  
  // ID3DDevice interface
  virtual void *GetFrameCapturerDevice() { return (ID3D11Device *)this; }
  virtual IFrameCapturer *GetFrameCapturer() { return this; }
  virtual IUnknown *GetRealIUnknown() { return m_pDevice; }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D11Texture2D); }
  virtual bool IsDeviceUUID(REFIID iid)
  {
    if(iid == __uuidof(ID3D11Device) || iid == __uuidof(ID3D11Device1) ||
       iid == __uuidof(ID3D11Device2) || iid == __uuidof(ID3D11Device3) ||
       iid == __uuidof(ID3D11Device4) || iid == __uuidof(ID3D11Device5))
      return true;
    return false;
  }
  virtual IUnknown *GetDeviceInterface(REFIID iid)
  {
    if(iid == __uuidof(ID3D11Device))
      return (ID3D11Device *)this;
    else if(iid == __uuidof(ID3D11Device1) && m_pDevice1)
      return (ID3D11Device1 *)this;
    else if(iid == __uuidof(ID3D11Device2) && m_pDevice2)
      return (ID3D11Device2 *)this;
    else if(iid == __uuidof(ID3D11Device3) && m_pDevice3)
      return (ID3D11Device3 *)this;
    else if(iid == __uuidof(ID3D11Device4) && m_pDevice4)
      return (ID3D11Device4 *)this;
    else if(iid == __uuidof(ID3D11Device5) && m_pDevice5)
      return (ID3D11Device5 *)this;
    return NULL;
  }
  virtual void FirstFrame(IDXGISwapper *swapper) { /* Simple wrapper - no frame capture */ }
  virtual void NewSwapchainBuffer(IUnknown *backbuffer) { /* Simple wrapper - no wrapping */ }
  virtual void ReleaseSwapchainResources(IDXGISwapper *swapper, UINT QueueCount,
                                         IUnknown *const *ppPresentQueue,
                                         IUnknown **unwrappedQueues) { /* Simple wrapper */ }
  virtual IUnknown *WrapSwapchainBuffer(IDXGISwapper *swapper, DXGI_FORMAT bufferFormat,
                                        UINT buffer, IUnknown *realSurface)
  {
    // Simple wrapper - return unwrapped surface
    if(realSurface)
      realSurface->AddRef();
    return realSurface;
  }
  virtual IDXGIResource *WrapExternalDXGIResource(IDXGIResource *res)
  {
    // Simple wrapper - return unwrapped resource
    if(res)
      res->AddRef();
    return res;
  }
  virtual HRESULT Present(IDXGISwapper *swapper, UINT SyncInterval, UINT Flags)
  {
    // Simple wrapper - no present interception
    return S_OK;
  }

  // IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  // ID3D11Device - all methods forward to m_pDevice
  void STDMETHODCALLTYPE GetImmediateContext(ID3D11DeviceContext **ppImmediateContext);
  HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC *pDesc,
                                         const D3D11_SUBRESOURCE_DATA *pInitialData,
                                         ID3D11Buffer **ppBuffer);
  HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc,
                                            const D3D11_SUBRESOURCE_DATA *pInitialData,
                                            ID3D11Texture1D **ppTexture1D);
  HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
                                            const D3D11_SUBRESOURCE_DATA *pInitialData,
                                            ID3D11Texture2D **ppTexture2D);
  HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
                                            const D3D11_SUBRESOURCE_DATA *pInitialData,
                                            ID3D11Texture3D **ppTexture3D);
  HRESULT STDMETHODCALLTYPE CreateShaderResourceView(ID3D11Resource *pResource,
                                                      const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                      ID3D11ShaderResourceView **ppSRView);
  HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D11Resource *pResource,
                                                       const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                                       ID3D11UnorderedAccessView **ppUAView);
  HRESULT STDMETHODCALLTYPE CreateRenderTargetView(ID3D11Resource *pResource,
                                                    const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
                                                    ID3D11RenderTargetView **ppRTView);
  HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource *pResource,
                                                    const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                    ID3D11DepthStencilView **ppDepthStencilView);
  HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
                                              UINT NumElements, const void *pShaderBytecodeWithInputSignature,
                                              SIZE_T BytecodeLength, ID3D11InputLayout **ppInputLayout);
  HRESULT STDMETHODCALLTYPE CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                                ID3D11ClassLinkage *pClassLinkage,
                                                ID3D11VertexShader **ppVertexShader);
  HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                                 ID3D11ClassLinkage *pClassLinkage,
                                                 ID3D11GeometryShader **ppGeometryShader);
  HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
      const void *pShaderBytecode, SIZE_T BytecodeLength,
      const D3D11_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries,
      const UINT *pBufferStrides, UINT NumStrides, UINT RasterizedStream,
      ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader);
  HRESULT STDMETHODCALLTYPE CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                              ID3D11ClassLinkage *pClassLinkage,
                                              ID3D11PixelShader **ppPixelShader);
  HRESULT STDMETHODCALLTYPE CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                              ID3D11ClassLinkage *pClassLinkage,
                                              ID3D11HullShader **ppHullShader);
  HRESULT STDMETHODCALLTYPE CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                               ID3D11ClassLinkage *pClassLinkage,
                                               ID3D11DomainShader **ppDomainShader);
  HRESULT STDMETHODCALLTYPE CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
                                                ID3D11ClassLinkage *pClassLinkage,
                                                ID3D11ComputeShader **ppComputeShader);
  HRESULT STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage **ppLinkage);
  HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
                                             ID3D11BlendState **ppBlendState);
  HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
                                                     ID3D11DepthStencilState **ppDepthStencilState);
  HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc,
                                                   ID3D11RasterizerState **ppRasterizerState);
  HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
                                               ID3D11SamplerState **ppSamplerState);
  HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery);
  HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc,
                                            ID3D11Predicate **ppPredicate);
  HRESULT STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc,
                                          ID3D11Counter **ppCounter);
  HRESULT STDMETHODCALLTYPE CreateDeferredContext(UINT ContextFlags,
                                                  ID3D11DeviceContext **ppDeferredContext);
  HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface,
                                                void **ppResource);
  HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport);
  HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount,
                                                           UINT *pNumQualityLevels);
  void STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo);
  HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType,
                                          UINT *pActiveCounters, LPSTR szName, UINT *pNameLength,
                                          LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription,
                                          UINT *pDescriptionLength);
  HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData,
                                                 UINT FeatureSupportDataSize);
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData);
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData);
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData);
  D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel(void);
  UINT STDMETHODCALLTYPE GetCreationFlags(void);
  HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason(void);
  HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags);
  UINT STDMETHODCALLTYPE GetExceptionMode(void);
  void STDMETHODCALLTYPE GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext);
  HRESULT STDMETHODCALLTYPE CreateDeferredContext1(UINT ContextFlags,
                                                   ID3D11DeviceContext1 **ppDeferredContext);
  HRESULT STDMETHODCALLTYPE CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc,
                                              ID3D11BlendState1 **ppBlendState);
  HRESULT STDMETHODCALLTYPE CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
                                                    ID3D11RasterizerState1 **ppRasterizerState);
  HRESULT STDMETHODCALLTYPE CreateDeviceContextState(UINT Flags,
                                                      const D3D_FEATURE_LEVEL *pFeatureLevels,
                                                      UINT FeatureLevels, UINT SDKVersion,
                                                      REFIID EmulatedInterface,
                                                      D3D_FEATURE_LEVEL *pChosenFeatureLevel,
                                                      ID3DDeviceContextState **ppContextState);
  HRESULT STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource, REFIID returnedInterface,
                                                 void **ppResource);
  HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess,
                                                     REFIID returnedInterface, void **ppResource);
  void STDMETHODCALLTYPE GetImmediateContext2(ID3D11DeviceContext2 **ppImmediateContext);
  HRESULT STDMETHODCALLTYPE CreateDeferredContext2(UINT ContextFlags,
                                                   ID3D11DeviceContext2 **ppDeferredContext);
  void STDMETHODCALLTYPE GetResourceTiling(ID3D11Resource *pTiledResource, UINT *pNumTilesForEntireResource,
                                               D3D11_PACKED_MIP_DESC *pPackedMipDesc,
                                               D3D11_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
                                               UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
                                               D3D11_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips);
  HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels1(DXGI_FORMAT Format, UINT SampleCount,
                                                            UINT Flags, UINT *pNumQualityLevels);
  void STDMETHODCALLTYPE GetImmediateContext3(ID3D11DeviceContext3 **ppImmediateContext);
  HRESULT STDMETHODCALLTYPE CreateDeferredContext3(UINT ContextFlags,
                                                    ID3D11DeviceContext3 **ppDeferredContext);
  void STDMETHODCALLTYPE WriteToSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                                                const D3D11_BOX *pDstBox, const void *pSrcData,
                                                UINT SrcRowPitch, UINT SrcDepthPitch);
  void STDMETHODCALLTYPE ReadFromSubresource(void *pDstData, UINT DstRowPitch, UINT DstDepthPitch,
                                                 ID3D11Resource *pSrcResource, UINT SrcSubresource,
                                                 const D3D11_BOX *pSrcBox);
  HRESULT STDMETHODCALLTYPE RegisterDeviceRemovedEvent(HANDLE hEvent, DWORD *pdwCookie);
  void STDMETHODCALLTYPE UnregisterDeviceRemoved(DWORD dwCookie);
  HRESULT STDMETHODCALLTYPE OpenSharedFence(HANDLE hFence, REFIID returnedInterface, void **ppFence);
  HRESULT STDMETHODCALLTYPE CreateFence(UINT64 InitialValue, D3D11_FENCE_FLAG Flags, REFIID returnedInterface,
                                         void **ppFence);
  // ID3D11Device3 methods
  HRESULT STDMETHODCALLTYPE CreateTexture2D1(const D3D11_TEXTURE2D_DESC1 *pDesc1,
                                             const D3D11_SUBRESOURCE_DATA *pInitialData,
                                             ID3D11Texture2D1 **ppTexture2D);
  HRESULT STDMETHODCALLTYPE CreateTexture3D1(const D3D11_TEXTURE3D_DESC1 *pDesc1,
                                             const D3D11_SUBRESOURCE_DATA *pInitialData,
                                             ID3D11Texture3D1 **ppTexture3D);
  HRESULT STDMETHODCALLTYPE CreateRasterizerState2(const D3D11_RASTERIZER_DESC2 *pRasterizerDesc,
                                                    ID3D11RasterizerState2 **ppRasterizerState);
  HRESULT STDMETHODCALLTYPE CreateShaderResourceView1(ID3D11Resource *pResource,
                                                        const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc1,
                                                        ID3D11ShaderResourceView1 **ppSRView1);
  HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView1(ID3D11Resource *pResource,
                                                         const D3D11_UNORDERED_ACCESS_VIEW_DESC1 *pDesc1,
                                                         ID3D11UnorderedAccessView1 **ppUAView1);
  HRESULT STDMETHODCALLTYPE CreateRenderTargetView1(ID3D11Resource *pResource,
                                                      const D3D11_RENDER_TARGET_VIEW_DESC1 *pDesc1,
                                                      ID3D11RenderTargetView1 **ppRTView1);
  HRESULT STDMETHODCALLTYPE CreateQuery1(const D3D11_QUERY_DESC1 *pQueryDesc1,
                                          ID3D11Query1 **ppQuery1);
};

