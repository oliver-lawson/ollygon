#include "toolbar_edit_mode.hpp"

namespace ollygon {

ToolbarEditMode::ToolbarEditMode(EditModeManager* manager, QWidget* parent)
    : QToolBar("Edit Mode", parent)
    , mode_manager(manager)
{
    setMovable(false);
    setFloatable(false);
    
    setup_ui();
    
    // listen to mode changes from elsewhere (keyboard shortcuts etc)
    connect(mode_manager, &EditModeManager::mode_changed,
            this, &ToolbarEditMode::on_mode_changed);
    
    // set initial states
    update_button_states();
}

void ToolbarEditMode::setup_ui() {
    // create a container widget to hold our buttons with custom layout
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    
    button_group = new QButtonGroup(this);
    button_group->setExclusive(true);
    
    // create buttons
    btn_vertex = new QPushButton("Vertex");
    btn_vertex->setCheckable(true);
    btn_vertex->setToolTip("Vertex mode (1)");
    button_group->addButton(btn_vertex, (int)EditMode::Vertex);
    layout->addWidget(btn_vertex);
    
    btn_edge = new QPushButton("Edge");
    btn_edge->setCheckable(true);
    btn_edge->setToolTip("Edge mode (2)");
    button_group->addButton(btn_edge, (int)EditMode::Edge);
    layout->addWidget(btn_edge);
    
    btn_face = new QPushButton("Face");
    btn_face->setCheckable(true);
    btn_face->setToolTip("Face mode (3)");
    button_group->addButton(btn_face, (int)EditMode::Face);
    layout->addWidget(btn_face);
    
    btn_object = new QPushButton("Object");
    btn_object->setCheckable(true);
    btn_object->setToolTip("Object mode (4)");
    button_group->addButton(btn_object, (int)EditMode::Object);
    layout->addWidget(btn_object);
    
    btn_sculpt = new QPushButton("Sculpt");
    btn_sculpt->setCheckable(true);
    btn_sculpt->setToolTip("Sculpt mode (5)");
    btn_sculpt->setEnabled(false);//TEMP
    button_group->addButton(btn_sculpt, (int)EditMode::Sculpt);
    layout->addWidget(btn_sculpt);
    
    layout->addStretch();
    
    addWidget(container);
    
    connect(button_group, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &ToolbarEditMode::on_button_clicked);
}

void ToolbarEditMode::on_button_clicked(int id) {
    mode_manager->set_mode((EditMode)id);
}

void ToolbarEditMode::on_mode_changed(EditMode mode) {
    update_button_states();
}

void ToolbarEditMode::update_button_states() {
    // update which button is checked
    QAbstractButton* btn = button_group->button((int)mode_manager->get_mode());
    if (btn) {
        btn->setChecked(true);
    }
}

} // namespace ollygon
