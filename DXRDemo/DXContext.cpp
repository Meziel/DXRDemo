#include "DXContext.h"

#include "Utilities.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace DXRDemo
{
    DXContext::DXContext(const Window& window, uint32_t numBuffers) :
        _numBuffers(numBuffers)
    {
        //_EnableDebugLayer();

        _backBuffers.resize(numBuffers);

        // Initialize
        _tearingSupported = _CheckTearingSupport();

        // Create DirextX 12 objects
        Adapter = _GetAdapter(_useWarp);
        Device = _CreateDevice(Adapter);

        // Create command queues
        DirectCommandQueue = std::make_unique<CommandQueue>(Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        CopyCommandQueue = std::make_unique<CommandQueue>(Device, D3D12_COMMAND_LIST_TYPE_COPY);

        // Create swap chain
        _swapChain = _CreateSwapChain(window.GetHWND(),
            DirectCommandQueue->GetD3D12CommandQueue().Get(),
            window.GetWidth(),
            window.GetHeight(),
            numBuffers);
        _currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();
        
        // Create RTV descriptor heap
        _rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numBuffers, false);
        _rtvDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Create render target views
        _UpdateRenderTargetViews();
    }

    DXContext::~DXContext()
    {
        // DirectX Cleanup
        // Make sure the command queue has finished all commands before closing
        Flush();
    }

    void DXContext::SetVSync(bool enabled)
    {
        _vSyncEnabled = enabled;
    }

    bool DXContext::IsVSyncEnabled() const
    {
        return _vSyncEnabled;
    }

    void DXContext::SetRaytracing(bool enabled)
    {
        _raytracingEnabled = enabled;
    }

    bool DXContext::IsRaytracingEnabled() const
    {
        return _raytracingEnabled;
    }

    void DXContext::Flush()
    {
        DirectCommandQueue->Flush();
        CopyCommandQueue->Flush();
    }

    UINT DXContext::GetNumberBuffers() const
    {
        return _numBuffers;
    }

    UINT DXContext::GetCurrentBackBufferIndex() const
    {
        return _currentBackBufferIndex;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> DXContext::GetCurrentBackBuffer() const
    {
        return _backBuffers[_currentBackBufferIndex];
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DXContext::GetCurrentRenderTargetView() const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            _rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            _currentBackBufferIndex,
            _rtvDescriptorSize);
    }

    UINT DXContext::Present()
    {
        UINT syncInterval = _vSyncEnabled ? 1 : 0;
        UINT presentFlags = _tearingSupported && !_vSyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(_swapChain->Present(syncInterval, presentFlags));
        _currentBackBufferIndex = _swapChain->GetCurrentBackBufferIndex();

        return _currentBackBufferIndex;
    }

    ComPtr<ID3D12DescriptorHeap> DXContext::CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t numDescriptors,
        bool shaderVisible)
    {
        ComPtr<ID3D12DescriptorHeap> descriptorHeap;

        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = numDescriptors;
        desc.Type = type;
        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ThrowIfFailed(Device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

        return descriptorHeap;
    }

    UINT DXContext::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
    {
        return Device->GetDescriptorHandleIncrementSize(type);
    }

    bool DXContext::IsRaytracingSupported()
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
        ThrowIfFailed(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
        return options5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
    }

    void DXContext::UpdateBufferResource(ID3D12GraphicsCommandList4* commandList, ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData, D3D12_RESOURCE_FLAGS flags)
    {
        {
            size_t bufferSize = numElements * elementSize;

            // Create a committed resource for the GPU resource in a default heap
            CD3DX12_HEAP_PROPERTIES defaultProperties(D3D12_HEAP_TYPE_DEFAULT);
            CD3DX12_RESOURCE_DESC defaultResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
            ThrowIfFailed(Device->CreateCommittedResource(
                &defaultProperties,
                D3D12_HEAP_FLAG_NONE,
                &defaultResourceDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(pDestinationResource)));

            // Create an committed resource for the upload
            if (bufferData)
            {
                CD3DX12_HEAP_PROPERTIES uploadProperties(D3D12_HEAP_TYPE_UPLOAD);
                CD3DX12_RESOURCE_DESC uploadResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
                ThrowIfFailed(Device->CreateCommittedResource(
                    &uploadProperties,
                    D3D12_HEAP_FLAG_NONE,
                    &uploadResourceDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(pIntermediateResource)));

                D3D12_SUBRESOURCE_DATA subresourceData = {};
                subresourceData.pData = bufferData;
                subresourceData.RowPitch = bufferSize;
                subresourceData.SlicePitch = subresourceData.RowPitch;

                UpdateSubresources(commandList,
                    *pDestinationResource, *pIntermediateResource,
                    0, 0, 1, &subresourceData);
            }
        }
    }

    /// <summary>
    /// Enables errors to be caught by the debug layer when created
    /// DirextX 12 objects
    /// </summary>
    void DXContext::_EnableDebugLayer()
    {
#if defined(_DEBUG)
        // Always enable the debug layer before doing anything DX12 related
        // so all possible errors generated while creating DX12 objects
        // are caught by the debug layer.
        ComPtr<ID3D12Debug> debugInterface;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
        debugInterface->EnableDebugLayer();
#endif
    }

    /// <summary>
    /// Creates a DirectX adapter
    /// </summary>
    /// <param name="useWarp"></param>
    /// <returns></returns>
    ComPtr<IDXGIAdapter4> DXContext::_GetAdapter(bool useWarp)
    {
        ComPtr<IDXGIFactory4> dxgiFactory;
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

        ComPtr<IDXGIAdapter1> dxgiAdapter1;
        ComPtr<IDXGIAdapter4> dxgiAdapter4;

        if (useWarp)
        {
            ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
            ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
        }

        else
        {
            SIZE_T maxDedicatedVideoMemory = 0;
            for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
                dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

                // Check to see if the adapter can create a D3D12 device without actually 
                // creating it. The adapter with the largest dedicated video memory
                // is favored.
                if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                    SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
                        D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)) &&
                    dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
                {
                    maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                    ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
                }
            }
        }

        return dxgiAdapter4;
    }

    /// <summary>
    /// Creates a DirectX 12 device
    /// </summary>
    /// <param name="adapter"></param>
    /// <returns></returns>
    ComPtr<ID3D12Device5> DXContext::_CreateDevice(ComPtr<IDXGIAdapter4> adapter)
    {
        ComPtr<ID3D12Device5> d3d12Device5;
        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device5)));

        // Enable debug messages in debug mode.
