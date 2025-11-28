#pragma once

#include <QToolBar>
#include <QPushButton>
#include <QButtonGroup>
#include "core/selection_modes.hpp"

namespace ollygon {

class SelectionSystem;

class ToolbarSelectionMode : public QToolBar {
    Q_OBJECT

public:
    explicit ToolbarSelectionMode(SelectionSystem* selection_system, QWidget* parent = nullptr);

private slots:
    void on_mode_changed(SelectionMode mode);
    void on_button_clicked(int id);

private:
    void setup_ui();
    void update_button_states();

    SelectionSystem* selection_system;
    QButtonGroup* button_group;

    QPushButton* btn_click;
    QPushButton* btn_box;
    QPushButton* btn_lasso;
    QPushButton* btn_paint;
};

} // namespace ollygon
