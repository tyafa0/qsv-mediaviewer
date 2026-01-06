#include "pixmap_object.h"
#include <QPainter>

PixmapObject::PixmapObject(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    // C++11以降では、プロパティのアニメーションを有効にするために初期値設定が必要
    setProperty("scale", 1.0);
}

PixmapObject::PixmapObject(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsObject(parent), m_pixmap(pixmap)
{
    setProperty("scale", 1.0);
}

QRectF PixmapObject::boundingRect() const
{
    return m_pixmap.rect();
}

void PixmapObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->drawPixmap(0, 0, m_pixmap);
}

void PixmapObject::setPixmap(const QPixmap &pixmap)
{
    prepareGeometryChange(); // 描画範囲が変わることをシーンに通知
    m_pixmap = pixmap;
    update(); // 再描画を要求
}

QPixmap PixmapObject::pixmap() const
{
    return m_pixmap;
}
