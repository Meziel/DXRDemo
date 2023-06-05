#include "Game.h"

#include <wrl.h>
#include <chrono>
#include <d3dcompiler.h>

using namespace DirectX;
using namespace Microsoft::WRL;

namespace Portals
{
    Game::Game(DXContext& dxContext, uint32_t width, uint32_t height) :
        _dxContext(&dxContext),
        _viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
    {
        _fenceValues.resize(dxContext.GetNumberBuffers());
        _CreatePipelineState();
    }

    void Game::Update()
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

        // Update the model matrix
        _modelMatrix = XMMatrixIdentity();

        // Update the view matrix
        const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
        const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
        const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
        _viewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

        // Update the projection matrix
        float aspectRatio = _viewport.Width / static_cast<float>(_viewport.Height);
        _projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(_fov), aspectRatio, 0.1f, 100.0f);
    }

    void Game::Render()
    {
        CommandQueue& directCommandQueue = *_dxContext->DirectCommandQueue;
        auto directCommandList = directCommandQueue.GetCommandList();
        
        UINT currentBackBufferIndex = _dxContext->GetCurrentBackBufferIndex();
        auto backBuffer = _dxContext->GetCurrentBackBuffer();
        auto rtv = _dxContext->GetCurrentRenderTargetView();
        auto dsv = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
        
        // Clear the render targets
        {
            _TransitionResource(directCommandList, backBuffer,
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET);

            FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

            _ClearRTV(directCommandList, rtv, clearColor);
            _ClearDepth(directCommandList, dsv);
        }

        directCommandList->SetPipelineState(_pipelineState.Get());
        directCommandList->SetGraphicsRootSignature(_rootSignature.Get());

        directCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        directCommandList->IASetVertexBuffers(0, 1, &_vertexBufferView);
        directCommandList->IASetIndexBuffer(&_indexBufferView);

        directCommandList->RSSetViewports(1, &_viewport);
        directCommandList->RSSetScissorRects(1, &_scissorRect);

        directCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        // Update the MVP matrix
        XMMATRIX mvpMatrix = XMMatrixMultiply(_modelMatrix, _viewMatrix);
        mvpMatrix = XMMatrixMultiply(mvpMatrix, _projectionMatrix);
        directCommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0);

        directCommandList->DrawIndexedInstanced(_countof(_indicies), 1, 0, 0, 0);

        // Present
        {
            _TransitionResource(directCommandList, backBuffer,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT);

            _fenceValues[_dxContext->GetCurrentBackBufferIndex()] = directCommandQueue.ExecuteCommandList(directCommandList);

            _dxContext->Present();

            directCommandQueue.WaitForFenceValue(_fenceValues[_dxContext->GetCurrentBackBufferIndex()]);
        }
    }

    void Game::_CreatePipelineState()
    {
        auto device = _dxContext->Device;
        CommandQueue& copyCommandQueue = *_dxContext->CopyCommandQueue;
        auto commandList = copyCommandQueue.GetCommandList();

        // Upload vertex buffer data.
        ComPtr<ID3D12Resource> intermediateVertexBuffer;
        _UpdateBufferResource(commandList,
            &_vertexBuffer, &intermediateVertexBuffer,
            _countof(_vertices), sizeof(VertexPosColor), _vertices);

        // Create the vertex buffer view.
        _vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
        _vertexBufferView.SizeInBytes = sizeof(_vertices);
        _vertexBufferView.StrideInBytes = sizeof(VertexPosColor);

        // Upload index buffer data.
        ComPtr<ID3D12Resource> intermediateIndexBuffer;
        _UpdateBufferResource(commandList,
            &_indexBuffer, &intermediateIndexBuffer,
            _countof(_indicies), sizeof(WORD), _indicies);

        // Create index buffer view.
        _indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
        _indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        _indexBufferView.SizeInBytes = sizeof(_indicies);

        // Create the descriptor heap for the depth-stencil view.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&_dsvHeap)));

        // Load the vertex shader.
        ComPtr<ID3DBlob> vertexShaderBlob;
        ThrowIfFailed(D3DReadFileToBlob(L"..//x64//Debug//VertexShader.cso", &vertexShaderBlob));

        // Load the pixel shader.
        ComPtr<ID3DBlob> pixelShaderBlob;
        ThrowIfFailed(D3DReadFileToBlob(L"..//x64//Debug//PixelShader.cso", &pixelShaderBlob));

        // Create the vertex input layout
        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        // Create a root signature.
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Allow input layout and deny unnecessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        // A single 32-bit constant root parameter that is used by the vertex shader.
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        // Serialize the root signature.
        ComPtr<ID3DBlob> rootSignatureBlob;
        ComPtr<ID3DBlob> errorBlob;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
            featureData.HighestVersion, &rootSignatureBlob, &errorBlob));
        // Create the root signature.
        ThrowIfFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));

        struct PipelineStateStream
        {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } pipelineStateStream;

        D3D12_RT_FORMAT_ARRAY rtvFormats = {};
        rtvFormats.NumRenderTargets = 1;
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        pipelineStateStream.pRootSignature = _rootSignature.Get();
        pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
        pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
        pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipelineStateStream.RTVFormats = rtvFormats;

        D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
            sizeof(PipelineStateStream), &pipelineStateStream
        };
        ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&_pipelineState)));

        auto fenceValue = copyCommandQueue.ExecuteCommandList(commandList);
        copyCommandQueue.WaitForFenceValue(fenceValue);

        // Resize/Create the depth buffer.
        _ResizeDepthBuffer(static_cast<int>(_viewport.Width), static_cast<int>(_viewport.Height));
    }

    void Game::_ResizeDepthBuffer(int width, int height)
    {
        // Flush any GPU commands that might be referencing the depth buffer.
        _dxContext->Flush();

        width = std::max(1, width);
        height = std::max(1, height);

        auto device = _dxContext->Device;

        // Resize screen dependent resources.
        // Create a depth buffer.
        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        optimizedClearValue.DepthStencil = { 1.0f, 0 };

        CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resourcedDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
            1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourcedDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optimizedClearValue,
            IID_PPV_ARGS(&_depthBuffer)
        ));

        // Update the depth-stencil view.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice = 0;
        dsv.Flags = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(_depthBuffer.Get(), &dsv,
            _dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Transition a resource
    void Game::_TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> resource,
        D3D12_RESOURCE_STATES beforeState,
        D3D12_RESOURCE_STATES afterState)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.Get(),
            beforeState, afterState);

        commandList->ResourceBarrier(1, &barrier);
    }

    void Game::_ClearRTV(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv,
        FLOAT* clearColor)
    {
        commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    void Game::_ClearDepth(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv,
        FLOAT depth)
    {
        commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    }

    void Game::_UpdateBufferResource(
        ComPtr<ID3D12GraphicsCommandList2> commandList,
        ID3D12Resource** pDestinationResource,
        ID3D12Resource** pIntermediateResource,
        size_t numElements, size_t elementSize, const void* bufferData,
        D3D12_RESOURCE_FLAGS flags)
    {
        auto device = _dxContext->Device;

        size_t bufferSize = numElements * elementSize;

        // Create a committed resource for the GPU resource in a default heap
        CD3DX12_HEAP_PROPERTIES defaultProperties(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC defaultResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
        ThrowIfFailed(device->CreateCommittedResource(
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
            ThrowIfFailed(device->CreateCommittedResource(
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

            UpdateSubresources(commandList.Get(),
                *pDestinationResource, *pIntermediateResource,
                0, 0, 1, &subresourceData);
        }
    }
}