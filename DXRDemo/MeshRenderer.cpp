#include "MeshRenderer.h"
#include <d3d12.h>
#include "DxContext.h"
#include "DXRHelper.h"
#include "DXRUtils/BottomLevelASGenerator.h"

using namespace Microsoft::WRL;
using namespace DirectX::SimpleMath;
using namespace nv_helpers_dx12;

namespace DXRDemo
{
    void MeshRenderer::CreateBuffers(
        DXContext& dxContext,
        ID3D12GraphicsCommandList4* commandList)
    {
        VertexBuffers.resize(Meshes.size());
        IndexBuffers.resize(Meshes.size());

        uint32_t meshIndex = 0;
        for (auto& mesh : Meshes)
        {
            bool hasColor = mesh->VertexColors.size() > 0;

            // Create 'GPU' vertices to transfer to buffers
            std::vector<VertexPosColor> gpuVertices;
            for (size_t i = 0; i < mesh->Vertices.size(); ++i)
            {
                VertexPosColor gpuVertex;
                gpuVertex.Position = mesh->Vertices[i];
                gpuVertex.Color = hasColor && i < mesh->VertexColors[0].size() ? mesh->VertexColors[0][i] : Vector4();

                gpuVertices.push_back(std::move(gpuVertex));
            }

            ComPtr<ID3D12Resource> intermediateBuffer;
            // Vertex buffer 
            {
                dxContext.UpdateBufferResource(
                    commandList,
                    &VertexBuffers[meshIndex],
                    &intermediateBuffer,
                    gpuVertices.size(),
                    sizeof(VertexPosColor),
                    gpuVertices.data(),
                    D3D12_RESOURCE_FLAG_NONE);
            }

            // Index buffer
            {
                dxContext.UpdateBufferResource(commandList,
                    &IndexBuffers[meshIndex],
                    &intermediateBuffer,
                    mesh->Indices.size(),
                    sizeof(int32_t),
                    mesh->Indices.data(),
                    D3D12_RESOURCE_FLAG_NONE);
            } 

            ++meshIndex;
        }
    }

    void MeshRenderer::CreateBufferViews(DXContext& dxContext)
    {
        VertexBufferViews.resize(Meshes.size());
        IndexBufferViews.resize(Meshes.size());

        uint32_t meshIndex = 0;
        for (auto& mesh : Meshes)
        {
            // Vertex buffer view
            {
                VertexBufferViews[meshIndex].BufferLocation = VertexBuffers[meshIndex]->GetGPUVirtualAddress();
                VertexBufferViews[meshIndex].SizeInBytes = static_cast<UINT>(mesh->Vertices.size() * sizeof(VertexPosColor));
                VertexBufferViews[meshIndex].StrideInBytes = sizeof(VertexPosColor);
            }

            // Index buffer view
            {
                IndexBufferViews[meshIndex].BufferLocation = IndexBuffers[meshIndex]->GetGPUVirtualAddress();
                IndexBufferViews[meshIndex].Format = DXGI_FORMAT_R32_UINT;
                IndexBufferViews[meshIndex].SizeInBytes = sizeof(uint32_t);
            }

            ++meshIndex;
        }
    }

    void MeshRenderer::CreateBottomLevelAS(DXContext& dxContext, ID3D12GraphicsCommandList4* commandList)
    {
        BottomLevelASBuffers.clear();

        uint32_t meshIndex = 0;
        for (auto& mesh : Meshes)
        {
            using VVertexBuffer = std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t>>;

            BottomLevelASGenerator bottomLevelAS; // Adding all vertex buffers and not transforming their position.
            VVertexBuffer vVertexBuffers({ {VertexBuffers[meshIndex].Get(), static_cast<uint32_t>(mesh->Vertices.size())} });
            VVertexBuffer vIndexBuffers({ {IndexBuffers[meshIndex].Get(), static_cast<uint32_t>(mesh->Indices.size())}});

            for (size_t i = 0; i < vVertexBuffers.size(); ++i)
            {
                if (i < vIndexBuffers.size() && vIndexBuffers[i].second > 0)
                {
                    bottomLevelAS.AddVertexBuffer(
                        vVertexBuffers[i].first.Get(), 0,
                        vVertexBuffers[i].second, sizeof(MeshRenderer::VertexPosColor),
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
            bottomLevelAS.ComputeASBufferSizes(dxContext.Device.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

            // Once the sizes are obtained, the application is responsible for allocating
            // the necessary buffers. Since the entire generation will be done on the GPU,
            // we can directly allocate those on the default heap
            AccelerationStructureBuffers buffers;
            buffers.pScratch = nv_helpers_dx12::CreateBuffer(dxContext.Device.Get(), scratchSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);
            buffers.pResult = nv_helpers_dx12::CreateBuffer(dxContext.Device.Get(), resultSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nv_helpers_dx12::kDefaultHeapProps);

            // Build the acceleration structure. Note that this call integrates a barrier
            // on the generated AS, so that it can be used to compute a top-level AS right
            // after this method.
            bottomLevelAS.Generate(commandList, buffers.pScratch.Get(), buffers.pResult.Get(), false, nullptr);
            BottomLevelASBuffers.push_back(std::move(buffers));

            ++meshIndex;
        }

        auto fenceValue = dxContext.DirectCommandQueue->ExecuteCommandList(commandList);
        dxContext.DirectCommandQueue->WaitForFenceValue(fenceValue);
    }
}
