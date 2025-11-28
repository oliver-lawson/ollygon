#pragma once

#include "core/geometry.hpp"
#include <string>
#include <memory>

namespace ollygon {

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
    static bool parse_obj_line(const std::string& line, Geo& geo);
};

} // namespace ollygon
