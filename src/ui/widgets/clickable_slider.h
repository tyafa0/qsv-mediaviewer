#ifndef CLICKABLE_SLIDER_H
#define CLICKABLE_SLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QWheelEvent>

class QWheelEvent;

class ClickableSlider : public QSlider
{
    Q_OBJECT

public:
    explicit ClickableSlider(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
};

#endif // CLICKABLE_SLIDER_H
