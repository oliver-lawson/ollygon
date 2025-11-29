#include "import_mesh.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

namespace ollygon {


// == obj ==
MeshImportResult MeshImporter::import_obj(const std::string& filepath, Geo& out_geo)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filepath << std::endl;
        return MeshImportResult::FileNotFound;
    }

    std::cout << "Importing OBJ file: <" << filepath << std::endl;

    // temporary storage - OBJ separates positions and normals
    std::vector<Vec3> temp_positions;
    std::vector<Vec3> temp_normals;

    // map from (pos_idx, norm_idx) -> final vertex index in geo
    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertex_map;

    std::string line;
    int line_num = 0;

    while (std::getline(file, line)) {
        line_num++;

        // skip empty lines & comments
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            //vertex position: v x y z [w]
            //eg: v 0.000000 2.000000 2.000000
            //with optional [w] that defaults to 1.0
            float x, y, z;
            ss >> x >> y >> z;
            if (ss.fail()) {
                std::cerr << "Invalid vertex format at line " << line_num << std::endl;
                return MeshImportResult::ParseError;
            }
            temp_positions.emplace_back(x, y, z);
        }
        else if (prefix == "vn") {
            // vertex normal: vn x y z
            float x, y, z;
            ss >> x >> y >> z;
            if (ss.fail()) {
                std::cerr << "Invalid normal format at line " << line_num << std::endl;
                return MeshImportResult::ParseError;
            }
            temp_normals.emplace_back(x, y, z);
        }
        else if (prefix == "vt") {
            // texture coordinate: vt u v [w]
            // texture coordinate - skip for now
            continue;
        }
        else if (prefix == "f") {
            // face
            // f 1 2 3              (just vertex indices)
            // f 1/1 2/2 3/3        (vertex/texture indices)
            // f 1/1/1 2/2/2 3/3/3  (vertex/texture/normal indices)
            // f 1//1 2//2 3//3     (vertex//normal indices, no texture)
            //
            //blender obj default seems to be v/vt/vn
            std::vector<VertexKey> face_keys;
            std::string vertex_data;

            while (ss >> vertex_data) {
                int v_idx = 0, vt_idx = 0, vn_idx = 0;

                size_t first_slash = vertex_data.find('/');
                if (first_slash == std::string::npos) {
                    v_idx = std::stoi(vertex_data);
                }
                else {
                    v_idx = std::stoi(vertex_data.substr(0, first_slash));
                    size_t second_slash = vertex_data.find('/', first_slash + 1);

                    if (second_slash == std::string::npos) {
                        // v/vt
                        if (first_slash + 1 < vertex_data.length()) {
                            vt_idx = std::stoi(vertex_data.substr(first_slash + 1));
                        }
                    }
                    else {
                        // v/vt/vn or v//vn
                        // 
                        // check if texture coordinate between the slashes (v/vt/vn)
                        // if slashes are adjacent (v//vn), this condition is false:
                        if (second_slash > first_slash + 1) {
                            vt_idx = std::stoi(vertex_data.substr(first_slash + 1, second_slash - first_slash - 1));
                        }
                        // parse normal index if present after the second slash
                        if (second_slash + 1 < vertex_data.length()) {
                            vn_idx = std::stoi(vertex_data.substr(second_slash + 1));
                        }
                    }
                }

                // convert to 0-indexed (OBJ is 1-indexed)
                // also handle negative indices (relative to end)
                if (v_idx > 0) v_idx -= 1;
                else if (v_idx < 0) v_idx = temp_positions.size() + v_idx;

                if (vn_idx > 0) vn_idx -= 1;
                else if (vn_idx < 0) vn_idx = temp_normals.size() + vn_idx;
                else vn_idx = -1;  // no normal specified

                // validate
                if (v_idx < 0 || v_idx >= (int)temp_positions.size()) {
                    std::cerr << "Vertex index out of range at line " << line_num << std::endl;
                    return MeshImportResult::ParseError;
                }

                face_keys.push_back({ v_idx, vn_idx });
            }

            if (face_keys.size() < 3) {
                std::cerr << "Face has fewer than 3 vertices at line " << line_num << std::endl;
                return MeshImportResult::ParseError;
            }

            // OBJ faces have 3+ sides - tris, quads, n-gons too.
            // convert keys to actual vertex indices, creating new verts as needed
            std::vector<uint32_t> face_indices;
            for (const auto& key : face_keys) {
                auto it = vertex_map.find(key);
                if (it != vertex_map.end()) {
                    face_indices.push_back(it->second);
                }
                else {
                    // create new vertex
                    Vec3 pos = temp_positions[key.pos_idx];
                    Vec3 norm(0, 1, 0);  // default up if no normal

                    if (key.norm_idx >= 0 && key.norm_idx < (int)temp_normals.size()) {
                        norm = temp_normals[key.norm_idx];
                    }

                    uint32_t new_idx = out_geo.vertex_count();
                    out_geo.add_vertex(pos, norm);
                    vertex_map[key] = new_idx;
                    face_indices.push_back(new_idx);
                }
            }

            // triangulate (fan from first vertex)
            //TODO: revisit once n-gon/quad first-class support
            //TODO: add proper CDT or so once we have it,
            // as this will break on concave polys
            for (size_t i = 1; i < face_indices.size() - 1; i++) {
                out_geo.add_tri(face_indices[0], face_indices[i], face_indices[i + 1]);
            }
        }
        else if (prefix == "o" || prefix == "g" || prefix == "s" ||
            prefix == "mtllib" || prefix == "usemtl") {
            // object/group name: o name | g name
            // smoothing group: s off | s 1 | s 2 ...
            // material library / use material
            // flattening everything into one mesh for now, so skip
            //TODO: pass through obj not just geo
            //TODO: material support
            continue;
        }
        else {
            // unknown line type - log it and don't fail
            // probably lots more to add. until I exhaustively list them,
            // i'm presuming the rest will work so lets pass true for now
            std::cerr << "Skipping unknown OBJ line type: " << prefix << std::endl;
        }
    }

    // if no normals were in the file, compute them from faces
    if (temp_normals.empty() && out_geo.tri_count() > 0) {
        std::cout << "No normals in OBJ, computing face normals..." << std::endl;
        compute_face_normals(out_geo);
    }

    out_geo.source_file = filepath;

    std::cout << "OBJ import complete: " << out_geo.vertex_count() << " vertices, "
        << out_geo.tri_count() << " triangles" << std::endl;

    return MeshImportResult::Success;
}

