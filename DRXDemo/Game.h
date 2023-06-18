#pragma once

#include "DXContext.h"
#include "DRXUtils/TopLevelASGenerator.h"
#include "DRXUtils/ShaderBindingTableGenerator.h"
#include "dxcapi.h"

namespace DRXDemo
{
    class Game final
    {
    public:
        Game(Window& window, DXContext& dxContext, uint32_t width, uint32_t height);

        void Update();
        void Render();
        void OnKeyUp(uint8_t key);
        void OnKeyDown(uint8_t key);
    
    private:

        struct VertexPosColor
        {
            DirectX::XMFLOAT3 Position;
            DirectX::XMFLOAT3 Color;
        };

        inline const static VertexPosColor _vertices[8] = {
            //{ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
            //{ DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
            //{ DirectX::XMFLOAT3(1.0f,  1.0f, -1.0f),  DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
            //{ DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f),  DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
            //{ DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
            //{ DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
            //{ DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f),  DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
            //{ DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f),  DirectX::XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7

            { { 0.0f, 0.25f, 0.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f } },
            { { 0.25f, -0.25f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f } },
            { { -0.25f, -0.25f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f } }
        };

        inline const static WORD _indicies[36] =
        {
            //0, 1, 2, 0, 2, 3,
            //4, 6, 5, 4, 7, 6,
            //4, 5, 1, 4, 1, 0,
            //3, 2, 6, 3, 6, 7,
            //1, 5, 6, 1, 6, 2,
            //4, 0, 3, 4, 3, 7
            0,1,2
        };

        Window* _window;
        DXContext* _dxContext;
        std::vector<uint64_t> _fenceValues;

        void _OnInit();

        void _ResizeDepthBuffer(int width, int height);

        // Helper functions
        // Transition a resource
        void _TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            Microsoft::WRL::ComPtr<ID3D12Resource> resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState);

        // Clear a render target view
        void _ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE rtv,
            const float* clearColor);

        // Clear the depth of a depth-stencil view
        void _ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE dsv,
            float depth = 1.0f);

        // Create a GPU buffer
        void _UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
            size_t numElements, size_t elementSize, const void* bufferData,
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

        // Descriptor heap for depth buffer
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap;

        // Vertex buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
        D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;

        // Index buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
        D3D12_INDEX_BUFFER_VIEW _indexBufferView;

        // Depth buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> _depthBuffer;

        // Pipeline state object
        Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;

        // Root signature
        Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;

        D3D12_VIEWPORT _viewport;
        D3D12_RECT _scissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

        float _fov = 45.0f;
        float _clearColor[4] = { 0.4f, 0.6f, 0.9f, 1.0f };

        DirectX::XMMATRIX _modelMatrix;
        DirectX::XMMATRIX _viewMatrix;
        DirectX::XMMATRIX _projectionMatrix;

        struct AccelerationStructureBuffers
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> pScratch; // Scratch memory for AS builder
            Microsoft::WRL::ComPtr<ID3D12Resource> pResult; // Where the AS is
            Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
        };

        Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAS; // Storage for the bottom Level AS
        nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
        AccelerationStructureBuffers m_topLevelASBuffers;
        std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> m_instances;

        /// Create the acceleration structure of an instance
        ///
        /// \param vVertexBuffers : pair of buffer and vertex count
        /// \return AccelerationStructureBuffers for TLAS
        AccelerationStructureBuffers CreateBottomLevelAS(
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers);
        
        /// Create the main acceleration structure that holds
        /// all instances of the scene
        /// \param instances : pair of BLAS and transform
        void CreateTopLevelAS(
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances);
        
        /// Create all acceleration structures, bottom and top
        void CreateAccelerationStructures();

        // DXR
        Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRayGenSignature();
        Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateHitSignature();
        Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateMissSignature();
        void CreateRaytracingPipeline();
        Microsoft::WRL::ComPtr<ID3DBlob> m_rayGenLibrary;
        Microsoft::WRL::ComPtr<ID3DBlob> m_hitLibrary;
        Microsoft::WRL::ComPtr<ID3DBlob> m_missLibrary;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rayGenSignature;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_hitSignature;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_missSignature;
        // Ray tracing pipeline state
        Microsoft::WRL::ComPtr<ID3D12StateObject> m_rtStateObject;
        // Ray tracing pipeline state properties, retaining the shader identifiers
        // to use in the Shader Binding Table
        Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> m_rtStateObjectProps;

        // Allocate the buffer storing the raytracing output, with the same dimensions
        // as the target image
        void CreateRaytracingOutputBuffer();
        void CreateShaderResourceHeap();
        Microsoft::WRL::ComPtr<ID3D12Resource> m_outputResource;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;

        void CreateShaderBindingTable();
        nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_sbtStorage;

        void CreateGlobalConstantBuffer();
        Microsoft::WRL::ComPtr<ID3D12Resource> m_globalConstantBuffer;
    };
}