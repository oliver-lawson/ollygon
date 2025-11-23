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

} // namespace ollygon
