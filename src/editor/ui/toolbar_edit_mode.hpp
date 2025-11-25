#pragma once

#include <QToolBar>
#include <QHBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include "core/edit_mode.hpp"

namespace ollygon {

//forward decs
class SelectionHandler;

// horizontal toolbar with mode buttons
class ToolbarEditMode : public QToolBar {
    Q_OBJECT

public:
    explicit ToolbarEditMode(EditModeManager* mode_manager, SelectionHandler* selection_handler, QWidget* parent = nullptr);

private slots:
    void on_mode_changed(EditMode mode);
    void on_selection_changed(SceneNode* node);
    void on_button_clicked(int id);

private:
    void setup_ui();
    void update_button_states();

    EditModeManager* mode_manager;
    SelectionHandler* selection_handler;
    QButtonGroup* button_group;

    QPushButton* btn_vertex;
    QPushButton* btn_edge;
    QPushButton* btn_face;
    QPushButton* btn_object;
    QPushButton* btn_sculpt;
};

} // namespace ollygon
