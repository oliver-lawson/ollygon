#include "drag_spin_box.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QStyleOption>
#include <cmath>

namespace ollygon {

DragSpinBox::DragSpinBox(QWidget* parent)
    : QWidget(parent)
    , current_value(0.0f)
    , min_value(-1000.0f)
    , max_value(1000.0f)
    , drag_speed(0.01f)
    , decimal_places(3)
    , is_dragging(false)
    , is_hovering(false)
    , text_edit(nullptr)
    , in_edit_mode(false)
{
    setMinimumHeight(20);
    setMaximumHeight(24);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::SizeHorCursor);
    
    // hidden text_edit for typing mode
    text_edit = new QLineEdit(this);
    text_edit->hide();
    text_edit->setFrame(false);
    text_edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    connect(text_edit, &QLineEdit::editingFinished, this, &DragSpinBox::end_edit_mode);
}

void DragSpinBox::set_value(float value) {
    float clamped = std::clamp(value, min_value, max_value);
    if (std::abs(current_value - clamped) > 1e-6f) {
        current_value = clamped;
        update();
    }
}

void DragSpinBox::set_range(float min, float max) {
    min_value = min;
    max_value = max;
    current_value = std::clamp(current_value, min_value, max_value);
    update();
}

void DragSpinBox::paintEvent(QPaintEvent* event) {
    if (in_edit_mode) return;  // this is just for non-text mode
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    // bg
    QColor bg_colour = is_hovering ? QColor(70, 70, 70) : QColor(60, 60, 60);
    if (is_dragging) bg_colour = QColor(80, 80, 80);
    
    painter.fillRect(rect, bg_colour);
    
    // border
    painter.setPen(QPen(QColor(40, 40, 40), 1));
    painter.drawRect(rect.adjusted(0, 0, -1, -1));
    
    // text
    QString value_text = QString::number(current_value, 'f', decimal_places);
    
    painter.setPen(QColor(220, 220, 220));
    QFont font = painter.font();
    font.setPixelSize(11);
    painter.setFont(font);
    
    painter.drawText(rect.adjusted(4, 0, -4, 0), Qt::AlignRight | Qt::AlignVCenter, value_text);
}

void DragSpinBox::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        is_dragging = true;
        drag_start_pos = event->pos();
        drag_start_value = current_value;
        setCursor(Qt::BlankCursor);  // hide cursor while dragging
        update();
    }
}

void DragSpinBox::mouseMoveEvent(QMouseEvent* event) {
    if (is_dragging) {
        int delta_x = event->pos().x() - drag_start_pos.x();
        
        // adjust speed based on modifiers
        float speed = drag_speed;
        if (event->modifiers() & Qt::ShiftModifier) {
            speed *= 0.1f;  // fine control
        }
        if (event->modifiers() & Qt::ControlModifier) {
            speed *= 10.0f;  // bigger control
        }
        
        float new_value = drag_start_value + delta_x * speed;
        new_value = std::clamp(new_value, min_value, max_value);
        
        if (std::abs(new_value - current_value) > 1e-6f) {
            current_value = new_value;
            emit value_changed(current_value);
            update();
        }
        
        // wrap cursor if we go too far
        if (std::abs(delta_x) > 200) {
            QPoint global_pos = mapToGlobal(drag_start_pos);
            QCursor::setPos(global_pos);
            drag_start_pos = event->pos();
            drag_start_value = current_value;
        }
    }
}

void DragSpinBox::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && is_dragging) {
        is_dragging = false;
        setCursor(Qt::SizeHorCursor);
        
        // restore cursor to widget center
        QPoint center = rect().center();
        QCursor::setPos(mapToGlobal(center));
        
        update();
    }
}

void DragSpinBox::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        start_edit_mode();
    }
}

void DragSpinBox::wheelEvent(QWheelEvent* event) {
    float speed = drag_speed * 10.0f;  // wheel is coarser
    if (event->modifiers() & Qt::ShiftModifier) {
        speed *= 0.1f;
    }
    
    float delta = event->angleDelta().y() > 0 ? speed : -speed;
    float new_value = std::clamp(current_value + delta, min_value, max_value);
    
    if (std::abs(new_value - current_value) > 1e-6f) {
        current_value = new_value;
        emit value_changed(current_value);
        update();
    }
    
    event->accept();
}

void DragSpinBox::enterEvent(QEnterEvent* event) {
    is_hovering = true;
    update();
}

void DragSpinBox::leaveEvent(QEvent* event) {
    is_hovering = false;
    update();
}

void DragSpinBox::resizeEvent(QResizeEvent* event) {
    // resize text edit to match widget
    if (text_edit) {
        text_edit->setGeometry(rect().adjusted(2, 2, -2, -2));
    }
}

void DragSpinBox::start_edit_mode() {
    in_edit_mode = true;
    text_edit->setText(QString::number(current_value, 'f', decimal_places));
    text_edit->show();
    text_edit->setFocus();
    text_edit->selectAll();
    setCursor(Qt::IBeamCursor);
}

void DragSpinBox::end_edit_mode() {
    if (!in_edit_mode) return;
    
    update_value_from_text();
    in_edit_mode = false;
    text_edit->hide();
    setCursor(Qt::SizeHorCursor);
    update();
}

void DragSpinBox::update_value_from_text() {
    bool ok;
    float new_value = text_edit->text().toFloat(&ok);
    
    if (ok) {
        new_value = std::clamp(new_value, min_value, max_value);
        if (std::abs(new_value - current_value) > 1e-6f) {
            current_value = new_value;
            emit value_changed(current_value);
        }
    }
}

} // namespace ollygon
