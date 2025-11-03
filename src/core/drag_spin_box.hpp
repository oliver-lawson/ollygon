#pragma once

#include <QWidget>
#include <QLineEdit>
#include <functional>

namespace ollygon {

class DragSpinBox : public QWidget {
    Q_OBJECT

public:
    explicit DragSpinBox(QWidget* parent = nullptr);
    
    void set_value(float value);
    float get_value() const { return current_value; }
    
    void set_range(float min, float max);
    void set_speed(float speed) { drag_speed = speed; }  // units per pixel
    void set_precision(int decimals) { decimal_places = decimals; }
    
signals:
    void value_changed(float new_value);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private:
    void start_edit_mode();
    void end_edit_mode();
    void update_value_from_text();
    
    float current_value;
    float min_value;
    float max_value;
    float drag_speed;
    int decimal_places;
    
    bool is_dragging;
    bool is_hovering;
    QPoint drag_start_pos;
    float drag_start_value;
    
    QLineEdit* text_edit;
    bool in_edit_mode;
};

} // namespace ollygon
