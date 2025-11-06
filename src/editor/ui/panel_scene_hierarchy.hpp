#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include "core/scene.hpp"
#include "core/selection_handler.hpp"

namespace ollygon {

class SceneHierarchyTree : public QTreeWidget {
    Q_OBJECT

public:
    explicit SceneHierarchyTree(QWidget* parent = nullptr);

    void populate_from_scene(Scene* scene);
    void set_selection_handler(SelectionHandler* new_handler);

signals:
    void node_visibility_toggled(SceneNode* node);
    void node_locked_toggled(SceneNode* node);

protected:
    void paintEvent(QPaintEvent* event) override;


public slots:
    void refresh_display();

private slots:
    void on_item_clicked(QTreeWidgetItem* item, int column);

private:
    void add_node_recursive(SceneNode* node, QTreeWidgetItem* parent);
    SceneNode* get_node_from_item(QTreeWidgetItem* item) const;
    void update_item_display(QTreeWidgetItem* item, SceneNode* node);
    void paint_item_recursive(QTreeWidgetItem* item, QPainter* painter) const;

    SelectionHandler* selection_handler;
};

// TODO - pretty empty for now, widget wrapper that will make more sense
// when we have new item buttons, fitler search etc
class PanelSceneHierarchy : public QWidget {
    Q_OBJECT

public:
    explicit PanelSceneHierarchy(QWidget* parent = nullptr);

    void set_scene(Scene* scene);
    void set_selection_handler(SelectionHandler* selectionhandler);
    void rebuild_tree();

    SceneHierarchyTree* tree; // public so we can call a cheaper refresh_display on it from main_window

signals:
    void scene_modified();

private:
    Scene* scene;
};

} // namespace ollygon