#if defined(_DEBUG)
        ComPtr<ID3D12InfoQueue> pInfoQueue;
        if (SUCCEEDED(d3d12Device5.As(&pInfoQueue)))
        {
            //pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            //pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            //pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);


            // Suppress whole categories of messages
            //D3D12_MESSAGE_CATEGORY Categories[] = {};

            // Suppress messages based on their severity level
            D3D12_MESSAGE_SEVERITY Severities[] =
            {
                D3D12_MESSAGE_SEVERITY_INFO
            };

            // Suppress individual messages by their ID
            D3D12_MESSAGE_ID DenyIds[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
            };

            D3D12_INFO_QUEUE_FILTER NewFilter = {};
            //NewFilter.DenyList.NumCategories = _countof(Categories);
            //NewFilter.DenyList.pCategoryList = Categories;
            NewFilter.DenyList.NumSeverities = _countof(Severities);
            NewFilter.DenyList.pSeverityList = Severities;
            NewFilter.DenyList.NumIDs = _countof(DenyIds);
            NewFilter.DenyList.pIDList = DenyIds;

            //ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
        }
#endif

        return d3d12Device5;
    }

    /// <summary>
    /// Checks if variable refresh rate is supported. If it is true is returned; otherwise false.
    /// </summary>
    /// <returns></returns>
    bool DXContext::_CheckTearingSupport()
    {
        BOOL allowTearing = FALSE;

        // Rather than create the DXGI 1.5 factory interface directly, we create the
        // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
        // graphics debugging tools which will not support the 1.5 factory interface 
        // until a future update.
        ComPtr<IDXGIFactory4> factory4;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
        {
            ComPtr<IDXGIFactory5> factory5;
            if (SUCCEEDED(factory4.As(&factory5)))
            {
                if (FAILED(factory5->CheckFeatureSupport(
                    DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                    &allowTearing, sizeof(allowTearing))))
                {
                    allowTearing = FALSE;
                }
            }
        }

        return allowTearing == TRUE;
    }

    /// <summary>
    /// Creates a swap chain
    /// </summary>
    /// <param name="hWnd"></param>
    /// <param name="commandQueue"></param>
    /// <param name="width"></param>
    /// <param name="height"></param>
    /// <param name="bufferCount"></param>
    /// <returns></returns>
    ComPtr<IDXGISwapChain4> DXContext::_CreateSwapChain(
        HWND hWnd,
        ComPtr<ID3D12CommandQueue> commandQueue,
        uint32_t width,
        uint32_t height,
        uint32_t bufferCount)
    {
        ComPtr<IDXGISwapChain4> dxgiSwapChain4;
        ComPtr<IDXGIFactory4> dxgiFactory4;
        UINT createFactoryFlags = 0;
#if defined(_DEBUG)
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc = { 1, 0 };
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = bufferCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        // It is recommended to always allow tearing if tearing support is available.
        swapChainDesc.Flags = _CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        ComPtr<IDXGISwapChain1> swapChain1;
        ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
            commandQueue.Get(),
            hWnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1));

        // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
        // will be handled manually.
        ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

        ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

        return dxgiSwapChain4;
    }

    /// <summary>
    /// Update Render Target Views
    /// </summary>
    /// <param name="device"></param>
    /// <param name="swapChain"></param>
    /// <param name="descriptorHeap"></param>
    void DXContext::_UpdateRenderTargetViews()
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (uint32_t i = 0; i < _numBuffers; ++i)
        {
            ComPtr<ID3D12Resource> backBuffer;
            ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

            Device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

            _backBuffers[i] = backBuffer;

            rtvHandle.Offset(_rtvDescriptorSize);
        }
    }
}