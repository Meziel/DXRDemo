#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <d3dx12.h>
#include <directxmath.h>

#include <chrono>

#include "Window.h"

namespace Portals
{
    class DXContext
    {
    public:
        DXContext(const Window& window);
        DXContext(const DXContext&) = delete;
        DXContext(DXContext&&) noexcept = delete;
        DXContext& operator=(const DXContext& rhs) = delete;
        DXContext& operator=(DXContext&&) noexcept = delete;
        ~DXContext();
    
        // The number of swap chain back buffers.
        static constexpr uint8_t _numFrames = 3;

        Microsoft::WRL::ComPtr<ID3D12Resource> _backBuffers[_numFrames];
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _commandAllocators[_numFrames];
        UINT _currentBackBufferIndex;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvDescriptorHeap;
        UINT _rtvDescriptorSize;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> _commandQueue;
        // By default, enable V-Sync.
        // Can be toggled with the V key.
        bool _vSync = true;
        bool _tearingSupported = false;
        Microsoft::WRL::ComPtr<IDXGISwapChain4> _swapChain;

        // Synchronization objects
        Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
        uint64_t _fenceValue = 0;
        uint64_t _frameFenceValues[_numFrames] = {};
        HANDLE _fenceEvent;

        uint64_t _Signal(
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
            Microsoft::WRL::ComPtr<ID3D12Fence> fence,
            uint64_t& fenceValue);

        void _WaitForFenceValue(
            Microsoft::WRL::ComPtr<ID3D12Fence> fence,
            uint64_t fenceValue,
            HANDLE fenceEvent,
            std::chrono::milliseconds duration = std::chrono::milliseconds::max());

    private:

        // Use WARP adapter
        bool _useWarp = false;

        // DirectX 12 Objects
        Microsoft::WRL::ComPtr<ID3D12Device2> _device;

        void _EnableDebugLayer();
        
        Microsoft::WRL::ComPtr<IDXGIAdapter4> _GetAdapter(bool useWarp);
        
        Microsoft::WRL::ComPtr<ID3D12Device2> _CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
        
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> _CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
        
        bool _CheckTearingSupport();
        
        Microsoft::WRL::ComPtr<IDXGISwapChain4> _CreateSwapChain(
            HWND hWnd,
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
            uint32_t width,
            uint32_t height,
            uint32_t bufferCount);
        
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _CreateDescriptorHeap(
            Microsoft::WRL::ComPtr<ID3D12Device2> device,
            D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);

        void _UpdateRenderTargetViews(
            Microsoft::WRL::ComPtr<ID3D12Device2> device,
            Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap);

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _CreateCommandAllocator(
            Microsoft::WRL::ComPtr<ID3D12Device2> device,
            D3D12_COMMAND_LIST_TYPE type);

        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _CreateCommandList(
            Microsoft::WRL::ComPtr<ID3D12Device2> device,
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator,
            D3D12_COMMAND_LIST_TYPE type);

        Microsoft::WRL::ComPtr<ID3D12Fence> _CreateFence(Microsoft::WRL::ComPtr<ID3D12Device2> device);

        HANDLE _CreateEventHandle();

        void _Flush(
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
            Microsoft::WRL::ComPtr<ID3D12Fence> fence,
            uint64_t& fenceValue,
            HANDLE fenceEvent);
    };
}