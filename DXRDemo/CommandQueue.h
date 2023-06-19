#pragma once

#include <d3d12.h> 
#include <wrl.h>
#include <cstdint>
#include <queue>

namespace DXRDemo
{
    class CommandQueue final
    {
    public:
        CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
        CommandQueue(const CommandQueue&) = delete;
        CommandQueue(CommandQueue&&) noexcept = delete;
        CommandQueue& operator=(const CommandQueue&) = delete;
        CommandQueue& operator=(CommandQueue&&) noexcept = delete;
        ~CommandQueue();

        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> GetCommandList(ID3D12PipelineState* pipelineState = nullptr);
        uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList);
        uint64_t Signal();
        bool IsFenceComplete(uint64_t fenceValue) const;
        void WaitForFenceValue(uint64_t fenceValue) const;
        void Flush();
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

    private:
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _GetCommandAllocator();
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _CreateCommandAllocator();
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> _CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator, ID3D12PipelineState* pipelineState = nullptr);
    
        struct CommandAllocatorEntry
        {
            uint64_t fenceValue;
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        };

        Microsoft::WRL::ComPtr<ID3D12Device2>       _device;
        D3D12_COMMAND_LIST_TYPE                     _commandListType;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue>  _d3d12CommandQueue;
        std::queue<CommandAllocatorEntry>           _commandAllocators;
        std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>> _commandLists;

        // Signaling
        Microsoft::WRL::ComPtr<ID3D12Fence>         _fence;
        HANDLE                                      _fenceEvent;
        uint64_t                                    _fenceValue;
    };
}