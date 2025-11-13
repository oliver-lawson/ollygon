#include "serialisation.hpp"
#include <QFile>

namespace ollygon {

// == main methods ==

bool SceneSerialiser::save_scene(const Scene* scene, const Camera* viewport_camera, const QString& filepath)
{
    if (!scene) return false;

    QJsonObject root_obj;
    root_obj["version"] = 1;

    root_obj["viewport_camera"] = serialise_camera(viewport_camera);

    root_obj["scene"] = serialise_node(scene->get_root());

    QJsonDocument doc(root_obj);

    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing: " << filepath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Scene saved to " << filepath;

    return true;
}

bool SceneSerialiser::load_scene(Scene* scene, Camera* viewport_camera, const QString& filepath)
{
    if (!scene) return false;

    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for reading: " << filepath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Invalid JSON in file: " << filepath;
    }

    QJsonObject root_obj = doc.object();
    int version = root_obj["version"].toInt();

    if (version != 1) {
        // TEMP
        qWarning() << "Unsupported scene version: " << version;
        return false;
    }

    //deserialise viewport camera, scene
    if (viewport_camera && root_obj.contains("viewport_camera")) {
        deserialise_camera(viewport_camera, root_obj["viewport_camera"].toObject());
    }

    QJsonObject scene_obj = root_obj["scene"].toObject();
    auto new_root = deserialise_node(scene_obj);

    //replace scene root
    scene->get_root()->children.clear();
    for (auto& child : new_root->children) {
        scene->get_root()->add_child(std::move(child));
    }

    qDebug() << "Scene loaded from " << filepath;
    return true;
}

QJsonObject SceneSerialiser::serialise_material(const Material& mat)
{
    QJsonObject obj;

    QString type_str;
    switch (mat.type) {
    case MaterialType::Lambertian: type_str = "lambertian"; break;
    case MaterialType::Metal: type_str = "metal"; break;
    case MaterialType::Dielectric: type_str = "dielectric"; break;
    case MaterialType::Emissive: type_str = "emissive"; break;
    case MaterialType::Chequerboard: type_str = "chequerboard"; break;
    default: type_str = "lambertian"; break;
    }
    obj["type"] = type_str;
    obj["albedo"] = colour_to_json(mat.albedo);
    obj["emission"] = colour_to_json(mat.emission);
    obj["roughness"] = mat.roughness;
    obj["ior"] = mat.ior;
    obj["chequerboard_colour_a"] = colour_to_json(mat.chequerboard_colour_a);
    obj["chequerboard_colour_b"] = colour_to_json(mat.chequerboard_colour_b);
    obj["chequerboard_scale"] = mat.chequerboard_scale;
    obj["metallic"] = mat.metallic;
    obj["specular"] = mat.specular;

    return obj;
}

Material SceneSerialiser::deserialise_material(const QJsonObject& obj)
{
    Material mat;

    QString type_str = obj["type"].toString();
    if (type_str == "lambertian") mat.type = MaterialType::Lambertian;
    else if (type_str == "metal") mat.type = MaterialType::Metal;
    else if (type_str == "dielectric") mat.type = MaterialType::Dielectric;
    else if (type_str == "emissive") mat.type = MaterialType::Emissive;
    else if (type_str == "chequerboard") mat.type = MaterialType::Chequerboard;

    mat.albedo = json_to_colour(obj["albedo"].toArray());
    mat.emission = json_to_colour(obj["emission"].toArray());
    mat.roughness = obj["roughness"].toDouble(0.0);
    mat.ior = obj["ior"].toDouble(1.5);
    mat.chequerboard_colour_a = json_to_colour(obj["chequerboard_colour_a"].toArray());
    mat.chequerboard_colour_b = json_to_colour(obj["chequerboard_colour_b"].toArray());
    mat.chequerboard_scale = obj["chequerboard_scale"].toDouble(1.0);
    mat.metallic = obj["metallic"].toDouble(0.0);
    mat.specular = obj["specular"].toDouble(0.0);

    return mat;
}

