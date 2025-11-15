#include "render_scene.hpp"
#include "core/mat4.hpp"

namespace ollygon {
namespace okaytracer {

RenderScene RenderScene::from_scene(const Scene* scene) {
    RenderScene render_scene;

    if (!scene) return render_scene;

    add_node_recursive(scene->get_root(), render_scene.primitives);

    return render_scene;
}

void RenderScene::add_node_recursive(const SceneNode* node, std::vector<RenderPrimitive>& render_prims) {
    
    if (!node || !node->visible) return; //invisible parents = invisible children

    // add primitives
    if (node->node_type == NodeType::Primitive && node->primitive) {
        RenderPrimitive render_prim;

        switch (node->primitive->get_type())
        {
        case PrimitiveType::Sphere:
            render_prim = create_sphere_primitive(
                node,
                static_cast<const SpherePrimitive*>(node->primitive.get())
            );
            render_prims.push_back(render_prim);
            break;
        case PrimitiveType::Quad:
            render_prim = create_quad_primitive(
                node,
                static_cast<const QuadPrimitive*>(node->primitive.get())
            );
            render_prims.push_back(render_prim);
            break;
        case PrimitiveType::Cuboid:
            add_cuboid_as_triangles(
                node,
                static_cast<const CuboidPrimitive*>(node->primitive.get()), render_prims
            );
            break;
        }

    }

    //add meshes
    if (node->node_type == NodeType::Mesh && node->geo) {
        add_mesh_primitives(node, node->geo.get(), render_prims);
    }

    // add lights with geo
    if (node->node_type == NodeType::Light && node->primitive) {
        RenderPrimitive render_prim;

        switch (node->primitive->get_type()) {
        case PrimitiveType::Quad:
            render_prim = create_quad_primitive(
                node,
                static_cast<const QuadPrimitive*>(node->primitive.get())
            );
            // override material with emissive
            if (node->light) {
                render_prim.material = Material::emissive(node->light->colour * node->light->intensity);
            }
            render_prims.push_back(render_prim);
            break;
        default: break;
        }
    }

    // recurse children
    for (const auto& child : node->children) {
        add_node_recursive(child.get(), render_prims);
    }
}

// == utils ==
// helper to build TRS matrix from node
static Mat4 get_transform_matrix(const SceneNode* node) {
    Mat4 translation = Mat4::translate(
        node->transform.position.x,
        node->transform.position.y,
        node->transform.position.z
    );

    const float deg_to_rad = 3.14195f / 180.0f;
    Mat4 rotation = Mat4::rotate_euler(
        node->transform.rotation.x * deg_to_rad,
        node->transform.rotation.y * deg_to_rad,
        node->transform.rotation.z * deg_to_rad
    );

    Mat4 scale = Mat4::scale(
        node->transform.scale.x,
        node->transform.scale.y,
        node->transform.scale.z
    );

    return translation * rotation * scale;
}

// == create prims ==

RenderPrimitive RenderScene::create_sphere_primitive(const SceneNode* node, const SpherePrimitive* sphere)
{
    RenderPrimitive prim;
    prim.type = RenderPrimitive::Type::Sphere;

    // apply transform
    prim.centre = node->transform.position;
    prim.radius = sphere->radius * node->transform.scale.x; //TEMP uniform on x
    // no rotation yet..until spheroids

    prim.material = node->material;

    return prim;
}

RenderPrimitive RenderScene::create_quad_primitive(const SceneNode* node, const QuadPrimitive* quad)
{
    RenderPrimitive prim;
    prim.type = RenderPrimitive::Type::Quad;

    Mat4 model = get_transform_matrix(node);

    Vec3 local_corner = (quad->u + quad->v) * -1.0f;
    prim.quad_corner = model.transform_point(local_corner);
    prim.quad_u = model.transform_direction(quad->u * 2.0f);
    prim.quad_v = model.transform_direction(quad->v * 2.0f);
    prim.quad_normal = Vec3::cross(prim.quad_u, prim.quad_v).normalised();

    prim.material = node->material;

    return prim;
}

void RenderScene::add_cuboid_as_triangles(const SceneNode* node, const CuboidPrimitive* cuboid, std::vector<RenderPrimitive>& prims)
{
    Mat4 model = get_transform_matrix(node);

    Vec3 h = cuboid->extents / 2.0f;

    //8 corners in local space
    Vec3 corners[8] = {
        Vec3(-h.x, -h.y, -h.z),
        Vec3(h.x, -h.y, -h.z),
        Vec3(h.x,  h.y, -h.z),
        Vec3(-h.x,  h.y, -h.z),
        Vec3(-h.x, -h.y,  h.z),
        Vec3(h.x, -h.y,  h.z),
        Vec3(h.x,  h.y,  h.z),
        Vec3(-h.x,  h.y,  h.z)
    };

    // transform corners to world space
    Vec3 world_corners[8];
    for (int i = 0; i < 8; ++i) {
        world_corners[i] = model.transform_point(corners[i]);
    }

    //faces
    struct Face {
        int indices[4];
        Vec3 normal;
    };
    Face faces[6] = {
        {{3, 2, 1, 0}, Vec3(0, 0, -1)},  // back
        {{6, 7, 4, 5}, Vec3(0, 0,  1)},  // front
        {{7, 3, 0, 4}, Vec3(-1, 0, 0)},  // left
        {{2, 6, 5, 1}, Vec3(1, 0,  0)},  // right
        {{0, 1, 5, 4}, Vec3(0, -1, 0)},  // bottom
        {{7, 6, 2, 3}, Vec3(0,  1, 0)}   // top
    };

    // create 2 tris per face
    for (int f = 0; f < 6; ++f) {
        // transform the normal
        Vec3 world_normal = model.transform_direction(faces[f].normal).normalised();

        // first tri (0, 1, 2)
        RenderPrimitive tri1;
        tri1.type = RenderPrimitive::Type::Triangle;
        tri1.tri_v0 = world_corners[faces[f].indices[0]];
        tri1.tri_v1 = world_corners[faces[f].indices[1]];
        tri1.tri_v2 = world_corners[faces[f].indices[2]];
        tri1.tri_n0 = world_normal;
        tri1.tri_n1 = world_normal;
        tri1.tri_n2 = world_normal;
        tri1.material = node->material;
        prims.push_back(tri1);

        // second (0, 2, 3)
        RenderPrimitive tri2;
        tri2.type = RenderPrimitive::Type::Triangle;
        tri2.tri_v0 = world_corners[faces[f].indices[0]];
        tri2.tri_v1 = world_corners[faces[f].indices[2]];
        tri2.tri_v2 = world_corners[faces[f].indices[3]];
        tri2.tri_n0 = world_normal;
        tri2.tri_n1 = world_normal;
        tri2.tri_n2 = world_normal;
        tri2.material = node->material;
        prims.push_back(tri2);
    }
}

// == create mesh ==

void RenderScene::add_mesh_primitives( const SceneNode* node, const Geo* geo, std::vector<RenderPrimitive>& prims )
{
    if (geo->indices.empty() || geo->verts.empty()) return;

    Mat4 translate = Mat4::translate(
        node->transform.position.x,
        node->transform.position.y,
        node->transform.position.z
    );
    Mat4 scale = Mat4::scale(
        node->transform.scale.x,
        node->transform.scale.y,
        node->transform.scale.z
    );
    Mat4 model = translate * scale;

    // convert each tri to world space
    for (size_t i = 0; i < geo->indices.size(); i += 3) {
        uint32_t i0 = geo->indices[i];
        uint32_t i1 = geo->indices[i + 1];
        uint32_t i2 = geo->indices[i + 2];

        const Vertex& v0 = geo->verts[i0];
        const Vertex& v1 = geo->verts[i1];
        const Vertex& v2 = geo->verts[i2];

        RenderPrimitive prim;
        prim.type = RenderPrimitive::Type::Triangle;

        // transform verts to world space
        prim.tri_v0 = model.transform_point(v0.position);
        prim.tri_v1 = model.transform_point(v1.position);
        prim.tri_v2 = model.transform_point(v2.position);

        // transform normals..use normal matrix
        prim.tri_n0 = model.transform_direction(v0.normal).normalised();
        prim.tri_n1 = model.transform_direction(v1.normal).normalised();
        prim.tri_n2 = model.transform_direction(v2.normal).normalised();

        prim.material = node->material;

        prims.push_back(prim);
    }
}

} // namespace okaytracer
} // namespace ollygon
