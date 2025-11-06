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
    setIndentation(15);

    // disable Qt's built-in rounded selection bar.
    // with AlternatingRowColors=false it's subtle but a rounded
    // bar is visible below other selected rows
    // with AlternatingRowColors true, it disables the rounded bar on hover, which
    // is much nicer. Alternating.. seems to overrule something and we get the
    // rounded bar forced on us on the active element, but it looks OK
    setSelectionMode(QAbstractItemView::NoSelection);
    // I can't seem to figure how to paint behind the text yet, across the
    // entire rows, so for now lets just do this and use the custom
    // paint for the selection highlights (which sadly have to render
    // over the text)
    setAlternatingRowColors(true);
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

// Qt's native Windows style seems to  aggressively paint its own bg in drawRow(), 
// overwriting any custom painting we do beforehand? tried many things and couldn't
// get shot of it.  the best solution I landed upon is to paint in paintEvent()
// AFTER the base class finishes, so our overlays sit on top. We use 
// semi-transparent colours so text remains visible underneath.
// TODO: maybe replace this whole thing with qml...
void SceneHierarchyTree::paintEvent(QPaintEvent* event) {
    // let qt paint defaults
    QTreeWidget::paintEvent(event);

    // paint our custom overlay on top
    QPainter painter(viewport());

    for (int i = 0; i < topLevelItemCount(); ++i) {
        paint_item_recursive(topLevelItem(i), &painter);
    }
}

void SceneHierarchyTree::paint_item_recursive(QTreeWidgetItem* item, QPainter* painter) const {
    if (!item) return;

    QModelIndex index = indexFromItem(item, 0);
    if (!index.isValid()) return;

    QRect item_rect = visualRect(index);
    if (!item_rect.isValid()) return;

    // extend to full width
    item_rect.setLeft(0);
    item_rect.setRight(viewport()->width());

    // draw selection
    SceneNode* node = get_node_from_item(item);
    if (node && selection_handler && selection_handler->get_selected() == node) {
        QColor selection_colour(255, 140, 0, 50);
        painter->fillRect(item_rect, selection_colour);
    }

    // recurse to children
    for (int i = 0; i < item->childCount(); ++i) {
        paint_item_recursive(item->child(i), painter);
    }
}

void SceneHierarchyTree::filter_items(const QString& filter_text)
{
    if (filter_text.isEmpty()) {
        // show everything
        for (int i = 0; i < topLevelItemCount(); i++) {
            QTreeWidgetItem* item = topLevelItem(i);
            item->setHidden(false);
            filter_item_recursive(item, "");
        }
        expandAll();
    }
    else {
        // filter and show matching items + their parents
        for (int i = 0; i < topLevelItemCount(); i++)
        {
            QTreeWidgetItem* item = topLevelItem(i);
            bool visible = filter_item_recursive(item, filter_text);
            item->setHidden(!visible);
        }
    }
}

bool SceneHierarchyTree::filter_item_recursive(QTreeWidgetItem* item, const QString& filter_text)
{
    if (!item) return false;

    // show all
    if (filter_text.isEmpty()) {
        item->setHidden(false);
        for (int i = 0; i < item->childCount(); i++) {
            filter_item_recursive(item->child(i), filter_text);
        }
        return true;
    }

    // check if this item matches
    bool matches = item->text(Column::Name).contains(filter_text, Qt::CaseInsensitive);

    // check if any children match
    bool any_child_visible = false;
    for (int i = 0; i < item->childCount(); i++) {
        if (filter_item_recursive(item->child(i), filter_text)) {
            any_child_visible = true;
        }
    }

    // show this item if matches OR any child is visible
    bool should_show = matches || any_child_visible;
    item->setHidden(!should_show);
    return should_show;
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

SceneNode* SceneHierarchyTree::get_node_from_item(QTreeWidgetItem* item) const
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

    // force repaint so we can see previously selected rows get deselected too
    viewport()->update();
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
    layout->setSpacing(2);

    // search filter
    filter_edit = new QLineEdit();
    filter_edit->setPlaceholderText("Search...");
    filter_edit->setClearButtonEnabled(true);
    layout->addWidget(filter_edit);

    //tree

    tree = new SceneHierarchyTree();
    layout->addWidget(tree);

    // connect tree/filter
    connect(filter_edit, &QLineEdit::textChanged, this, &PanelSceneHierarchy::on_filter_changed);

    // connect any tree signals to single "modified" signal
    connect(tree, &SceneHierarchyTree::node_visibility_toggled, this, &PanelSceneHierarchy::scene_modified);
    connect(tree, &SceneHierarchyTree::node_locked_toggled, this, &PanelSceneHierarchy::scene_modified);
}

void PanelSceneHierarchy::on_filter_changed(const QString& new_text)
{
    tree->filter_items(new_text);
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
