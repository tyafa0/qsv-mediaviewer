#ifndef LISTOPTIONSWIDGET_H
#define LISTOPTIONSWIDGET_H

#include <QWidget>
#include <QIcon>
#include <qlayoutitem.h>

// Foward declarations (クラスの事前宣言)
class QHBoxLayout;
class QLabel;
class QSpinBox;
class QCheckBox;

class ListOptionsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ListOptionsWidget(const QIcon &icon, QWidget *parent = nullptr);

    // --- 外部から状態を操作するための関数 ---
    void setFontSize(int size);
    void setReorderEnabled(bool enabled);

    // --- 外部からウィジェットの表示/非表示を切り替える ---
    void setReorderVisible(bool visible);
    void setIcon(const QIcon &icon);
    void addOptionWidget(QWidget* widget);

    int value() const;
    bool isChecked() const;

signals:
    // --- 外部へ変更を通知するためのシグナル ---
    void fontSizeChanged(int size);
    void reorderToggled(bool checked);

private:
    // --- このウィジェットが内部に持つUI部品 ---
    QHBoxLayout *m_layout;
    QLabel      *m_label;
    QSpinBox    *m_spinBox;
    QCheckBox   *m_checkBox;
    QSpacerItem* m_spacer;
};

#endif // LISTOPTIONSWIDGET_H
