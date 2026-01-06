#include "compactwindow.h"
#include "controlbar.h" // ControlBarクラスのヘッダをインクルード

#include <QApplication>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QMenu>

CompactWindow::CompactWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Dialog);
    // ControlBarのインスタンスを作成
    m_controlBar = new ControlBar(this);
    m_controlBar->setCompactMode(true);

    // ウィンドウのレイアウトを作成し、ControlBarを配置
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0); // ウィンドウの余白をなくす
    layout->addWidget(m_controlBar);
    setLayout(layout);

    // ウィンドウの見た目を設定
    // FramelessWindowHint: タイトルバーや枠を非表示にする
    // WindowStaysOnTopHint: 常に最前面に表示する
    // setWindowFlags(Qt::FramelessWindowHint);

    // ウィンドウサイズをControlBarのサイズに合わせる
    adjustSize();

    // 右クリックメニューを有効にする
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &CompactWindow::customContextMenuRequested, this, &CompactWindow::showContextMenu);

    QShortcut *toggleShortcut = new QShortcut(QKeySequence(Qt::Key_F10), this);
    // このショートカットはこのウィンドウがアクティブな時だけ有効
    toggleShortcut->setContext(Qt::WindowShortcut);
    connect(toggleShortcut, &QShortcut::activated, this, &CompactWindow::toggleToFullMode);
}

CompactWindow::~CompactWindow()
{
    // m_controlBarはthisの子なので自動で削除される
}

ControlBar* CompactWindow::controlBar() const
{
    return m_controlBar;
}

// 右クリックメニューの実装
void CompactWindow::showContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);
    QAction *fullModeAction = contextMenu.addAction("フルウィンドウに戻す");
    contextMenu.addSeparator();
    QAction *exitAction = contextMenu.addAction("アプリケーションを終了");

    QAction *selectedAction = contextMenu.exec(mapToGlobal(pos));

    if (selectedAction == fullModeAction) {
        emit switchToFullModeRequested(); // フルモードに戻るシグナルを送信
    } else if (selectedAction == exitAction) {
        QApplication::quit(); // アプリケーションを終了
    }
}

void CompactWindow::toggleToFullMode()
{
    // フルモードに戻る要求シグナルを発信するだけ
    emit switchToFullModeRequested();
}

void CompactWindow::closeEvent(QCloseEvent *event)
{
    // 1. ウィンドウが閉じることを通知するシグナルを発行
    emit closed();
    // 2. 本来の閉じる処理を実行
    QWidget::closeEvent(event);
}

// --- 以下、フレームレスウィンドウをマウスで移動させるための処理 ---

void CompactWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void CompactWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && m_dragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void CompactWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}
