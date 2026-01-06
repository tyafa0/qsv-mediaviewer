#ifndef MEDIAITEMDELEGATE_H
#define MEDIAITEMDELEGATE_H

#include <QStyledItemDelegate>

class MediaItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    // 「再生中/表示中」状態を保存するためのカスタムデータロール
    static const int IsActiveRole = Qt::UserRole + 10;

    explicit MediaItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // MEDIAITEMDELEGATE_H
