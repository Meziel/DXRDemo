#include "Game.h"

#include <wrl.h>
#include <chrono>
#include <d3dcompiler.h>

#include "DXRHelper.h"
#include "DRXUtils/BottomLevelASGenerator.h"
#include "DRXUtils/RaytracingPipelineGenerator.h"
#include "DRXUtils/RootSignatureGenerator.h"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace nv_helpers_dx12;

namespace DRXDemo
{
    Game::Game(Window& window, DXContext& dxContext, uint32_t width, uint32_t height) :
        _window(&window),
        _dxContext(&dxContext),
        _viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
    {
        _fenceValues.resize(dxContext.GetNumberBuffers());
        _OnInit();
    }

    void Game::Update()
    {
        static uint64_t frameCounter = 0;
        static double elapsedSeconds = 0.0;
        static std::chrono::high_resolution_clock clock;
        static auto t0 = clock.now();
        static bool rayTracingEnabled = false;
        static bool printedAnything = false;
        static float rotation[3] = { 0 };

        ++frameCounter;
        auto t1 = clock.now();
        auto deltaTime = t1 - t0;
        t0 = t1;

        double secondsSinceLastTick = deltaTime.count() * 1e-9;
        elapsedSeconds += secondsSinceLastTick;

        const float secondsPerRotation = 10;

        rotation[0] += 2 * XM_PI / secondsPerRotation * secondsSinceLastTick;
        rotation[1] += 2 * XM_PI / secondsPerRotation * secondsSinceLastTick;

        if (elapsedSeconds > 1.0)
        {
            char buffer[500];
            auto fps = frameCounter / elapsedSeconds;
            sprintf_s(buffer, 500, "FPS: %f\n", fps);
            OutputDebugStringA(buffer);

            frameCounter = 0;
            elapsedSeconds = 0.0;
        }

        // Print whether we are raytracing
        if (!printedAnything || _dxContext->IsRaytracingEnabled() != rayTracingEnabled)
        {
            if (_dxContext->IsRaytracingEnabled())
            {
                OutputDebugStringA("Raytracing Enabled\n");
            }
            else
            {
                OutputDebugStringA("Rasterization Enabled\n");
            }

            printedAnything = true;
            rayTracingEnabled = _dxContext->IsRaytracingEnabled();
        }

        // Update the model matrix
        _modelMatrix = XMMatrixRotationAxis({ 1.f, 0.f, 0.f }, rotation[0]);
        _modelMatrix *= XMMatrixRotationAxis({ 0.f, 1.f, 0.f }, rotation[1]);
        _modelMatrix *= XMMatrixRotationAxis({ 0.f, 0.f, 1.f }, rotation[2]);

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
        auto directCommandList = directCommandQueue.GetCommandList(_pipelineState.Get());
        
        UINT currentBackBufferIndex = _dxContext->GetCurrentBackBufferIndex();
        auto backBuffer = _dxContext->GetCurrentBackBuffer();
        auto rtv = _dxContext->GetCurrentRenderTargetView();
        auto dsv = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

        directCommandList->SetGraphicsRootSignature(_rootSignature.Get());
        directCommandList->RSSetViewports(1, &_viewport);
        directCommandList->RSSetScissorRects(1, &_scissorRect);

        directCommandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        // Raster
        if (!_dxContext->IsRaytracingEnabled())
        {
            // Update the MVP matrix
            XMMATRIX mvpMatrix = XMMatrixMultiply(_modelMatrix, _viewMatrix);
            mvpMatrix = XMMatrixMultiply(mvpMatrix, _projectionMatrix);

            // Indicate that the back buffer will be used as a render target
            _TransitionResource(directCommandList, backBuffer,
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET);

            // Clear the render targets
            _ClearRTV(directCommandList, rtv, _clearColor);
            _ClearDepth(directCommandList, dsv);

            //directCommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0);
            CopyDataToBuffer(_mvpBuffer, &mvpMatrix, sizeof(mvpMatrix));
            directCommandList->SetGraphicsRootConstantBufferView(0, _mvpBuffer->GetGPUVirtualAddress());

            directCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            directCommandList->IASetVertexBuffers(0, 1, &_vertexBufferView);
            directCommandList->IASetIndexBuffer(&_indexBufferView);

            // Draw command
            directCommandList->DrawIndexedInstanced(_countof(_indicies), 1, 0, 0, 0);

            _TransitionResource(directCommandList, backBuffer,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT);
        }
        // Ray tracing
        else
        {
            // Update acceleration structures
            m_instances[0].second = _modelMatrix;
            CreateTopLevelAS(directCommandList, m_instances, true);

            // Update inverse view and projection matrices
            XMMATRIX inverseProjectionMatrix = XMMatrixInverse(nullptr, _projectionMatrix);
            CopyDataToBuffer(_inverseProjectBuffer, &inverseProjectionMatrix, sizeof(inverseProjectionMatrix));

            XMMATRIX inverseViewMatrix = XMMatrixInverse(nullptr, _viewMatrix);
            CopyDataToBuffer(_inverseViewBuffer, &inverseViewMatrix, sizeof(inverseViewMatrix));

            // Set descriptor heap
            std::vector<ID3D12DescriptorHeap*> heaps = { m_srvUavHeap.Get() };
            directCommandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());

            // Transition output buffer from copy to unordered access (
            CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            directCommandList->ResourceBarrier(1, &transition);

            // Setup raytracing task
            D3D12_DISPATCH_RAYS_DESC desc = {};
            desc.RayGenerationShaderRecord.StartAddress = m_sbtStorage->GetGPUVirtualAddress();
            desc.RayGenerationShaderRecord.SizeInBytes = m_sbtHelper.GetRayGenSectionSize();
            
            // Required to be 64 bit aligned
            desc.MissShaderTable.StartAddress = ROUND_UP(m_sbtStorage->GetGPUVirtualAddress(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) +
                ROUND_UP(m_sbtHelper.GetRayGenSectionSize(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
            desc.MissShaderTable.SizeInBytes = m_sbtHelper.GetMissSectionSize();
            desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

            // Required to be 64 bit aligned
            desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() +
                ROUND_UP(m_sbtHelper.GetRayGenSectionSize(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) +
                ROUND_UP(m_sbtHelper.GetMissSectionSize(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
            desc.HitGroupTable.SizeInBytes = m_sbtHelper.GetHitGroupSectionSize();
            desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();
           
            desc.Width = static_cast<UINT>(_viewport.Width);
            desc.Height = static_cast<UINT>(_viewport.Height);
            desc.Depth = 1;
            directCommandList->SetPipelineState1(m_rtStateObject.Get());
            directCommandList->DispatchRays(&desc);

            // Copy RT output image to render target

            // Transition output from unordered access to copy source
            transition = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
            directCommandList->ResourceBarrier(1, &transition);
            
            // Transition render target from render target to copy dest
            transition = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            directCommandList->ResourceBarrier(1, &transition);
            
            // Copy raytrace output to render target
            directCommandList->CopyResource(backBuffer.Get(), m_outputResource.Get());

            transition = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PRESENT);
            directCommandList->ResourceBarrier(1, &transition);
        }

        // Present
         _fenceValues[_dxContext->GetCurrentBackBufferIndex()] = directCommandQueue.ExecuteCommandList(directCommandList);
         _dxContext->Present();
         directCommandQueue.WaitForFenceValue(_fenceValues[_dxContext->GetCurrentBackBufferIndex()]);
    }

    void Game::OnKeyUp(uint8_t key)
    {
    }

    void Game::OnKeyDown(uint8_t key)
    {
        switch (key)
        {
            case 'V':
                _dxContext->SetVSync(!_dxContext->IsVSyncEnabled());
                break;
            case VK_SPACE:
                _dxContext->SetRaytracing(!_dxContext->IsRaytracingEnabled());
                break;
            case VK_ESCAPE:
                _window->Quit();
                break;
            default:
                break;
        }
    }

    void Game::_OnInit()
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
            _countof(_indicies), sizeof(int32_t), _indicies);

        // Create index buffer view.
        _indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
        _indexBufferView.Format = DXGI_FORMAT_R32_UINT;
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
        rootParameters[0].InitAsConstantBufferView(0);

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

        // Create CBVs
        Game::CreateBuffer(sizeof(_clearColor), &_clearColorBuffer);
        CopyDataToBuffer(_clearColorBuffer, _clearColor, sizeof(_clearColor));

        Game::CreateBuffer(sizeof(DirectX::XMMATRIX), &_mvpBuffer);
        Game::CreateBuffer(sizeof(DirectX::XMMATRIX), &_inverseProjectBuffer);
        Game::CreateBuffer(sizeof(DirectX::XMMATRIX), &_inverseViewBuffer);

        CreateAccelerationStructures();
        CreateRaytracingPipeline();
        CreateRaytracingOutputBuffer();
        CreateShaderResourceHeap();
        CreateShaderBindingTable();
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
    void Game::_TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
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
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv,
        const FLOAT* clearColor)
    {
        commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }

    void Game::_ClearDepth(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv,
        FLOAT depth)
    {
        commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    }

    void Game::_UpdateBufferResource(
        ComPtr<ID3D12GraphicsCommandList4> commandList,
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

    //-----------------------------------------------------------------------------
    //
    // Create a bottom-level acceleration structure based on a list of vertex
    // buffers in GPU memory along with their vertex count. The build is then done
    // in 3 steps: gathering the geometry, computing the sizes of the required
    // buffers, and building the actual AS
    //
    Game::AccelerationStructureBuffers Game::CreateBottomLevelAS(
        ComPtr<ID3D12GraphicsCommandList4> commandList,
        std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers,
        std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t>> vIndexBuffers)
    {

        nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS; // Adding all vertex buffers and not transforming their position.
        for (size_t i = 0; i < vVertexBuffers.size(); ++i)
        {
            if (i < vIndexBuffers.size() && vIndexBuffers[i].second > 0)
            {
                bottomLevelAS.AddVertexBuffer(
                    vVertexBuffers[i].first.Get(), 0,
                    vVertexBuffers[i].second, sizeof(VertexPosColor),
                    vIndexBuffers[i].first.Get(), 0,
                    vIndexBuffers[i].second, nullptr, 0, true);
            }
            else
            {
                bottomLevelAS.AddVertexBuffer(
                    vVertexBuffers[i].first.Get(), 0,
                    vVertexBuffers[i].second, sizeof(VertexPosColor), 0,
                    0);
            }    
        }
            
        // The AS build requires some scratch space to store temporary information.
        // The amount of scratch memory is dependent on the scene complexity.
        UINT64 scratchSizeInBytes = 0;
            
        // The final AS also needs to be stored in addition to the existing vertex
        // buffers. It size is also dependent on the scene complexity.
        UINT64 resultSizeInBytes = 0;
        bottomLevelAS.ComputeASBufferSizes(_dxContext->Device.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);
            
        // Once the sizes are obtained, the application is responsible for allocating
        // the necessary buffers. Since the entire generation will be done on the GPU,
        // we can directly allocate those on the default heap
        AccelerationStructureBuffers buffers;
        buffers.pScratch = nv_helpers_dx12::CreateBuffer(_dxContext->Device.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);
        buffers.pResult = nv_helpers_dx12::CreateBuffer(_dxContext->Device.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);
            
        // Build the acceleration structure. Note that this call integrates a barrier
        // on the generated AS, so that it can be used to compute a top-level AS right
        // after this method.
        bottomLevelAS.Generate(commandList.Get(), buffers.pScratch.Get(), buffers.pResult.Get(), false, nullptr);
        return buffers;
    }

    void Game::CreateTopLevelAS(
        ComPtr<ID3D12GraphicsCommandList4> commandList,
        const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances,
        bool updateOnly)
    {
        if (!updateOnly)
        {
            // Gather all the instances into the builder helper
            for (size_t i = 0; i < instances.size(); i++)
            {
                m_topLevelASGenerator.AddInstance(instances[i].first.Get(), instances[i].second, static_cast<uint32_t>(i), static_cast<uint32_t>(0));
            }
            // As for the bottom-level AS, the building the AS requires some scratch space
            // to store temporary data in addition to the actual AS. In the case of the
            // top-level AS, the instance descriptors also need to be stored in GPU
            // memory. This call outputs the memory requirements for each (scratch,
            // results, instance descriptors) so that the application can allocate the
            // corresponding memory  
            UINT64 scratchSize, resultSize, instanceDescsSize;
            m_topLevelASGenerator.ComputeASBufferSizes(_dxContext->Device.Get(), true, &scratchSize, &resultSize, &instanceDescsSize);

            // Create the scratch and result buffers. Since the build is all done on GPU,
            // those can be allocated on the default heap
            m_topLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(_dxContext->Device.Get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps);
            m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(_dxContext->Device.Get(), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);

            // The buffer describing the instances: ID, shader binding information,
            // matrices ... Those will be copied into the buffer by the helper through
            // mapping, so the buffer has to be allocated on the upload heap.
            m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(_dxContext->Device.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
        }

        // After all the buffers are allocated, or if only an update is required, we
        // can build the acceleration structure. Note that in the case of the update
        // we also pass the existing AS as the 'previous' AS, so that it can be
        // refitted in place.
        m_topLevelASGenerator.Generate(
            commandList.Get(),
            m_topLevelASBuffers.pScratch.Get(),
            m_topLevelASBuffers.pResult.Get(),
            m_topLevelASBuffers.pInstanceDesc.Get(),
            updateOnly,
            m_topLevelASBuffers.pResult.Get());
    }

    /// Create all acceleration structures, bottom and top
    void Game::CreateAccelerationStructures()
    {
        CommandQueue& directCommandQueue = *_dxContext->DirectCommandQueue;
        auto directCommandList = directCommandQueue.GetCommandList(_pipelineState.Get());

        // Build the bottom AS from the Triangle vertex buffer
        AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS(
            directCommandList,
            { {_vertexBuffer.Get(), static_cast<uint32_t>(_countof(_vertices))}},
            { {_indexBuffer.Get(), static_cast<uint32_t>(_countof(_indicies))}});
        
        // Just one instance for now
        m_instances = {{bottomLevelBuffers.pResult, _modelMatrix}}; 
        CreateTopLevelAS(directCommandList, m_instances);
        
        // Flush the command list and wait for it to finish

        auto fenceValue = directCommandQueue.ExecuteCommandList(directCommandList);
        directCommandQueue.WaitForFenceValue(fenceValue);
        
        // Store the AS buffers. The rest of the buffers will be released once we exit the function
        m_bottomLevelAS = bottomLevelBuffers.pResult;
    }

    ComPtr<ID3D12RootSignature> Game::CreateRayGenSignature()
    {
        nv_helpers_dx12::RootSignatureGenerator rsc;
        // Inverse projection CBV
        rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 0);
        // Inverse view CBV
        rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV, 1);
        // Heap
        rsc.AddHeapRangesParameter({
            {
                0 /*u0*/,
                1 /*1 descriptor */,
                0 /*use the implicit register space 0*/,
                D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
                0 /*heap slot where the UAV is defined*/
            },
            {
                0 /*t0*/,
                1,
                0,
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/,
                1} 
            });
        return rsc.Generate(_dxContext->Device.Get(), true);
    }

    ComPtr<ID3D12RootSignature> Game::CreateHitSignature()
    {
        nv_helpers_dx12::RootSignatureGenerator rsc;
        rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 0); // Vertices
        rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV, 1); // Indices
        return rsc.Generate(_dxContext->Device.Get(), true);
    }

