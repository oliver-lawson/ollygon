#include "toolbar_edit_mode.hpp"
#include "core/selection_handler.hpp"

namespace ollygon {

ToolbarEditMode::ToolbarEditMode(EditModeManager* manager, SelectionHandler* selection_handler, QWidget* parent)
    : QToolBar("Edit Mode", parent)
    , mode_manager(manager)
    , selection_handler(selection_handler)
{
    setMovable(false);
    setFloatable(false);
    
    setup_ui();
    
    // listen to mode changes from elsewhere (keyboard shortcuts etc)
    connect(mode_manager, &EditModeManager::mode_changed,
            this, &ToolbarEditMode::on_mode_changed);
    // listen to selection changes to update availability
    if (selection_handler) {
        connect(selection_handler, &SelectionHandler::selection_changed,
            this, &ToolbarEditMode::on_selection_changed);
    }

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

void ToolbarEditMode::on_mode_changed(EditMode mode) {
    update_button_states();
}

void ToolbarEditMode::on_selection_changed(SceneNode* node) {
    update_button_states();
}

void ToolbarEditMode::on_button_clicked(int id) {
    EditMode mode = (EditMode)id;
    SceneNode* selected = selection_handler ? selection_handler->get_selected_node() : nullptr;

    // try to set the mode - it will only succeed if available
    mode_manager->try_set_mode(mode, selected);
}

void ToolbarEditMode::update_button_states()
{
    SceneNode* selected = selection_handler ? selection_handler->get_selected_node() : nullptr;
    EditMode current_mode = mode_manager->get_mode();

    // update each button's enabled state and checked state
    auto update_button = [&](QPushButton* btn, EditMode mode) {
        bool available = mode_manager->is_mode_available(mode, selected);
        btn->setEnabled(available);
        btn->setChecked(current_mode == mode);

        // update tooltip to explain why disabled
        if (!available && mode != EditMode::Object) {
            QString base_tooltip;
            switch (mode) {
            case EditMode::Vertex:
                base_tooltip = "Vertex mode (1)";
                break;
            case EditMode::Edge:
                base_tooltip = "Edge mode (2)";
                break;
            case EditMode::Face:
                base_tooltip = "Face mode (3)";
                break;
            case EditMode::Sculpt:
                base_tooltip = "Sculpt mode (5)";
                break;
            default:
                base_tooltip = btn->toolTip();
                break;
            }

            QString tooltip = base_tooltip;
            switch (mode) {
            case EditMode::Vertex:
            case EditMode::Edge:
            case EditMode::Face:
                if (!selected) {
                    tooltip += " - select a mesh first";
                }
                else if (selected->node_type != NodeType::Mesh) {
                    tooltip += " - mesh objects only";
                }
                else {
                    tooltip += " - no geometry";
                }
                break;
            case EditMode::Sculpt:
                tooltip += " - not implemented yet";
                break;
            default:
                break;
            }

            btn->setToolTip(tooltip);
        }
        };

    update_button(btn_vertex, EditMode::Vertex);
    update_button(btn_edge, EditMode::Edge);
    update_button(btn_face, EditMode::Face);
    update_button(btn_object, EditMode::Object);
    update_button(btn_sculpt, EditMode::Sculpt);
}

} // namespace ollygon
