#include "clickable_slider.h"
#include <QMouseEvent>
#include <QStyle>
#include <QWheelEvent>
#include <qstyleoption.h>

ClickableSlider::ClickableSlider(QWidget *parent)
    : QSlider(parent)
{
}

void ClickableSlider::mousePressEvent(QMouseEvent *event)
{
    // QStyleを使って、スライダーの有効な操作範囲（溝の部分）を取得します
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    // ハンドル自身をクリックした場合は、通常のドラッグ動作に任せます
    if (handleRect.contains(event->pos())) {
        QSlider::mousePressEvent(event);
        return;
    }

    // ハンドル以外の場所がクリックされた場合の処理
    if (event->button() == Qt::LeftButton) {
        int newValue;
        if (orientation() == Qt::Horizontal) {
            // 水平スライダーの場合
            double posRatio = static_cast<double>(event->pos().x()) / width();
            if (property("inverted").toBool()) {
                posRatio = 1.0 - posRatio;
            }
            newValue = minimum() + qRound(posRatio * (maximum() - minimum()));
        } else {
            // 垂直スライダーの場合
            double posRatio = static_cast<double>(event->pos().y()) / height();
            newValue = minimum() + qRound(posRatio * (maximum() - minimum()));
        }
        if (invertedAppearance()) {
            // 計算した値を反転させる
            newValue = maximum() - (newValue - minimum());
        }

        // 値が範囲内に収まるように補正
        if (newValue < minimum()) newValue = minimum();
        if (newValue > maximum()) newValue = maximum();

        setValue(newValue); // 計算した値にハンドルを移動

        // ハンドルを掴んだ時と同じように、即座にシグナルが発行されるようにする
        event->accept();
        sliderPressed();
        sliderMoved(newValue);
        sliderReleased();
    } else {
        QSlider::mousePressEvent(event);
    }
}

void ClickableSlider::wheelEvent(QWheelEvent *event)
{
    // QSlider のデフォルトのホイール処理（setValue）を無効化し、
    // 代わりに MainWindow の eventFilter で処理できるように、
    // イベントだけを「受理 (accept)」します。
    event->accept();
}
