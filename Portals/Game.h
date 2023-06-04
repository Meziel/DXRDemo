#pragma once

#include "DXContext.h"

namespace Portals
{
    class Game
    {
    public:
        inline Game(DXContext& dxContext)
        {
            _dxContext = &dxContext;
        }

        inline void Update()
        {
            static uint64_t frameCounter = 0;
            static double elapsedSeconds = 0.0;
            static std::chrono::high_resolution_clock clock;
            static auto t0 = clock.now();

            ++frameCounter;
            auto t1 = clock.now();
            auto deltaTime = t1 - t0;
            t0 = t1;

            elapsedSeconds += deltaTime.count() * 1e-9;
            if (elapsedSeconds > 1.0)
            {
                char buffer[500];
                auto fps = frameCounter / elapsedSeconds;
                sprintf_s(buffer, 500, "FPS: %f\n", fps);
                OutputDebugStringA(buffer);

                frameCounter = 0;
                elapsedSeconds = 0.0;
            }
        }

        inline void Render()
        {
            auto commandAllocator = _dxContext->_commandAllocators[_dxContext->_currentBackBufferIndex];
            auto backBuffer = _dxContext->_backBuffers[_dxContext->_currentBackBufferIndex];

            commandAllocator->Reset();
            _dxContext->_commandList->Reset(commandAllocator.Get(), nullptr);

            // Clear the render target.
            {
                CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    backBuffer.Get(),
                    D3D12_RESOURCE_STATE_PRESENT,
                    D3D12_RESOURCE_STATE_RENDER_TARGET);

                _dxContext->_commandList->ResourceBarrier(1, &barrier);

                // TODO: Remove magic numbers
                FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

                // Get descriptor handle of RTV using descriptor heap
                CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
                    _dxContext->_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                    _dxContext->_currentBackBufferIndex,
                    _dxContext->_rtvDescriptorSize);

                _dxContext->_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            }

            // Present
            {
                CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    backBuffer.Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT);
                _dxContext->_commandList->ResourceBarrier(1, &barrier);

                ThrowIfFailed(_dxContext->_commandList->Close());

                ID3D12CommandList* const commandLists[] = {
                    _dxContext->_commandList.Get()
                };
                _dxContext->_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

                UINT syncInterval = _dxContext->_vSync ? 1 : 0;
                UINT presentFlags = _dxContext->_tearingSupported && !_dxContext->_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
                ThrowIfFailed(_dxContext->_swapChain->Present(syncInterval, presentFlags));

                _dxContext->_frameFenceValues[_dxContext->_currentBackBufferIndex] = _dxContext->_Signal(
                    _dxContext->_commandQueue,
                    _dxContext->_fence,
                    _dxContext->_fenceValue);

                // After signaling, back buffer index is updated
                _dxContext->_currentBackBufferIndex = _dxContext->_swapChain->GetCurrentBackBufferIndex();

                _dxContext->_WaitForFenceValue(_dxContext->_fence, _dxContext->_frameFenceValues[_dxContext->_currentBackBufferIndex], _dxContext->_fenceEvent);
            }
        }
    
    private:
        DXContext* _dxContext;
    };
}