// == ply ==
MeshImportResult MeshImporter::import_ply(const std::string& filepath, Geo& out_geo)
{
    //TODO
    return MeshImportResult::UnsupportedFormat;
}

void MeshImporter::compute_face_normals(Geo& geo)
{
    // reset all normals to zero
    for (auto& v : geo.verts) {
        v.normal = Vec3(0, 0, 0);
    }

    // accumulate face normals to vertices
    for (size_t i = 0; i < geo.indices.size(); i += 3) {
        uint32_t i0 = geo.indices[i];
        uint32_t i1 = geo.indices[i + 1];
        uint32_t i2 = geo.indices[i + 2];

        Vec3 v0 = geo.verts[i0].position;
        Vec3 v1 = geo.verts[i1].position;
        Vec3 v2 = geo.verts[i2].position;

        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;
        Vec3 face_normal = Vec3::cross(edge1, edge2);
        //don't normalise yet - larger faces contribute more (area weighting)

        geo.verts[i0].normal = geo.verts[i0].normal + face_normal;
        geo.verts[i1].normal = geo.verts[i1].normal + face_normal;
        geo.verts[i2].normal = geo.verts[i2].normal + face_normal;
    }

    // normalise all vertex normals
    for (auto& v : geo.verts) {
        float len = v.normal.length();
        if (len > 1e-6f) {
            v.normal = v.normal / len;
        }
        else {
            v.normal = Vec3(0, 1, 0);  // fallback
        }
    }
}

} // namespace ollygon