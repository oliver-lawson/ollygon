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
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private slots:
    void on_item_clicked(QTreeWidgetItem* item, int column);

private:
    void add_node_recursive(SceneNode* node, QTreeWidgetItem* parent);
    SceneNode* get_node_from_item(QTreeWidgetItem* item);
    void update_item_display(QTreeWidgetItem* item, SceneNode* node);

    SelectionHandler* selection_handler;
};

class PanelSceneHierarchy : public QWidget {
    Q_OBJECT

public:
    explicit PanelSceneHierarchy(QWidget* parent = nullptr);

    void set_scene(Scene* scene);
    void set_selection_handler(SelectionHandler* selectionhandler);
    void rebuild_tree();

signals:
    void scene_modified();

private:
    Scene* scene;
    SceneHierarchyTree* tree;
};

} // namespace ollygon
