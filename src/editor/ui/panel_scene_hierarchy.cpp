#include "panel_scene_hierarchy.hpp"
#include <QPainter>

namespace ollygon {

enum Column {
    Name = 0,
    Visible = 1,
    Locked = 2,
    ColumnCount = 3
};

// == Tree ==

SceneHierarchyTree::SceneHierarchyTree(QWidget* parent)
    : QTreeWidget(parent)
    , selection_handler(nullptr)
{
    setColumnCount(Column::ColumnCount);
    setHeaderLabels({ "Name", "👁", "🔒" });

    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(Column::Name, QHeaderView::Stretch);
    header()->setSectionResizeMode(Column::Visible, QHeaderView::Fixed);
    header()->setSectionResizeMode(Column::Locked, QHeaderView::Fixed);
    setColumnWidth(Column::Visible, 30);
    setColumnWidth(Column::Locked, 30);

    header()->setContextMenuPolicy(Qt::CustomContextMenu); //TODO
    
    setAlternatingRowColors(true);
    setIndentation(15);
    
    connect(this, &QTreeWidget::itemClicked, this, &SceneHierarchyTree::on_item_clicked);
}

void SceneHierarchyTree::populate_from_scene(Scene* scene)
{
    clear();
    if (!scene) return;

    add_node_recursive(scene->get_root(), nullptr);
    expandAll();
}

void SceneHierarchyTree::set_selection_handler(SelectionHandler* new_handler) {
    selection_handler = new_handler;
}

void SceneHierarchyTree::add_node_recursive(SceneNode* node, QTreeWidgetItem* parent)
{
    QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);

    item->setData(Column::Name, Qt::UserRole, QVariant::fromValue(static_cast<void*>(node)));
    update_item_display(item, node);

    for (auto& child : node->children) {
        add_node_recursive(child.get(), item);
    }
}

SceneNode* SceneHierarchyTree::get_node_from_item(QTreeWidgetItem* item)
{
    if (!item) return nullptr;
    return static_cast<SceneNode*>(item->data(Column::Name, Qt::UserRole).value<void*>());
}

void SceneHierarchyTree::update_item_display(QTreeWidgetItem* item, SceneNode* node)
{
    // name column
    QString display_name = QString::fromStdString(node->name);
    item->setText(Column::Name, display_name);

    // visible column
    item->setText(Column::Visible, node->visible ? "👁" : "");
    item->setTextAlignment(Column::Visible, Qt::AlignCenter);

    // locked column
    item->setText(Column::Locked, node->locked ? "🔒" : "");
    item->setTextAlignment(Column::Locked, Qt::AlignCenter);

    // grey out invisible nodes
    QColor text_colour = node->visible ? palette().text().color() : Qt::gray;
    item->setForeground(Column::Name, text_colour);
}


// slots
void SceneHierarchyTree::refresh_display() {
    // walk all items and update
    // TODO track node mappings and update granularly
    // TODO centralise to newly selected node?
    std::function<void(QTreeWidgetItem*)> refresh_item = [&](QTreeWidgetItem* item) {
        SceneNode* node = get_node_from_item(item);
        if (node) {
            update_item_display(item, node);
        }
        for (int i = 0; i < item->childCount(); i++) {
            refresh_item(item->child(i));
        }
    };

    for (int i = 0; i < topLevelItemCount(); ++i) {
        refresh_item(topLevelItem(i));
    }
}

void SceneHierarchyTree::on_item_clicked(QTreeWidgetItem* item, int column) {
    SceneNode* node = get_node_from_item(item);
    if (!node) return;

    if (column == Column::Visible) {
        node->visible = !node->visible;
        update_item_display(item, node);
        emit node_visibility_toggled(node);
    }
    else if (column == Column::Locked) {
        node->locked = !node->locked;
        update_item_display(item, node);
        emit node_locked_toggled(node);
    }
    else if (column == Column::Name) {
        // select!
        if (selection_handler && node->name != "Root" && !node->locked) {
            selection_handler->set_selected(node);
        }
    }
}

// == Panel ==

PanelSceneHierarchy::PanelSceneHierarchy(QWidget* parent)
    : QWidget(parent)
    , scene(nullptr)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tree = new SceneHierarchyTree();
    layout->addWidget(tree);

    // connect any tree signals to single "modified" signal
    connect(tree, &SceneHierarchyTree::node_visibility_toggled, this, &PanelSceneHierarchy::scene_modified);
    connect(tree, &SceneHierarchyTree::node_locked_toggled, this, &PanelSceneHierarchy::scene_modified);
}

void PanelSceneHierarchy::set_scene(Scene* new_scene) {
    scene = new_scene;
    rebuild_tree();
}

void PanelSceneHierarchy::set_selection_handler(SelectionHandler* new_handler) {
    tree->set_selection_handler(new_handler);

    // listen to selection changes
    if (new_handler) {
        connect(new_handler, &SelectionHandler::selection_changed, tree, &SceneHierarchyTree::refresh_display);
    }
}

void PanelSceneHierarchy::rebuild_tree() {
    tree->populate_from_scene(scene);
}

} // namespace ollygon
