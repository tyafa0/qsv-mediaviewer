#ifndef PIXMAP_OBJECT_H
#define PIXMAP_OBJECT_H

#include <QGraphicsObject>
#include <QPixmap>

class PixmapObject : public QGraphicsObject
{
    Q_OBJECT
    // pos, scale, opacityプロパティをアニメーション可能にするための宣言
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)
    Q_PROPERTY(qreal scale READ scale WRITE setScale)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit PixmapObject(QGraphicsItem *parent = nullptr);
    explicit PixmapObject(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void setPixmap(const QPixmap &pixmap);
    QPixmap pixmap() const;

private:
    QPixmap m_pixmap;
};

#endif // PIXMAP_OBJECT_H