    ComPtr<ID3D12RootSignature> Game::CreateMissSignature()
    {
        nv_helpers_dx12::RootSignatureGenerator rsc;
        // Clear Color
        rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_CBV);
        return rsc.Generate(_dxContext->Device.Get(), true);
    }

    void Game::CreateRaytracingPipeline()
    {
        // To be used, each DX12 shader needs a root signature defining which
        // parameters and buffers will be accessed.
        m_rayGenSignature = CreateRayGenSignature();
        m_missSignature = CreateMissSignature();
        m_hitSignature = CreateHitSignature();

        ThrowIfFailed(D3DReadFileToBlob(L"..//x64//Debug//RayGen.cso", &m_rayGenLibrary));
        ThrowIfFailed(D3DReadFileToBlob(L"..//x64//Debug//Miss.cso", &m_missLibrary));
        ThrowIfFailed(D3DReadFileToBlob(L"..//x64//Debug//Hit.cso", &m_hitLibrary));

        nv_helpers_dx12::RayTracingPipelineGenerator pipeline(_dxContext->Device.Get());

        pipeline.AddLibrary(m_rayGenLibrary.Get(), { L"RayGen" });
        pipeline.AddLibrary(m_missLibrary.Get(), { L"Miss" });
        pipeline.AddLibrary(m_hitLibrary.Get(), { L"ClosestHit" });

        pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");

        pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), {L"RayGen"});
        pipeline.AddRootSignatureAssociation(m_missSignature.Get(), {L"Miss"});
        pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), {L"HitGroup"});

        pipeline.SetMaxPayloadSize(4 * sizeof(float)); // RGB + distance

        pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

        pipeline.SetMaxRecursionDepth(1);

        m_rtStateObject = pipeline.Generate();
        ThrowIfFailed(m_rtStateObject->QueryInterface(IID_PPV_ARGS(&m_rtStateObjectProps)));
    }

    void Game::CreateRaytracingOutputBuffer()
    {
        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.DepthOrArraySize = 1;
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB formats cannot be used
        // with UAVs. For accuracy we should convert to sRGB ourselves in the shader
        resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resDesc.Width = static_cast<uint64_t>(_viewport.Width);
        resDesc.Height = static_cast<uint64_t>(_viewport.Height);
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;
        ThrowIfFailed(_dxContext->Device->CreateCommittedResource(
            &nv_helpers_dx12::kDefaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&m_outputResource)));
    }

    void Game::CreateShaderResourceHeap()
    {
        m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap(_dxContext->Device.Get(), 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

        // Unordered access view (Output image)
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        _dxContext->Device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, srvHandle);

        srvHandle.ptr += _dxContext->Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Shared resource view (Acceleration structure)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = m_topLevelASBuffers.pResult->GetGPUVirtualAddress();

        _dxContext->Device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
    }

    void Game::CreateShaderBindingTable()
    {
        m_sbtHelper.Reset();

        D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
        auto heapPointer = reinterpret_cast<void*>(srvUavHeapHandle.ptr);
        m_sbtHelper.AddRayGenerationProgram(L"RayGen", {
            reinterpret_cast<void*>(_inverseProjectBuffer->GetGPUVirtualAddress()),
            reinterpret_cast<void*>(_inverseViewBuffer->GetGPUVirtualAddress()),
            heapPointer
        });
        m_sbtHelper.AddMissProgram(L"Miss", { reinterpret_cast<void*>(_clearColorBuffer->GetGPUVirtualAddress()) });
        m_sbtHelper.AddHitGroup(L"HitGroup", {
            reinterpret_cast<void*>(_vertexBuffer->GetGPUVirtualAddress()),
            reinterpret_cast<void*>(_indexBuffer->GetGPUVirtualAddress()),
        });

        const uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();
        m_sbtStorage = nv_helpers_dx12::CreateBuffer(_dxContext->Device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
        if (!m_sbtStorage)
        {
            throw std::logic_error("Could not allocate the shader binding table");
        }

        m_sbtHelper.Generate(m_sbtStorage.Get(), m_rtStateObjectProps.Get());
    }

    void Game::CreateBuffer(size_t bufferSize, ID3D12Resource** buffer)
    {        
        *buffer = nv_helpers_dx12::CreateBuffer(
            _dxContext->Device.Get(),
            bufferSize,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nv_helpers_dx12::kUploadHeapProps);
    }

    void Game::CopyDataToBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> buffer, void* data, size_t bufferSize)
    {
        // Copy CPU memory to GPU
        uint8_t* pData;
        ThrowIfFailed(buffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, data, bufferSize);
        buffer->Unmap(0, nullptr);
    }
}