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
    //TODO
    return false;
}

// == ply ==
MeshImportResult MeshImporter::import_ply(const std::string& filepath, Geo& out_geo)
{
    //TODO
    return MeshImportResult();
}

} // namespace ollygon
