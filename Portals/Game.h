#pragma once

#include "DXContext.h"

namespace Portals
{
    class Game final
    {
    public:
        Game(DXContext& dxContext, uint32_t width, uint32_t height);

        void Update();
        void Render();
    
    private:

        struct VertexPosColor
        {
            DirectX::XMFLOAT3 Position;
            DirectX::XMFLOAT3 Color;
        };

        inline const static VertexPosColor _vertices[8] = {
            { DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
            { DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
            { DirectX::XMFLOAT3(1.0f,  1.0f, -1.0f),  DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
            { DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f),  DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
            { DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
            { DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
            { DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f),  DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
            { DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f),  DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
        };

        inline const static WORD _indicies[36] =
        {
            0, 1, 2, 0, 2, 3,
            4, 6, 5, 4, 7, 6,
            4, 5, 1, 4, 1, 0,
            3, 2, 6, 3, 6, 7,
            1, 5, 6, 1, 6, 2,
            4, 0, 3, 4, 3, 7
        };

        DXContext* _dxContext;
        std::vector<uint64_t> _fenceValues;

        void _CreatePipelineState();

        void _ResizeDepthBuffer(int width, int height);

        // Helper functions
        // Transition a resource
        void _TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            Microsoft::WRL::ComPtr<ID3D12Resource> resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState);

        // Clear a render target view
        void _ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE rtv,
            FLOAT* clearColor);

        // Clear the depth of a depth-stencil view
        void _ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE dsv,
            FLOAT depth = 1.0f);

        // Create a GPU buffer
        void _UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
            ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
            size_t numElements, size_t elementSize, const void* bufferData,
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

        // Vertex buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;

        // Index buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
        D3D12_INDEX_BUFFER_VIEW _indexBufferView;

        // Depth buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> _depthBuffer;

        // Descriptor heap for depth buffer
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap;

        // Root signature
        Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;

        // Pipeline state object
        Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;

        D3D12_VIEWPORT _viewport;
        D3D12_RECT _scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

        float _fov = 45.0f;

        DirectX::XMMATRIX _modelMatrix;
        DirectX::XMMATRIX _viewMatrix;
        DirectX::XMMATRIX _projectionMatrix;
    };
}