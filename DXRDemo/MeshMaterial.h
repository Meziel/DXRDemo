#pragma once

#include <unordered_map>
#include <string>

namespace DXRDemo
{
    struct MeshMaterial final
    {
        std::string Name;
        DirectX::SimpleMath::Vector4 DiffuseColor;
        DirectX::SimpleMath::Vector3 EmissionColor;
    };
}