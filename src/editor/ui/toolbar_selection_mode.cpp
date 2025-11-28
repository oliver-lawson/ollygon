#include "toolbar_selection_mode.hpp"
#include "core/selection_system.hpp"
#include <QHBoxLayout>

namespace ollygon {

ToolbarSelectionMode::ToolbarSelectionMode(SelectionSystem* system, QWidget* parent)
    : QToolBar("Selection Mode", parent)
    , selection_system(system)
{
    setMovable(false);
    setFloatable(false);

    setup_ui();

    connect(selection_system, &SelectionSystem::selection_mode_changed,
        this, &ToolbarSelectionMode::on_mode_changed);

    update_button_states();
}

void ToolbarSelectionMode::setup_ui() {
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    button_group = new QButtonGroup(this);
    button_group->setExclusive(true);

    btn_click = new QPushButton("Click");
    btn_click->setCheckable(true);
    btn_click->setToolTip("Click selection");
    button_group->addButton(btn_click, (int)SelectionMode::Click);
    layout->addWidget(btn_click);

    btn_box = new QPushButton("Box");
    btn_box->setCheckable(true);
    btn_box->setToolTip("Box selection");
    button_group->addButton(btn_box, (int)SelectionMode::Box);
    layout->addWidget(btn_box);

    btn_lasso = new QPushButton("Lasso");
    btn_lasso->setCheckable(true);
    btn_lasso->setToolTip("Lasso selection (not implemented)");//TEMP
    btn_lasso->setEnabled(false);
    button_group->addButton(btn_lasso, (int)SelectionMode::Lasso);
    layout->addWidget(btn_lasso);

    btn_paint = new QPushButton("Paint");
    btn_paint->setCheckable(true);
    btn_paint->setToolTip("Paint selection (not implemented)");//TEMP
    btn_paint->setEnabled(false);
    button_group->addButton(btn_paint, (int)SelectionMode::Paint);
    layout->addWidget(btn_paint);

    layout->addStretch();

    addWidget(container);

    connect(button_group, QOverload<int>::of(&QButtonGroup::idClicked),
        this, &ToolbarSelectionMode::on_button_clicked);
}

void ToolbarSelectionMode::on_button_clicked(int id) {
    selection_system->set_selection_mode((SelectionMode)id);
}

void ToolbarSelectionMode::on_mode_changed(SelectionMode mode) {
    update_button_states();
}

void ToolbarSelectionMode::update_button_states() {
    QAbstractButton* btn = button_group->button((int)selection_system->get_selection_mode());
    if (btn) {
        btn->setChecked(true);
    }
}

} // namespace ollygon
