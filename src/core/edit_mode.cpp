#include "edit_mode.hpp"

namespace ollygon {

EditModeManager::EditModeManager(QObject* parent)
    : QObject(parent)
    , current_mode(EditMode::Object)
{
}

bool EditModeManager::set_mode(EditMode new_mode) {
    if (current_mode == new_mode) return false;

    current_mode = new_mode;
    emit mode_changed(new_mode);
    return true;
}

void EditModeManager::cycle_mode() {
    // cycle through object -> vertex -> edge -> face -> object
    switch (current_mode) {
    case EditMode::Object:
        set_mode(EditMode::Vertex);
        break;
    case EditMode::Vertex:
        set_mode(EditMode::Edge);
        break;
    case EditMode::Edge:
        set_mode(EditMode::Face);
        break;
    case EditMode::Face:
    case EditMode::Sculpt:
        set_mode(EditMode::Object);
        break;
    }
}

} // namespace ollygon
