#ifndef COMPACTWINDOW_H
#define COMPACTWINDOW_H

#include <QWidget>
#include <QShortcut>

// クラスの前方宣言
class ControlBar;
class QMouseEvent;
class QPoint;

class CompactWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CompactWindow(QWidget *parent = nullptr);
    ~CompactWindow();

    // MainWindowがこのウィンドウのControlBarにアクセスするための関数
    ControlBar* controlBar() const;

signals:
    // フルモードウィンドウに戻ることをMainWindowに通知するシグナル
    void switchToFullModeRequested();
    void closed();

protected:
    // ウィンドウをドラッグで移動させるためのイベントハンドラ
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    // 右クリックメニューを表示するスロット
    void showContextMenu(const QPoint &pos);
    void toggleToFullMode();

private:
    ControlBar* m_controlBar;

    // ウィンドウ移動用の変数
    bool m_dragging = false;
    QPoint m_dragPosition;
};

#endif // COMPACTWINDOW_H
