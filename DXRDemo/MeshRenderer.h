#pragma once

#include "Component.h"
#include <vector>
#include <memory>
#include "Mesh.h"

namespace DXRDemo
{
    class MeshRenderer : public Component
    {
    public:
        std::vector<std::shared_ptr<Mesh>> Meshes;
        //std::vector<std::shared_ptr<Material>> Materials;
    };
}