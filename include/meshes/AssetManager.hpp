
#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "Mesh.hpp"
#include "StaticMesh.hpp"

class AssetManager
{
public:
    static AssetManager &instance();

    std::shared_ptr<Mesh> getMesh(const std::string &name);

    std::shared_ptr<Mesh> loadStaticMeshFromArrays(const std::string &name, const float *verts, size_t vertex_count, const unsigned int *indices, size_t index_count, int floats_per_vertex);

    // Load an OBJ file; infers vertex format (pos[,uv][,norm]) and stores unique vertices.
    // Returns existing asset if name already loaded.
    std::shared_ptr<Mesh> loadStaticMeshFromOBJ(const std::string &filePath, const std::string &name);

private:
    AssetManager() = default;
    ~AssetManager() = default;
    AssetManager(const AssetManager &) = delete;
    AssetManager &operator=(const AssetManager &) = delete;

    std::unordered_map<std::string, std::shared_ptr<Mesh>> assets;
};

#endif // ASSET_MANAGER_HPP
