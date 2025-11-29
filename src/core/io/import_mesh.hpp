#pragma once

#include "core/geometry.hpp"
#include <string>
#include <memory>

namespace ollygon {

// helper to hash a vertex/normal index pair for deduplication
struct VertexKey {
    int pos_idx;
    int norm_idx;

    bool operator==(const VertexKey& other) const {
        return pos_idx == other.pos_idx && norm_idx == other.norm_idx;
    }
};

struct VertexKeyHash {
    size_t operator()(const VertexKey& k) const {
        return std::hash<int>()(k.pos_idx) ^ (std::hash<int>()(k.norm_idx) << 16);
    }
};

enum  class MeshImportResult {
    Success,
    FileNotFound,
    UnsupportedFormat,
    ParseError,
};

class MeshImporter {
public:
    static MeshImportResult import_obj(const std::string& filepath, Geo& out_geo);
    static MeshImportResult import_ply(const std::string& filepath, Geo& out_geo);

private:
    // computes smooth vertex normals from face geometry
    // (used when OBJ has no vn data)
    static void compute_face_normals(Geo& geo);
};

} // namespace ollygon
