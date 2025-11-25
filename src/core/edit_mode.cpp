#include "edit_mode.hpp"
#include "scene.hpp"

namespace ollygon {

EditModeManager::EditModeManager(QObject* parent)
    : QObject(parent)
    , current_mode(EditMode::Object)
{
}

bool EditModeManager::is_mode_available(EditMode mode, SceneNode* node) const
{   // node = selected node
    switch (mode)
    {
    case ollygon::EditMode::Object:
        //always available!
        return true;

    case ollygon::EditMode::Vertex:
    case ollygon::EditMode::Edge:
    case ollygon::EditMode::Face:
        //component modes only available for meshes
        if (!node) return false;
        return (node->node_type == NodeType::Mesh && node->geo && !node->geo->is_empty());
    case ollygon::EditMode::Sculpt: // for now
    default:
        return false;
    }
}

std::vector<EditMode> EditModeManager::get_available_modes(SceneNode* selected_node) const
{
    return std::vector<EditMode>();
}

bool EditModeManager::try_set_mode(EditMode new_mode, SceneNode* selected_node)
{
    if (!is_mode_available(new_mode, selected_node)) {
        //mode not available, try stay in current mode:
        if (is_mode_available(current_mode, selected_node)) {
            //current mode also not available! fall back to Object
            set_mode(EditMode::Object);
        }
        return false; //wasn't changed either way
    }
    return set_mode(new_mode);
}

bool EditModeManager::set_mode(EditMode new_mode) {
    if (current_mode == new_mode) return false;

    current_mode = new_mode;
    emit mode_changed(new_mode);
    return true;
}

} // namespace ollygon
