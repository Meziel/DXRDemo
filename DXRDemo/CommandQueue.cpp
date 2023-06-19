#include "CommandQueue.h"
#include "Utilities.h"
#include <cassert>

using namespace Microsoft::WRL;

namespace DXRDemo
{
    CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) :
        _device(device),
        _commandListType(type),
        _fenceValue(0)
    {
        // Create command queue
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = type;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;

        ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&_d3d12CommandQueue)));
        
        // Create fence
        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));

        // Create event handle
        _fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
        assert(_fenceEvent && "Failed to create fence event.");
    }

    CommandQueue::~CommandQueue()
    {
        ::CloseHandle(_fenceEvent);
    }

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> CommandQueue::GetCommandList(ID3D12PipelineState* pipelineState)
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = _GetCommandAllocator();
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList;

        // Reuse in object pool if possible
        if (!_commandLists.empty())
        {
            commandList = _commandLists.front();
            _commandLists.pop();

            ThrowIfFailed(commandList->Reset(commandAllocator.Get(), pipelineState));
        }
        else
        {
            commandList = _CreateCommandList(commandAllocator, pipelineState);
        }

        // Associate the command allocator with the command list so that it can be
        // retrieved when the command list is executed.
        ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

        return commandList;
    }

    uint64_t CommandQueue::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList)
    {
        ThrowIfFailed(commandList->Close());

        // Get command allocator associated with this command list
        ID3D12CommandAllocator* commandAllocator;
        UINT dataSize = sizeof(commandAllocator);
        ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

        ID3D12CommandList* const commandLists[] = {
            commandList.Get()
        };

        _d3d12CommandQueue->ExecuteCommandLists(1, commandLists);
        uint64_t fenceValue = Signal();

        // Command allocator must be waited on before being reused
        _commandAllocators.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });
        // Command list can be reused immediately after
        _commandLists.push(commandList);

        // The ownership of the command allocator has been transferred to the ComPtr
        // in the command allocator queue. It is safe to release the reference 
        // in this temporary COM pointer here.
        commandAllocator->Release();

        return fenceValue;
    }

    uint64_t CommandQueue::Signal()
    {
        uint64_t fenceValue = ++_fenceValue;
        _d3d12CommandQueue->Signal(_fence.Get(), fenceValue);
        return fenceValue;
    }

    bool CommandQueue::IsFenceComplete(uint64_t fenceValue) const
    {
        return _fence->GetCompletedValue() >= fenceValue;
    }

    void CommandQueue::WaitForFenceValue(uint64_t fenceValue) const
    {
        if (!IsFenceComplete(fenceValue))
        {
            _fence->SetEventOnCompletion(fenceValue, _fenceEvent);
            ::WaitForSingleObject(_fenceEvent, DWORD_MAX);
        }
    }

    void CommandQueue::Flush()
    {
        WaitForFenceValue(Signal());
    }

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
    {
        return _d3d12CommandQueue;
    }

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::_GetCommandAllocator()
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;

        if (!_commandAllocators.empty() && IsFenceComplete(_commandAllocators.front().fenceValue))
        {
            commandAllocator = _commandAllocators.front().commandAllocator;
            _commandAllocators.pop();

            ThrowIfFailed(commandAllocator->Reset());
            return commandAllocator;
        }
        else
        {
            commandAllocator = _CreateCommandAllocator();
        }

        return commandAllocator;
    }

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::_CreateCommandAllocator()
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        ThrowIfFailed(_device->CreateCommandAllocator(_commandListType, IID_PPV_ARGS(&commandAllocator)));

        return commandAllocator;
    }

    ComPtr<ID3D12GraphicsCommandList4> CommandQueue::_CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator, ID3D12PipelineState* pipelineState)
    {
        ComPtr<ID3D12GraphicsCommandList4> commandList;
        ThrowIfFailed(_device->CreateCommandList(0, _commandListType, allocator.Get(), pipelineState, IID_PPV_ARGS(&commandList)));

        return commandList;
    }
}