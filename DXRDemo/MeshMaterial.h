#pragma once

#include <unordered_map>

namespace DXRDemo
{
    enum class TextureType
    {
        Diffuse,
        SpecularMap,
        Ambient,
        Emissive,
        Heightmap,
        NormalMap,
        SpecularPowerMap,
        DisplacementMap,
        LightMap,
        End
    };

    struct Material final
    {
        std::unordered_map<TextureType, std::vector<std::string>> Textures;
    };
}