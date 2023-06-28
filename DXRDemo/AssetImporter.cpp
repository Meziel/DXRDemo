#include "AssetImporter.h"
#include "MeshRenderer.h"
#include <stdexcept>

using namespace DirectX::SimpleMath;
using namespace DirectX;

namespace DXRDemo
{

    std::unique_ptr<GameObject> AssetImporter::ImportAsset(const std::string& filename)
    {
        Assimp::Importer importer;

        const uint32_t flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_FlipWindingOrder;

        const aiScene* scene = importer.ReadFile(filename, flags);

        if (scene == nullptr)
        {
            throw std::runtime_error("Could not read from file");
        }

        std::unordered_map<unsigned int, std::shared_ptr<Mesh>> meshMap;
        meshMap.reserve(scene->mNumMeshes);

        // Create meshes
        if (scene->HasMeshes())
        {
            for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
            {

                _meshes.push_back(_CreateMesh(*scene->mMeshes[i]));
                const std::shared_ptr<Mesh>& newMesh = _meshes.back();
                meshMap.insert({i, newMesh});
            }
        }

        // TODO: Create materials
        {

        }

        // Create GameObjects from nodes
        return _CreateGameObjectFromNode(*scene->mRootNode, meshMap);
    }

    std::unique_ptr<Mesh> AssetImporter::_CreateMesh(const aiMesh& meshData) const
    {
        unique_ptr<Mesh> mesh = make_unique<Mesh>();

        // Copy vertices
        for (unsigned int i = 0; i < meshData.mNumVertices; ++i)
        {
            mesh->Vertices.emplace_back(reinterpret_cast<const float*>(&meshData.mVertices[i]));
        }

        // Copy vertex colors
        {
            unsigned int colorChannelCount = meshData.GetNumColorChannels();
            mesh->VertexColors.reserve(colorChannelCount);
            for (unsigned int i = 0; i < colorChannelCount; i++)
            {
                vector<Vector4> vertexColors;
                vertexColors.reserve(meshData.mNumVertices);
                aiColor4D* aiVertexColors = meshData.mColors[i];
                for (unsigned int j = 0; j < meshData.mNumVertices; j++)
                {
                    vertexColors.emplace_back(reinterpret_cast<const float*>(&aiVertexColors[j]));
                }
                mesh->VertexColors.push_back(move(vertexColors));
            }
        }
        
        // Copy faces
        if (meshData.HasFaces())
        {
            for (uint32_t i = 0; i < meshData.mNumFaces; i++)
            {
                aiFace* face = &meshData.mFaces[i];

                for (unsigned int j = 0; j < face->mNumIndices; j++)
                {
                    mesh->Indices.push_back(face->mIndices[j]);
                }
            }
        }

        return mesh;
    }

    std::unique_ptr<GameObject> AssetImporter::_CreateGameObjectFromNode(const aiNode& aiNode, const std::unordered_map<unsigned int, std::shared_ptr<Mesh>>& meshMap)
    {
        std::unique_ptr<GameObject> gameObject = std::make_unique<GameObject>();

        Matrix m(
            aiNode.mTransformation.a1, aiNode.mTransformation.a2, aiNode.mTransformation.a3, aiNode.mTransformation.a4,
            aiNode.mTransformation.b1, aiNode.mTransformation.b2, aiNode.mTransformation.b3, aiNode.mTransformation.b4,
            aiNode.mTransformation.c1, aiNode.mTransformation.c2, aiNode.mTransformation.c3, aiNode.mTransformation.c4,
            aiNode.mTransformation.d1, aiNode.mTransformation.d2, aiNode.mTransformation.d3, aiNode.mTransformation.d4);

        m.Decompose(gameObject->Transform.Scale, gameObject->Transform.Rotation, gameObject->Transform.Position);

        // Create MeshRenderer component
        if (aiNode.mNumMeshes > 0)
        {
            gameObject->Components.push_back(std::make_shared<MeshRenderer>());
            MeshRenderer& meshRenderer = static_cast<MeshRenderer&>(*gameObject->Components.back());

            for (unsigned int i = 0; i < aiNode.mNumMeshes; ++i)
            {
                meshRenderer.Meshes.push_back(meshMap.at(aiNode.mMeshes[i]));
            }
        }

        // Populate children
        for (unsigned int i = 0; i < aiNode.mNumChildren; ++i)
        {
            gameObject->Children.emplace_back(_CreateGameObjectFromNode(*aiNode.mChildren[i], meshMap));
        }

        return gameObject;
    }
}