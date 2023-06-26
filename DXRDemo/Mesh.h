#pragma once

#include <vector>
#include <directxtk/SimpleMath.h>

namespace DXRDemo
{
    struct Mesh final
    {
        std::vector<DirectX::SimpleMath::Vector3> Vertices;
        std::vector<std::vector<DirectX::SimpleMath::Vector4>> VertexColors;
        std::vector<DirectX::SimpleMath::Vector3> Normals;
        std::vector<std::uint32_t> Indices;
    };
}