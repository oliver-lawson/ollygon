#pragma once

#include "scene.hpp"
#include <memory>

namespace ollygon {

// scene manipulation operations and logic
// this should keep scene & main window clean going forward
class SceneOperations {
public:
    //delete returns true if success, false if node is root OR not found
    static bool delete_node(Scene* scene, SceneNode* node);

    // create stuff
    static std::unique_ptr<SceneNode> create_sphere(const std::string& name = "Sphere");
    static std::unique_ptr<SceneNode> create_cuboid(const std::string& name = "Cuboid");
    static std::unique_ptr<SceneNode> create_quad(const std::string& name = "Quad");

    static std::unique_ptr<SceneNode> create_empty(const std::string& name = "Empty");
    
    static std::unique_ptr<SceneNode> create_mesh(const std::string& name = "Mesh");

    static std::unique_ptr<SceneNode> create_point_light(const std::string& name = "Point Light");
    static std::unique_ptr<SceneNode> create_area_light(const std::string& name = "Area Light");

    //io
    static std::unique_ptr<SceneNode> import_mesh_from_file(const std::string& filepath);

private:
    //helpers
    static bool remove_node_recursive(SceneNode* parent, SceneNode* target);
    
};


} // namespace ollygon
