#pragma once

#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include <QPainter>
#include "scene.hpp"
#include "selection_handler.hpp"
#include "drag_spin_box.hpp"

namespace ollygon {

// classic inset dec to get some depth to things.  TODO move this elsewhere for reuse
class BorderOverlay : public QWidget {
    Q_OBJECT
public:
    BorderOverlay(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);

        QRect rect = this->rect().adjusted(0, 0, -1, -1);

        // T-L dark
        painter.setPen(QPen(QColor(10, 10, 10), 1));
        painter.drawLine(rect.topLeft(), rect.topRight());
        painter.drawLine(rect.topLeft(), rect.bottomLeft());

        // B-R light
        painter.setPen(QPen(QColor(80, 80, 80), 1));
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());
        painter.drawLine(rect.topRight(), rect.bottomRight());
    }
};

// handle resizing of our rows of custom spinboxes
class ResizeFilter : public QObject {
public:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Resize) {
            QWidget* w = qobject_cast<QWidget*>(obj);
            if (w) {
                int width = w->width();
                int spinbox_width = width / 3; // fixed to 3 per row for now
                
                //position the spinboxes
                auto children = w->findChildren<DragSpinBox*>(QString(), Qt::FindDirectChildrenOnly);
                if (children.size() >= 3) {
                    children[0]->setGeometry(0, 0, spinbox_width, 24);
                    children[1]->setGeometry(spinbox_width, 0, spinbox_width, 24);
                    children[2]->setGeometry(spinbox_width * 2, 0, width - spinbox_width * 2, 24);
                }
                
                // position overlay to cover everything
                auto overlay_widget = w->findChild<BorderOverlay*>(QString(), Qt::FindDirectChildrenOnly);
                if (overlay_widget) {
                    overlay_widget->setGeometry(0, 0, width, 24);
                    overlay_widget->raise();
                }
            }
        }
        return false;
    }
};

class PropertiesPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit PropertiesPanel(SelectionHandler* selection, QWidget* parent = nullptr);

public slots:
    void refresh_from_node();

private slots:
    void on_selection_changed(SceneNode* node);

signals:
    void properties_changed(); // visible/locked changed

private:
    void rebuild_ui(SceneNode* node);
    void create_transform_controls(SceneNode* node, QVBoxLayout* layout);
    void create_mesh_controls(SceneNode* node, QVBoxLayout* layout);
    void create_light_controls(SceneNode* node, QVBoxLayout* layout);

    void add_vec3_row(const QString& label, Vec3& vec, float min_val, float max_val, float speed, QGridLayout* grid, int row);

    void add_colour_row(const QString& label, Colour& colour, float min_val, float max_val, float speed, QGridLayout* grid, int row);

    void add_float_row(const QString& label, float& value, float min_val, float max_val, float speed, QGridLayout* grid, int row);

    SelectionHandler* selection_handler;
    QWidget* content_widget;
    QVBoxLayout* main_layout;
    SceneNode* current_node;
};

} // namespace ollygon