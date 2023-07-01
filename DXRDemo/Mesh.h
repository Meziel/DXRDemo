#pragma once

#include <vector>
#include <directxtk/SimpleMath.h>
#include <d3d12.h>
#include <wrl.h>
#include "MeshMaterial.h"

namespace DXRDemo
{
    class DXContext;

    struct Mesh final
    {
        std::vector<DirectX::SimpleMath::Vector3> Vertices;
        std::vector<std::vector<DirectX::SimpleMath::Vector4>> VertexColors;
        std::vector<DirectX::SimpleMath::Vector3> Normals;
        std::vector<std::uint32_t> Indices;
        std::shared_ptr<MeshMaterial> Material;
    };
}