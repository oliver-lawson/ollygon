#include "serialisation.hpp"

namespace ollygon {

// == main methods ==

bool SceneSerialiser::save_scene(const Scene* scene, const QString& filepath)
{
    return false;
}

bool SceneSerialiser::load_scene(Scene* scene, const QString& filepath)
{
    return false;
}

QJsonObject SceneSerialiser::serialise_node(const SceneNode* node)
{
    return QJsonObject();
}

std::unique_ptr<SceneNode> SceneSerialiser::deserialise_node(const QJsonObject& obj)
{
    return std::unique_ptr<SceneNode>();
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
    obj["u"] = SceneSerialiser::vec3_to_json(cuboid->extents);
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
