#pragma once

#include "Component.h"
#include <vector>
#include <memory>
#include "Mesh.h"

namespace DXRDemo
{
    class DXContext;

    // TODO: move somewhere else
    struct AccelerationStructureBuffers
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> pScratch; // Scratch memory for AS builder
        Microsoft::WRL::ComPtr<ID3D12Resource> pResult; // Where the AS is
        Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
    };

    class MeshRenderer : public Component
    {
    public:

        struct VertexPosColor
        {
            DirectX::XMFLOAT3 Position;
            DirectX::XMFLOAT4 Color;
        };

        std::vector<std::shared_ptr<Mesh>> Meshes;

        // Vertex buffer
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> VertexBuffers;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> UploadVertexBuffers;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> UploadIndexBuffers;
        std::vector<D3D12_VERTEX_BUFFER_VIEW> VertexBufferViews;

        // Index buffer
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> IndexBuffers;
        std::vector<D3D12_INDEX_BUFFER_VIEW> IndexBufferViews;

        // Acceleration structure buffers
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> BottomLevelASBuffers;

        void CreateBuffers(DXContext& dxContext, ID3D12GraphicsCommandList4* commandList);
        void CreateBufferViews(DXContext& dxContext);
        void CreateBottomLevelAS(
            DXContext& dxContext,
            ID3D12GraphicsCommandList4* commandList);
    };
}