QJsonObject SceneSerialiser::serialise_node(const SceneNode* node)
{
    QJsonObject obj;

    obj["name"] = QString::fromStdString(node->name);
    obj["visible"] = node->visible;
    obj["locked"] = node->locked;

    //transform
    QJsonObject transform_obj;
    transform_obj["position"] = vec3_to_json(node->transform.position);
    transform_obj["rotation"] = vec3_to_json(node->transform.rotation);
    transform_obj["scale"] = vec3_to_json(node->transform.scale);
    obj["transform"] = transform_obj;

    //node type
    QString node_type;
    switch (node->node_type) {
        case NodeType::Empty: node_type = "empty"; break;
        case NodeType::Mesh: node_type = "mesh"; break;
        case NodeType::Primitive: node_type = "primitive"; break;
        case NodeType::Light: node_type = "light"; break;
        case NodeType::Camera: node_type = "camera"; break;
        default: node_type = "error"; break;
    }
    obj["node_type"] = node_type;

    //TEMP albedo (per-geo baked mat placeholder)
    //TODO remove
    obj["albedo"] = colour_to_json(node->albedo);

    obj["material"] = serialise_material(node->material);

    //geometry/primitive
    if (node->primitive) {
        switch (node->primitive->get_type()) {
            case PrimitiveType::Sphere:
                obj["primitive"] = serialise_sphere(static_cast<const SpherePrimitive*>(node->primitive.get()));
                break;
            case PrimitiveType::Quad:
                obj["primitive"] = serialise_quad(static_cast<const QuadPrimitive*>(node->primitive.get()));
                break;
            case PrimitiveType::Cuboid:
                obj["primitive"] = serialise_cuboid(static_cast<const CuboidPrimitive*>(node->primitive.get()));
                break;
            default: break;
        }
    }

    // mesh/lights/etc
    if (node->geo) obj["geo"] = serialise_geo(node->geo.get());
    if (node->light) obj["light"] = serialise_light(node->light.get());
    
    //children
    QJsonArray children_array;
    for (const auto& child : node->children) {
        children_array.append(serialise_node(child.get()));
    }
    obj["children"] = children_array;

    return obj;
}

std::unique_ptr<SceneNode> SceneSerialiser::deserialise_node(const QJsonObject& obj) {

    auto node = std::make_unique<SceneNode>();

    node->name = obj["name"].toString().toStdString();
    node->visible = obj["visible"].toBool(true);
    node->locked = obj["locked"].toBool(false);

    //transform
    QJsonObject transform_obj = obj["transform"].toObject();
    node->transform.position = json_to_vec3(transform_obj["position"].toArray());
    node->transform.rotation = json_to_vec3(transform_obj["rotation"].toArray());
    node->transform.scale = json_to_vec3(transform_obj["scale"].toArray());

    //node type
    QString node_type = obj["node_type"].toString();
    if (node_type == "empty") node->node_type = NodeType::Empty;
    else if (node_type == "mesh") node->node_type = NodeType::Mesh;
    else if (node_type == "primitive") node->node_type = NodeType::Primitive;
    else if (node_type == "light") node->node_type = NodeType::Light;
    else if (node_type == "camera") node->node_type = NodeType::Camera;

    //albedo
    node->albedo = json_to_colour(obj["albedo"].toArray()); //TEMP TODO:remove

    node->material = deserialise_material(obj["material"].toObject());

    //primitives
    if (obj.contains("primitive")) {
        QJsonObject prim_obj = obj["primitive"].toObject();
        QString prim_type = prim_obj["type"].toString();

        if (prim_type == "sphere") {
            node->primitive = deserialise_sphere(prim_obj);
        }
        else if (prim_type == "quad") {
            node->primitive = deserialise_quad(prim_obj);
        }
        else if (prim_type == "cuboid") {
            node->primitive = deserialise_cuboid(prim_obj);
        }
    }

    // geo
    if (obj.contains("geo")) node->geo = deserialise_geo(obj["geo"].toObject());
    // light
    if (obj.contains("light")) node->light = deserialise_light(obj["light"].toObject());

    //children
    QJsonArray children_array = obj["children"].toArray();
    for (const auto& child_val : children_array) {
        auto child = deserialise_node(child_val.toObject());
        node->add_child(std::move(child));
    }

    return node;
}

// == objects ==

QJsonObject SceneSerialiser::serialise_sphere(const SpherePrimitive* sphere) {
    QJsonObject obj;
    obj["type"] = "sphere";
    obj["radius"] = sphere->radius;
    return obj;
}
std::unique_ptr<SpherePrimitive> SceneSerialiser::deserialise_sphere(const QJsonObject& obj) {
    float radius = obj["radius"].toDouble(1.0f);
    return std::make_unique<SpherePrimitive>(radius);
}

QJsonObject SceneSerialiser::serialise_quad(const QuadPrimitive* quad) {
    QJsonObject obj;
    obj["type"] = "quad";
    obj["u"] = SceneSerialiser::vec3_to_json(quad->u);
    obj["v"] = SceneSerialiser::vec3_to_json(quad->v);
    return obj;
}
std::unique_ptr<QuadPrimitive> SceneSerialiser::deserialise_quad(const QJsonObject& obj) {
    Vec3 u = json_to_vec3(obj["u"].toArray());
    Vec3 v = json_to_vec3(obj["v"].toArray());
    return std::make_unique<QuadPrimitive>(u, v);
}

QJsonObject SceneSerialiser::serialise_cuboid(const CuboidPrimitive* cuboid) {
    QJsonObject obj;
    obj["type"] = "cuboid";
    obj["extents"] = SceneSerialiser::vec3_to_json(cuboid->extents);
    return obj;
}
std::unique_ptr<CuboidPrimitive> SceneSerialiser::deserialise_cuboid(const QJsonObject& obj) {
    Vec3 extents = SceneSerialiser::json_to_vec3(obj["extents"].toArray());
    return std::make_unique<CuboidPrimitive>(extents);
}

