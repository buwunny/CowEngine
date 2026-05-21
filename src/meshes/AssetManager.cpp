#include "meshes/AssetManager.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <memory>

AssetManager &AssetManager::instance()
{
    static AssetManager inst;
    return inst;
}

std::shared_ptr<Mesh> AssetManager::getMesh(const std::string &name)
{
    auto it = assets.find(name);
    if (it != assets.end())
        return it->second;
    return nullptr;
}

std::shared_ptr<Mesh> AssetManager::loadStaticMeshFromArrays(const std::string &name, const float *verts, size_t vertex_count, const unsigned int *indices, size_t index_count, int floats_per_vertex)
{
    auto existing = getMesh(name);
    if (existing)
        return existing;

    auto mesh = std::make_shared<StaticMesh>(verts, vertex_count, indices, index_count, floats_per_vertex);
    assets[name] = mesh;
    return mesh;
}

// Parse a simple subset of OBJ: v, vt, vn, f. Triangulates quads.
std::shared_ptr<Mesh> AssetManager::loadStaticMeshFromOBJ(const std::string &filePath, const std::string &name)
{
    auto existing = getMesh(name);
    if (existing)
        return existing;

    // Resolve candidate paths: try given path, ASSET_ROOT + path, relative parents
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(filePath);
#ifdef ASSET_ROOT
    candidates.emplace_back(std::filesystem::path(ASSET_ROOT) / filePath);
#endif
    candidates.emplace_back(std::filesystem::path("./") / filePath);
    candidates.emplace_back(std::filesystem::path("../") / filePath);
    candidates.emplace_back(std::filesystem::path("../../") / filePath);

    std::filesystem::path chosen;
    for (auto &c : candidates)
    {
        if (std::filesystem::exists(c))
        {
            chosen = c;
            break;
        }
    }

    if (chosen.empty())
    {
        std::cerr << "AssetManager: failed to find OBJ file: " << filePath << " (tried several candidate locations)" << std::endl;
        return nullptr;
    }

    std::ifstream in(chosen);
    if (!in)
    {
        std::cerr << "AssetManager: failed to open resolved OBJ file: " << chosen << std::endl;
        return nullptr;
    }
    std::cerr << "AssetManager: loading OBJ from: " << chosen << std::endl;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;

    struct Index
    {
        int p, t, n;
    };

    // Map a triple string to new vertex index
    std::unordered_map<std::string, unsigned int> indexMap;
    std::vector<float> outVertices;
    std::vector<unsigned int> outIndices;

    bool hasUV = false, hasNormal = false;

    std::string line;
    while (std::getline(in, line))
    {
        if (line.size() < 2)
            continue;
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "v")
        {
            glm::vec3 p;
            ss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        }
        else if (prefix == "vt")
        {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            uvs.push_back(uv);
            hasUV = true;
        }
        else if (prefix == "vn")
        {
            glm::vec3 n;
            ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
            hasNormal = true;
        }
        else if (prefix == "f")
        {
            std::vector<std::string> verts;
            std::string v;
            while (ss >> v)
                verts.push_back(v);
            // triangulate polygon fan
            for (size_t i = 1; i + 1 < verts.size(); ++i)
            {
                std::string tri[3] = {verts[0], verts[i], verts[i + 1]};
                for (int k = 0; k < 3; ++k)
                {
                    const std::string &entry = tri[k];
                    auto it = indexMap.find(entry);
                    if (it != indexMap.end())
                    {
                        outIndices.push_back(it->second);
                        continue;
                    }

                    // parse v/vt/vn or v//vn or v/vt
                    int vi = 0, ti = 0, ni = 0;
                    size_t p1 = entry.find('/');
                    if (p1 == std::string::npos)
                    {
                        vi = std::stoi(entry);
                    }
                    else
                    {
                        size_t p2 = entry.find('/', p1 + 1);
                        vi = std::stoi(entry.substr(0, p1));
                        if (p2 == std::string::npos)
                        {
                            // v/vt
                            ti = std::stoi(entry.substr(p1 + 1));
                        }
                        else
                        {
                            if (p2 == p1 + 1)
                            {
                                // v//vn
                                ni = std::stoi(entry.substr(p2 + 1));
                            }
                            else
                            {
                                // v/vt/vn
                                ti = std::stoi(entry.substr(p1 + 1, p2 - p1 - 1));
                                ni = std::stoi(entry.substr(p2 + 1));
                            }
                        }
                    }

                    // OBJ indices are 1-based, handle negative indices
                    auto fixIndex = [](int idx, size_t size) -> int
                    {
                        if (idx > 0)
                            return idx - 1;
                        return static_cast<int>(size) + idx; // negative
                    };

                    int pidx = (vi != 0) ? fixIndex(vi, positions.size()) : -1;
                    int tidx = (ti != 0) ? fixIndex(ti, uvs.size()) : -1;
                    int nidx = (ni != 0) ? fixIndex(ni, normals.size()) : -1;

                    if (pidx < 0 || pidx >= static_cast<int>(positions.size()))
                    {
                        std::cerr << "AssetManager: bad position index in OBJ: " << entry << std::endl;
                        continue;
                    }

                    // Build vertex: pos, optional normal, optional uv -> we choose order pos, normal, uv for compatibility with StaticMesh expectation
                    unsigned int newIndex = static_cast<unsigned int>(outVertices.size() / 3); // temporary; will fix after push
                    // We'll push components directly and compute index as current vertex count
                    newIndex = static_cast<unsigned int>(outVertices.size());

                    // We'll create a key combining indices to avoid duplicates (use the original entry string)
                    unsigned int vertIndex = static_cast<unsigned int>(outVertices.size());

                    // Append position
                    outVertices.push_back(positions[pidx].x);
                    outVertices.push_back(positions[pidx].y);
                    outVertices.push_back(positions[pidx].z);

                    if (hasNormal && nidx >= 0)
                    {
                        outVertices.push_back(normals[nidx].x);
                        outVertices.push_back(normals[nidx].y);
                        outVertices.push_back(normals[nidx].z);
                    }
                    if (hasUV && tidx >= 0)
                    {
                        outVertices.push_back(uvs[tidx].x);
                        outVertices.push_back(uvs[tidx].y);
                    }

                    // compute vertex count: outVertices size / stride will be computed later; but index must be vertex index number
                    // Determine stride now
                    int stride = 3 + (hasNormal ? 3 : 0) + (hasUV ? 2 : 0);
                    unsigned int vertexCount = static_cast<unsigned int>(outVertices.size() / stride);
                    unsigned int assignedIndex = vertexCount - 1;

                    indexMap[entry] = assignedIndex;
                    outIndices.push_back(assignedIndex);
                }
            }
        }
    }

    // Now infer floats_per_vertex
    int floats_per_vertex = 3 + (hasNormal ? 3 : 0) + (hasUV ? 2 : 0);
    unsigned int vertex_count = static_cast<unsigned int>(outVertices.size() / floats_per_vertex);

    if (vertex_count == 0 || outIndices.empty())
    {
        std::cerr << "AssetManager: OBJ parsing produced no geometry: " << filePath << std::endl;
        return nullptr;
    }

    auto mesh = std::make_shared<StaticMesh>(outVertices.data(), vertex_count, outIndices.data(), outIndices.size(), floats_per_vertex);
    assets[name] = mesh;
    return mesh;
}
