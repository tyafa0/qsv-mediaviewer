#include "listoptionswidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QSpacerItem>

ListOptionsWidget::ListOptionsWidget(const QIcon &icon, QWidget *parent)
    : QWidget(parent)
{
    // 1. UI部品の作成
    m_layout = new QHBoxLayout(this);
    m_label = new QLabel(this);
    m_label->setPixmap(icon.pixmap(QSize(16, 16))); // アイコンを設定 (16x16)
    m_label->setFixedSize(16, 16);                 // サイズをアイコンに固定
    m_label->setToolTip("テキストサイズ");
    m_spinBox = new QSpinBox(this);
    m_checkBox = new QCheckBox("順番入れ替え", this);

    // 2. 詳細設定
    m_spinBox->setMinimum(8);
    m_spinBox->setMaximum(72);
    m_spinBox->setValue(10);
    m_spinBox->setMaximumSize(64, 16777215); // サイズを固定

    // 3. レイアウト
    m_layout->addWidget(m_label);
    m_layout->addWidget(m_spinBox);
    m_layout->addWidget(m_checkBox);
    m_spacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_layout->addSpacerItem(m_spacer);
    m_layout->setContentsMargins(0, 0, 0, 0); // 余白を詰める
    m_layout->setSpacing(3);

    setLayout(m_layout);

    // 4. 内部シグナルを外部シグナルに中継
    connect(m_spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ListOptionsWidget::fontSizeChanged);

    connect(m_checkBox, &QCheckBox::toggled,
            this, &ListOptionsWidget::reorderToggled);
}

// --- 外部から状態を操作するための関数 ---
void ListOptionsWidget::addOptionWidget(QWidget* widget)
{
    // レイアウト内のアイテム数を確認
    // 通常は [Label, SpinBox, CheckBox, Spacer] の順なので
    // count() - 1 の位置（Spacerの直前）に挿入します。
    int index = m_layout->count() - 1;
    if (index < 0) index = 0;

    m_layout->insertWidget(index, widget);
}


void ListOptionsWidget::setFontSize(int size)
{
    // 外部から値を設定されたときに、シグナルループを防ぐ
    m_spinBox->blockSignals(true);
    m_spinBox->setValue(size);
    m_spinBox->blockSignals(false);
}

void ListOptionsWidget::setReorderEnabled(bool enabled)
{
    // 外部から値を設定されたときに、シグナルループを防ぐ
    m_checkBox->blockSignals(true);
    m_checkBox->setChecked(enabled);
    m_checkBox->blockSignals(false);
}

// --- 外部からウィジェットの表示/非表示を切り替える ---

void ListOptionsWidget::setReorderVisible(bool visible)
{
    m_checkBox->setVisible(visible);
}

void ListOptionsWidget::setIcon(const QIcon &icon)
{
    m_label->setPixmap(icon.pixmap(QSize(16, 16)));
}

int ListOptionsWidget::value() const
{
    return m_spinBox->value();
}

bool ListOptionsWidget::isChecked() const
{
    return m_checkBox->isChecked();
}
