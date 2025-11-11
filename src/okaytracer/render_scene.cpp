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
            break;
        case PrimitiveType::Quad:
            render_prim = create_quad_primitive(
                node,
                static_cast<const QuadPrimitive*>(node->primitive.get())
            );
            break;
        case PrimitiveType::Cuboid:
            render_prim = create_cuboid_primitive(
                node,
                static_cast<const CuboidPrimitive*>(node->primitive.get())
            );
            break;
        }

        render_prims.push_back(render_prim);
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
            break;
        default: break;
        }

        render_prims.push_back(render_prim);
    }

    // recurse children
    for (const auto& child : node->children) {
        add_node_recursive(child.get(), render_prims);
    }
}


// == create prims ==

RenderPrimitive RenderScene::create_sphere_primitive(const SceneNode* node, const SpherePrimitive* sphere)
{
    RenderPrimitive prim;
    prim.type = RenderPrimitive::Type::Sphere;

    // apply transform
    prim.centre = node->transform.position;
    prim.radius = sphere->radius * node->transform.scale.x; //TEMP uniform on x

    prim.material = node->material;

    return prim;
}

RenderPrimitive RenderScene::create_quad_primitive(const SceneNode* node, const QuadPrimitive* quad)
{
    RenderPrimitive prim;
    prim.type = RenderPrimitive::Type::Quad;

    // transform quad to world space
    Mat4 translate = Mat4::translate(node->transform.position.x, node->transform.position.y, node->transform.position.z);
    Mat4 scale = Mat4::scale (node->transform.scale.x, node->transform.scale.y, node->transform.scale.z);
    Mat4 model = translate * scale;

    Vec3 local_corner = (quad->u + quad->v) * -1.0f;
    prim.quad_corner = model.transform_point(local_corner);
    prim.quad_u = model.transform_direction(quad->u * 2.0f);
    prim.quad_v = model.transform_direction(quad->v * 2.0f);
    prim.quad_normal = Vec3::cross(prim.quad_u, prim.quad_v).normalised();

    prim.material = node->material;

    return prim;
}

RenderPrimitive RenderScene::create_cuboid_primitive(const SceneNode* node, const CuboidPrimitive* cuboid)
{
    RenderPrimitive prim;
    prim.type = RenderPrimitive::Type::Cuboid;

    // transform cuboid to world space AABB
    Vec3 half_extents = Vec3(
        cuboid->extents.x * node->transform.scale.x * 0.5f,
        cuboid->extents.y * node->transform.scale.y * 0.5f,
        cuboid->extents.z * node->transform.scale.z * 0.5f
    );

    prim.cuboid_min = node->transform.position - half_extents;
    prim.cuboid_max = node->transform.position + half_extents;

    prim.material = node->material;

    return prim;
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