// == meshes ==

QJsonObject SceneSerialiser::serialise_geo(const Geo* geo) {
    QJsonObject obj;
    obj["type"] = "mesh";

    QJsonArray verts_array;
    for (const auto& v : geo->verts) {
        QJsonObject vert_obj;
        vert_obj["pos"] = vec3_to_json(v.position);
        vert_obj["norm"] = vec3_to_json(v.normal);
        verts_array.append(vert_obj);
    }
    obj["verts"] = verts_array;

    QJsonArray indices_array;
    for (uint32_t idx : geo->indices) {
        indices_array.append(static_cast<int>(idx));
    }
    obj["indices"] = indices_array;

    if (!geo->source_file.empty()) {
        obj["source_file"] = QString::fromStdString(geo->source_file);
    }

    return obj;
}

std::unique_ptr<Geo> SceneSerialiser::deserialise_geo(const QJsonObject& obj)
{
    auto geo = std::make_unique<Geo>();

    QJsonArray verts_array = obj["verts"].toArray();
    for (const auto& vert_val : verts_array) {
        QJsonObject vert_obj = vert_val.toObject();
        Vec3 pos = json_to_vec3(vert_obj["pos"].toArray());
        Vec3 norm = json_to_vec3(vert_obj["norm"].toArray());
        geo->add_vertex(pos, norm);
    }

    QJsonArray indices_array = obj["indices"].toArray();
    for (const auto& ind_val : indices_array) {
        geo->indices.push_back(ind_val.toInt());
    }

    if (obj.contains("source_file")) {
        geo->source_file = obj["source_file"].toString().toStdString();
    }

    return geo;
}

QJsonObject SceneSerialiser::serialise_light(const Light* light)
{
    QJsonObject obj;

    QString type_str;
    switch (light->type) {
        case LightType::Point: type_str = "point"; break;
        case LightType::Directional: type_str = "directional"; break;
        case LightType::Area: type_str = "area"; break;
        default: type_str = "point"; break;
    }
    obj["type"] = type_str;

    obj["colour"] = SceneSerialiser::colour_to_json(light->colour);
    obj["intensity"] = light->intensity;
    obj["is_area_light"] = light->is_area_light;

    return obj;
}

std::unique_ptr<Light> SceneSerialiser::deserialise_light(const QJsonObject& obj)
{
    auto light = std::make_unique<Light>();

    QString type_str = obj["type"].toString();
    if (type_str == "point") light->type = LightType::Point;
    else if (type_str == "directional") light->type = LightType::Directional;
    else if (type_str == "area") light->type = LightType::Area;

    light->colour = SceneSerialiser::json_to_colour(obj["colour"].toArray());
    light->intensity = obj["intensity"].toDouble(1.0);
    light->is_area_light = obj["is_area_light"].toBool(false);

    return light;
}

QJsonObject SceneSerialiser::serialise_camera(const Camera* camera)
{
    QJsonObject obj;

    const CameraController* controller = camera->get_controller();

    obj["mode"] = (controller->get_mode() == CameraMode::Orbit) ? "orbit" : "ue";
    obj["target"] = vec3_to_json(controller->get_target());
    obj["distance"] = controller->get_distance();
    obj["yaw"] = controller->get_yaw();
    obj["pitch"] = controller->get_pitch();
    obj["position"] = vec3_to_json(controller->get_position());

    return obj;
}

void SceneSerialiser::deserialise_camera(Camera* camera, const QJsonObject& obj)
{
    CameraController* controller = camera->get_controller();

    QString mode_str = obj["mode"].toString("orbit");
    CameraMode mode = (mode_str == "orbit") ? CameraMode::Orbit : CameraMode::UE;
    controller->set_mode(mode);

    Vec3 target = json_to_vec3(obj["target"].toArray());
    controller->set_target(target);

    float distance = obj["distance"].toDouble(8.0);
    float yaw = obj["yaw"].toDouble(0.0);
    float pitch = obj["pitch"].toDouble(0.0);

    controller->set_orbit_params(yaw, pitch, distance);
}



// == helpers ==

QJsonArray SceneSerialiser::vec3_to_json(const Vec3& v)
{
    return QJsonArray{ v.x, v.y, v.z };
}

Vec3 SceneSerialiser::json_to_vec3(const QJsonArray& arr)
{
    if (arr.size() != 3) return Vec3();

    return Vec3(arr[0].toDouble(), arr[1].toDouble(), arr[2].toDouble());
}

QJsonArray SceneSerialiser::colour_to_json(const Colour& c)
{
    return QJsonArray{c.r, c.g, c.b};
}

Colour SceneSerialiser::json_to_colour(const QJsonArray& arr)
{
    if (arr.size() != 3) return Colour(1.0f, 0.0f, 0.0f); //TODO make colour presets like ERROR red

    return Colour(arr[0].toDouble(), arr[1].toDouble(), arr[2].toDouble());
}

} // namespace ollygon
