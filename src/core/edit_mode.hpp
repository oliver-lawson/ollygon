#pragma once

#include <QObject>
#include <Vector>

namespace ollygon {

enum class EditMode {
    Vertex,
    Edge,
    Face,
    Object,
    Sculpt,
    EditModeCount
};

//forward decs
class SceneNode;

// global mode manager - owned by MainWindow, accessible everywhere
class EditModeManager : public QObject {
    Q_OBJECT

public:
    explicit EditModeManager(QObject* parent = nullptr);

    EditMode get_mode() const { return current_mode; }
    bool is_mode_available(EditMode mode, SceneNode* node) const;
    std::vector<EditMode> get_available_modes(SceneNode* selected_node) const;

    //returns true if mode changed
    bool try_set_mode(EditMode new_mode, SceneNode* selected_node);

    bool set_mode(EditMode new_mode);

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
