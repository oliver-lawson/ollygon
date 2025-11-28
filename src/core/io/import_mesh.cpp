#include "import_mesh.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

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

    std::string line;
    int line_num = 0;
    int vertex_count = 0;
    int normal_count = 0;
    int face_count = 0;

    while (std::getline(file, line)) {
        if (!parse_obj_line(line, out_geo)) {
            std::cerr << "Parse error at line " << line_num << ": " << line << std::endl;
            return MeshImportResult::ParseError;
        }
        line_num++;
    }
    std::cout << "OBJ import complete:" << std::endl;

    out_geo.source_file = filepath;

    return MeshImportResult::Success;
}

bool MeshImporter::parse_obj_line(const std::string& line, Geo& geo)
{
    // skip empty lines & comments
    if (line.empty() || line[0] == '#') { return true; }

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
            std::cerr << "Invalid vertex format" << std::endl;
            return false;
        }

        // store vertex with placeholder normal for now
        // we'll fix normals later if the file has vn lines
        geo.add_vertex(Vec3(x, y, z), Vec3(0, 0, 1));

        return true;
    }
    if (prefix == "vn") {
        // vertex normal: vn x y z
        return true; //TODO
    }
    if (prefix == "vt") {
        // texture coordinate: vt u v [w]
        return true; //TODO
    }
    if (prefix == "f") { // face
        // f 1 2 3              (just vertex indices)
        // f 1/1 2/2 3/3        (vertex/texture indices)
        // f 1/1/1 2/2/2 3/3/3  (vertex/texture/normal indices)
        // f 1//1 2//2 3//3     (vertex//normal indices, no texture)

        //blender obj default seems to be v/vt/vn

        std::vector<int> face_verts;
        std::string vertex_data;

        while (ss >> vertex_data) {
            // parse "v" or "v/vt" or "v/vt/vn" or "v//vn"
            int v_idx = 0;
            int vt_idx = 0;
            int vn_idx = 0;

            size_t first_slash = vertex_data.find('/'); //any slash?
            if (first_slash == std::string::npos) { //non-position, not found
                //first simplest forma, just f 1 2 3 indices
                v_idx = std::stoi(vertex_data);
            }
            else {
                // slashes
                v_idx = std::stoi(vertex_data.substr(0, first_slash));

                size_t second_slash = vertex_data.find('/', first_slash + 1);

                if (second_slash == std::string::npos) {
                    // format: v/vt
                    if (first_slash + 1 < vertex_data.length()) {
                        vt_idx = std::stoi(vertex_data.substr(first_slash + 1));
                    }
                }
                else {
                    // format: v/vt/vn or v//vn
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

            if (v_idx > 0) {
                v_idx -= 1; //obj indices are 1-indexed! convert
            }
            else if (v_idx < 0) {
                //wrap negative indices
                v_idx = geo.verts.size() + v_idx;
            }

            //validate index
            if (v_idx < 0 || v_idx >= (int)geo.verts.size()) {
                //shouldn't be here!
                std::cerr << "Obj import: Vertex index out of range!: " << v_idx << std::endl;
                return false;
            }

            face_verts.push_back(v_idx);
            //TODO: handle vt_idxx and vn_idx similary once supported
        } // END while (ss >> vertex_data){}

        // OBJ faces have 3+ sides - tris,quads, n-gons too.
        //triangulate if needed for now
        //TODO: revisit once n-gon/quad first-class support
        if (face_verts.size() < 3) {
            std::cerr << "OBJ import: Face has few than 3 verts!" << std::endl;
            return false;
        }

        if (face_verts.size() == 3) {
            geo.add_tri(face_verts[0], face_verts[1], face_verts[2]);
        }
        else {
            //triangulate from first vertex
            //TODO add proper CDT or so once we have it,
            // as this will break on concavepolys
            for (size_t i = 1; i < face_verts.size() - 1; i++) {
                geo.add_tri(face_verts[0], face_verts[i], face_verts[i+1]);
            }
        }

        return true;
    }
   
    if (prefix == "o" || prefix == "g") {
        // object/group name: o name
        // flattening everything into one mesh for now, so skip
        //TODO -pass through obj not just geo
        return true;
    }

    if (prefix == "s") {
        // smoothing group: s off | s 1 | s 2 ...
        //TODO
        return true;
    }

    if (prefix == "mtllib" || prefix == "usemtl") {
        // material library / use material
        //TODO
        return true;
    }

    // unknown line type - log it and don't fail
    // probably lots more to add. until I exhaustively list them,
    // i'm presuming the rest will work so lets pass true for now
    std::cerr << "Skipping unknown OBJ line type: " << prefix << std::endl;
    return true;
}

// == ply ==
MeshImportResult MeshImporter::import_ply(const std::string& filepath, Geo& out_geo)
{
    //TODO
    return MeshImportResult();
}

} // namespace ollygon
