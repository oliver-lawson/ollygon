#pragma once

#include <QObject>

namespace ollygon {

enum class EditMode {
    Vertex,
    Edge,
    Face,
    Object,
    Sculpt,
    EditModeCount
};

// global mode manager - owned by MainWindow, accessible everywhere
class EditModeManager : public QObject {
    Q_OBJECT

public:
    explicit EditModeManager(QObject* parent = nullptr);

    EditMode get_mode() const { return current_mode; }

    //returns true if mode changed
    bool set_mode(EditMode new_mode);

    //for keyboard shortcuts handling
    void cycle_mode();

    //mode checks
    bool is_component_mode() const {
        return current_mode == EditMode::Vertex ||
               current_mode == EditMode::Edge   ||
               current_mode == EditMode::Face;
    }

    bool is_object_mode() const {return current_mode == EditMode::Object;}

signals:
    void mode_changed(EditMode new_mode);

private:
    EditMode current_mode;
};

} // namespace ollygon
