#include "mediaitemdelegate.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>

MediaItemDelegate::MediaItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void MediaItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // 1. 状態の取得
    bool isSelected = (opt.state & QStyle::State_Selected);
    bool isHovered  = (opt.state & QStyle::State_MouseOver);
    bool isActive   = index.data(IsActiveRole).toBool();

    // 2. 色の定義 (ライト/ダーク共通)
    // ピンク背景時の文字色は「黒」で統一して可読性を確保する
    QColor pinkColor     = QColor("#ffc0cb");      // 選択色 (Pink)
    QColor darkPinkColor = QColor("#ff69b4");      // 再生中+選択色 (HotPink)
    QColor hoverColor    = QColor("#ffe4e1");      // ホバー色 (MistyRose)
    QColor onPinkText    = Qt::black;              // ★ ピンク背景上の文字色 (統一)

    // デフォルトの文字色・背景色 (テーマ依存)
    QColor defaultText = opt.palette.text().color();
    QColor defaultBg = opt.palette.base().color();

    // 3. 背景とテキスト色の決定
    QColor bgColor = defaultBg;
    QColor textColor = defaultText;
    bool makeBold = false;
    bool shouldPaintBackground = false;

    if (isActive) {
        shouldPaintBackground = true;
        if (isSelected) {
            // [条件4] 表示中 かつ 選択中
            bgColor = darkPinkColor;
            textColor = onPinkText; // ★ 黒に統一
            makeBold = true;
        } else {
            // [条件3] 表示中 かつ 非選択中 (反転表示)
            // ここはテーマに合わせて反転させる (ダークモードなら白背景・黒文字)
            bgColor = defaultText;
            textColor = defaultBg;
        }
    } else {
        if (isSelected) {
            // [条件1] 非表示中 かつ 選択中
            shouldPaintBackground = true;
            bgColor = pinkColor;
            textColor = onPinkText; // ★ 黒に統一
        } else if (isHovered) {
            // [条件5] 非表示中 かつ 非選択中 かつ ホバー中
            shouldPaintBackground = true;
            bgColor = hoverColor;
            textColor = onPinkText; // ★ 黒に統一
        } else {
            // [条件2] 何もなし -> デフォルト
        }
    }

    painter->save();

    // 4. 背景の描画
    if (shouldPaintBackground) {
        painter->fillRect(opt.rect, bgColor);
    }

    // 5. テキストの描画設定
    if (makeBold) {
        QFont f = opt.font;
        f.setBold(true);
        painter->setFont(f);
    }

    // パレット調整
    QPalette::ColorGroup cg = (opt.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
        cg = QPalette::Inactive;

    opt.palette.setColor(cg, QPalette::Text, textColor);
    opt.palette.setColor(cg, QPalette::HighlightedText, textColor);

    // ハイライト背景色を透明にして、標準描画が背景を上書きしないようにする
    if (shouldPaintBackground) {
        opt.palette.setColor(cg, QPalette::Highlight, Qt::transparent);
    }

    // 標準のエフェクトを消す
    opt.state &= ~QStyle::State_Selected;
    opt.state &= ~QStyle::State_MouseOver;

    // 標準描画 (テキストやアイコン)
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    painter->restore();
}
