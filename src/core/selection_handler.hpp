#pragma once

#include <QObject>
#include <unordered_set>
#include <vector>
#include "scene.hpp"
#include "edit_mode.hpp"

namespace ollygon {

//////// component selection data for a single mesh
// "components" are either v/e/f, the things available in edit mode
struct ComponentSelection {
    std::unordered_set<uint32_t> vertices;
    std::unordered_set<uint32_t> edges; // stored as (v1 * vertex_count + v2)
    std::unordered_set<uint32_t> faces; // triangle indices (indices.size()/3)

    void clear() {
        vertices.clear(); edges.clear(); faces.clear();
    }

    bool is_empty() const {
        return vertices.empty() && edges.empty() && faces.empty();
    }
};

class SelectionHandler : public QObject {
    Q_OBJECT

public:
    explicit SelectionHandler(QObject* parent = nullptr);

    // single selection convenience (returns last selected, or nullptr)
    SceneNode* get_selected_node() const {
        return selected_nodes.empty() ? nullptr : selected_nodes.back();
    }

    // multiple selection
    const std::vector<SceneNode*>& get_selected_nodes() const { return selected_nodes; }
    bool is_selected(SceneNode* node) const;
    size_t selection_count() const { return selected_nodes.size(); }

    // component selection
    const ComponentSelection& get_component_selection() const { return component_selection; }
    void set_component_selection(const ComponentSelection& new_selection);
    bool has_component_selection() const { return !component_selection.is_empty(); }

    bool raycast_select_moded(Scene* scene, const Vec3& ray_origin, const Vec3& ray_dir, EditMode mode, bool add_to_selection = false);

public slots:
    void set_selected(SceneNode* node);
    void add_to_selection(SceneNode* node);
    void remove_from_selection(SceneNode* node);
    void toggle_selection(SceneNode* node);
    void set_selection(const std::vector<SceneNode*>& nodes);
    void clear_selection();
    void clear_component_selection();

signals:
    void selection_changed(SceneNode* node);  // emits last selected (or nullptr)
    void component_selection_changed();

private:
    std::vector<SceneNode*> selected_nodes;
    ComponentSelection component_selection;

    bool raycast_anything_get_scenenode(SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, float& closest_t, SceneNode*& hit_node);
    bool raycast_vertex(SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, uint32_t& vertex_index, float& closest_dist);
    bool raycast_edge(SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, uint32_t& v1_index, uint32_t& v2_index, float& closest_dist);
    bool raycast_face(SceneNode* node, const Vec3& ray_origin, const Vec3& ray_dir, uint32_t& face_index, float& closest_t);

    // helper to get edge hash for storage
    uint32_t edge_hash(uint32_t v1, uint32_t v2, uint32_t vertex_count) const {
        if (v1 > v2) std::swap(v1, v2);
        return v1 * vertex_count + v2;
    }
};

} // namespace ollygon
