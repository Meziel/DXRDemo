#pragma once

#include <directxmath.h>
#include <combaseapi.h>
#include <dxcapi.h>
#include "DXContext.h"
#include "DXRUtils/TopLevelASGenerator.h"
#include "DXRUtils/ShaderBindingTableGenerator.h"
#include "Scene.h"
#include "MeshRenderer.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>

namespace DXRDemo
{
    class Game final
    {
    public:
        Game(Window& window, uint32_t width, uint32_t height);
        ~Game();

        void Update();
        void Render();
        void OnKeyUp(uint8_t key);
        void OnKeyDown(uint8_t key);

        Scene Scene;

        double FPS = 0;

    private:

        Window* _window;
        DXContext _dxContext;
        std::vector<uint64_t> _fenceValues;

        void _OnInit();
        void _CreateBuffers();
        void _CreateDescriptorHeaps();
        void _CreateBufferViews();
        // Rasterization init
        void _CreateRasterizationRootSignature();
        void _CreateRasterizationPipeline();

        void _InitializeGUI();


        //void _ResizeDepthBuffer(int width, int height);

        // Helper functions
        // Transition a resource
        void _TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            Microsoft::WRL::ComPtr<ID3D12Resource> resource,
            D3D12_RESOURCE_STATES beforeState,
            D3D12_RESOURCE_STATES afterState);

        // Clear a render target view
        void _ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE rtv,
            const FLOAT* clearColor);

        // Clear the depth of a depth-stencil view
        void _ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            D3D12_CPU_DESCRIPTOR_HANDLE dsv,
            FLOAT depth = 1.0f);

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

        DirectX::XMMATRIX _modelMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX _viewMatrix;
        DirectX::XMMATRIX _projectionMatrix;

        nv_helpers_dx12::TopLevelASGenerator TopLevelASGenerator;
        AccelerationStructureBuffers TopLevelASBuffers;
        std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> ASInstances;

        /// Create the acceleration structure of an instance
        ///
        /// \param vVertexBuffers : pair of buffer and vertex count
        /// \return AccelerationStructureBuffers for TLAS
        AccelerationStructureBuffers CreateBottomLevelAS(
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList,
            std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t>> vVertexBuffers,
            std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t>> vIndexBuffers = {});
        
        /// Create the main acceleration structure that holds
        /// all instances of the scene
        /// \param instances : pair of BLAS and transform
        void CreateTopLevelAS(
            ID3D12GraphicsCommandList4* commandList,
            const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>& instances,
            bool updateOnly = false);
        
        /// Create all acceleration structures, bottom and top
        void CreateAccelerationStructures();

        // DXR
        Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRayGenSignature();
        Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateHitSignature();
        Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateMissSignature();
        Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateShadowSignature();
        void CreateRaytracingPipeline();
        Microsoft::WRL::ComPtr<ID3DBlob> m_rayGenLibrary;
        Microsoft::WRL::ComPtr<ID3DBlob> m_hitLibrary;
        Microsoft::WRL::ComPtr<ID3DBlob> m_missLibrary;
        Microsoft::WRL::ComPtr<ID3DBlob> m_shadowLibrary;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rayGenSignature;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_hitSignature;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_missSignature;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_shadowSignature;
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

        void CreateBuffer(size_t bufferSize, ID3D12Resource** buffer);
        void CopyDataToBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> buffer, void* data, size_t bufferSize);
        Microsoft::WRL::ComPtr<ID3D12Resource> _clearColorBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> _inverseProjectBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> _inverseViewBuffer;
    };
}