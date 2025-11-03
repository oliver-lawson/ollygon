#pragma once

#include <QObject>
#include "scene.hpp"

namespace ollygon {

// manages selection state & slot/signal messages
// TEMP - overkill currently but will be expanded to handle ordered multi selects, store them etc

class SelectionHandler : public QObject {
    Q_OBJECT

public:
    explicit SelectionHandler(QObject* parent = nullptr);

    SceneNode* get_selected() const { return selected_node; }

    bool raycast_select(
        Scene* scene,
        const Vec3& ray_origin,
        const Vec3& ray_dir
    );

public slots:
    void set_selected(SceneNode* node);
    void clear_selection();

signals:
    void selection_changed(SceneNode* node);

private:
    SceneNode* selected_node;

    bool raycast_node(SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, float& closest_t, SceneNode*& hit_node);
};

} // namespace ollygon