#pragma once

#include "scene.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

namespace ollygon {

// serialisation interface for scene objects
class Serialisable {
public:
    virtual ~Serialisable() = default;
    virtual void to_json(QJsonObject& obj) const = 0;
    virtual void from_json(const QJsonObject& obj) = 0;
};

//scene serialisation ops
class SceneSerialiser {
public:
    static bool save_scene(const Scene* scene, const QString& filepath);
    static bool load_scene(Scene* scene, const QString& filepath);

private:
    static QJsonObject serialise_node(const SceneNode* node);
    static std::unique_ptr<SceneNode> deserialise_node(const QJsonObject& obj);

    static QJsonObject serialise_sphere(const SpherePrimitive* sphere);
    static std::unique_ptr<SpherePrimitive> deserialise_sphere(const QJsonObject& obj);
    static QJsonObject serialise_quad(const QuadPrimitive* quad);
    static std::unique_ptr<QuadPrimitive> deserialise_quad(const QJsonObject& obj);
    static QJsonObject serialise_cuboid(const CuboidPrimitive* quad);
    static std::unique_ptr<CuboidPrimitive> deserialise_cuboid(const QJsonObject& obj);

    static QJsonObject serialise_geo(const Geo* geo);
    static std::unique_ptr<Geo> deserialise_geo(const QJsonObject& obj);

    static QJsonObject serialise_light(const Light* light);
    static std::unique_ptr<Light> deserialise_light(const QJsonObject& obj);

    static QJsonArray vec3_to_json(const Vec3& v);
    static Vec3 json_to_vec3(const QJsonArray& arr);

    static QJsonArray colour_to_json(const Colour& c);
    static Colour json_to_colour(const QJsonArray& arr);
};

} // namespace ollygon
