#pragma once

#include <string>
#include <memory>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "GameObject.h"
#include "Mesh.h"

using namespace std;

namespace DXRDemo
{
    class AssetImporter final
    {
    public:
        std::unique_ptr<GameObject> ImportAsset(const std::string& filename);

    private:
        std::vector<std::shared_ptr<Mesh>> _meshes;
        std::vector<std::shared_ptr<MeshMaterial>> _materials;

        std::unique_ptr<MeshMaterial> _CreateMaterial(const aiMaterial& aiMaterial) const;
        std::unique_ptr<Mesh> _CreateMesh(
            const aiMesh& aiMesh,
            const std::unordered_map<unsigned int, std::shared_ptr<MeshMaterial>>& materialMap) const;
        std::unique_ptr<GameObject> _CreateGameObjectFromNode(
            const aiNode& aiNode,
            const std::unordered_map<unsigned int, std::shared_ptr<Mesh>>& meshMap);
    };
}
