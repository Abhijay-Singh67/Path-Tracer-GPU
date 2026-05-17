#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <iostream>

class Mesh {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
    };

    // Each face stores three indices into the `vertices` array.
    struct Face {
        int v0, v1, v2;
    };

    std::vector<Vertex> vertices;
    std::vector<Face> faces;

    Mesh() = default;

    explicit Mesh(const std::string& path) {
        if (!loadOBJ(path)) {
            std::cerr << "Failed to load mesh: " << path << std::endl;
        }
    }

    bool loadOBJ(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Cannot open OBJ: " << path << std::endl;
            return false;
        }

        // Raw OBJ data — OBJ stores positions and normals in separate arrays,
        // and faces reference them with potentially-different indices.
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;

        // A face vertex in OBJ is a (pos_idx, normal_idx) tuple.
        // We de-duplicate these into unique Vertex entries.
        struct FaceVert {
            int pos_idx;   // 0-based, -1 if missing
            int norm_idx;  // 0-based, -1 if missing
            bool operator==(const FaceVert& o) const {
                return pos_idx == o.pos_idx && norm_idx == o.norm_idx;
            }
        };
        struct FaceVertHash {
            size_t operator()(const FaceVert& fv) const {
                // Simple hash combine
                return std::hash<int>()(fv.pos_idx) ^ (std::hash<int>()(fv.norm_idx) << 1);
            }
        };
        std::unordered_map<FaceVert, int, FaceVertHash> vertCache;

        auto getOrCreateVertex = [&](const FaceVert& fv) -> int {
            auto it = vertCache.find(fv);
            if (it != vertCache.end()) return it->second;
            Vertex v;
            v.position = positions[fv.pos_idx];
            v.normal   = (fv.norm_idx >= 0) ? normals[fv.norm_idx] : glm::vec3(0.0f);
            int idx = (int)vertices.size();
            vertices.push_back(v);
            vertCache[fv] = idx;
            return idx;
        };

        // Parses an OBJ face token like "12", "12//5", "12/3/5", or "12/3".
        // OBJ is 1-indexed; we convert to 0-indexed. Negative indices in OBJ
        // mean "count from the end" — handle that too.
        auto parseFaceToken = [&](const std::string& tok) -> FaceVert {
            FaceVert fv{-1, -1};
            int parsed[3] = {0, 0, 0};
            bool present[3] = {false, false, false};

            size_t start = 0;
            int field = 0;
            for (size_t i = 0; i <= tok.size() && field < 3; i++) {
                if (i == tok.size() || tok[i] == '/') {
                    if (i > start) {
                        parsed[field] = std::stoi(tok.substr(start, i - start));
                        present[field] = true;
                    }
                    start = i + 1;
                    field++;
                }
            }

            auto resolve = [&](int raw, size_t count) -> int {
                if (raw > 0) return raw - 1;            // 1-indexed → 0-indexed
                if (raw < 0) return (int)count + raw;   // negative: from end
                return -1;
            };

            if (present[0]) fv.pos_idx  = resolve(parsed[0], positions.size());
            if (present[2]) fv.norm_idx = resolve(parsed[2], normals.size());
            // field 1 (uv) ignored for now
            return fv;
        };

        std::string line;
        bool any_normals_in_file = false;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            std::string tag;
            iss >> tag;

            if (tag == "v") {
                glm::vec3 p;
                iss >> p.x >> p.y >> p.z;
                positions.push_back(p);
            }
            else if (tag == "vn") {
                glm::vec3 n;
                iss >> n.x >> n.y >> n.z;
                normals.push_back(n);
                any_normals_in_file = true;
            }
            else if (tag == "f") {
                // Read all face tokens (could be 3, 4, or more for n-gons).
                std::vector<std::string> tokens;
                std::string t;
                while (iss >> t) tokens.push_back(t);
                if (tokens.size() < 3) continue;

                // Convert each token to a FaceVert, then to a vertex index.
                std::vector<int> faceIndices;
                faceIndices.reserve(tokens.size());
                for (const auto& tk : tokens) {
                    FaceVert fv = parseFaceToken(tk);
                    if (fv.pos_idx < 0) continue;
                    faceIndices.push_back(getOrCreateVertex(fv));
                }
                if (faceIndices.size() < 3) continue;

                // Fan-triangulate: (v0, v1, v2), (v0, v2, v3), (v0, v3, v4), ...
                // Works for convex n-gons, which is what OBJ typically contains.
                for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
                    faces.push_back({faceIndices[0], faceIndices[(int)i], faceIndices[(int)i + 1]});
                }
            }
            // ignore everything else (vt, vp, g, o, s, mtllib, usemtl, ...)
        }

        // If the OBJ didn't include normals, compute them ourselves.
        if (!any_normals_in_file) {
            computeVertexNormals();
        } else {
            // Some files have normals but they may not be normalized — be safe.
            for (auto& v : vertices) {
                if (glm::length(v.normal) > 1e-8f)
                    v.normal = glm::normalize(v.normal);
            }
        }

        std::cout << "Loaded " << path
                  << ": " << vertices.size() << " vertices, "
                  << faces.size() << " faces"
                  << (any_normals_in_file ? " (normals from file)" : " (normals computed)")
                  << std::endl;
        return true;
    }

    // Area-weighted vertex normal computation.
    // Each face contributes its (un-normalized) cross product to its three vertices.
    // The cross product magnitude is 2× the triangle's area, so larger faces have
    // proportionally more influence — which is what you want.
    void computeVertexNormals() {
        for (auto& v : vertices) v.normal = glm::vec3(0.0f);

        for (const auto& f : faces) {
            glm::vec3& p0 = vertices[f.v0].position;
            glm::vec3& p1 = vertices[f.v1].position;
            glm::vec3& p2 = vertices[f.v2].position;
            glm::vec3 faceN = glm::cross(p1 - p0, p2 - p0);  // not normalized — area-weighted
            vertices[f.v0].normal += faceN;
            vertices[f.v1].normal += faceN;
            vertices[f.v2].normal += faceN;
        }

        for (auto& v : vertices) {
            if (glm::length(v.normal) > 1e-8f)
                v.normal = glm::normalize(v.normal);
            else
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);  // fallback for degenerate
        }
    }

    // Convenience helpers to push this mesh into your GPU SSBO arrays.
    // Pass in your scene's vertex and index buffers and the material id.
    template <typename GPUVertex, typename GPUIndex>
    void appendToScene(std::vector<GPUVertex>& gpuVertices,
                       std::vector<GPUIndex>& gpuIndices,
                       int material_id,
                       const glm::mat4& transform = glm::mat4(1.0f)) const
    {
        int baseIdx = (int)gpuVertices.size();

        // Transform vertices and append
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(transform)));
        for (const auto& v : vertices) {
            GPUVertex gv;
            glm::vec3 pos = glm::vec3(transform * glm::vec4(v.position, 1.0f));
            glm::vec3 nrm = glm::normalize(normalMatrix * v.normal);
            gv.position = glm::vec4(pos, 0.0f);
            gv.normal   = glm::vec4(nrm, 0.0f);
            gpuVertices.push_back(gv);
        }

        // Append faces with offset indices
        for (const auto& f : faces) {
            GPUIndex idx;
            idx.index = glm::ivec4(baseIdx + f.v0, baseIdx + f.v1, baseIdx + f.v2, material_id);
            gpuIndices.push_back(idx);
        }
    }
};

#endif