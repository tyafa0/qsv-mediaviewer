#include "panoramaview.h"
#include <QScrollBar>
#include <QResizeEvent>
#include <QDebug>

PanoramaView::PanoramaView(QWidget *parent) : QGraphicsView(parent)
{
    // コンストラクタでは特に何もする必要はありません
}

void PanoramaView::setPanoramaMode(bool enabled)
{
    m_isPanoramaMode = enabled;
}

bool PanoramaView::isResizing() const
{
    return m_isResizing;
}

// QGraphicsViewのサイズが変更されるたびに、この関数が自動的に呼び出されます
void PanoramaView::resizeEvent(QResizeEvent *event)
{
    m_isResizing = true;
    if (m_isPanoramaMode) {
        // --- パノラマモード時のリサイズ処理 ---

        // 1. Qtのレイアウトエンジンが値をリセットする「前」のスクロール位置の割合を保存します。
        //    event->oldSize() はリサイズ前のサイズで、これが有効な場合にのみ保存処理を行います。
        if (event->oldSize().isValid() && !event->oldSize().isEmpty()) {
            QScrollBar *hBar = horizontalScrollBar();
            if (hBar && hBar->maximum() > 0) {
                m_scrollRatioX = static_cast<double>(hBar->value()) / hBar->maximum();
            } else {
                m_scrollRatioX = 0.0;
            }
            QScrollBar *vBar = verticalScrollBar();
            if (vBar && vBar->maximum() > 0) {
                m_scrollRatioY = static_cast<double>(vBar->value()) / vBar->maximum();
            } else {
                m_scrollRatioY = 0.0;
            }
            qDebug() << "[PanoramaView] Storing scroll ratio. X:" << m_scrollRatioX << "Y:" << m_scrollRatioY;
        }

        // 2. まず、QGraphicsView本来の標準リサイズ処理を呼び出します。
        //    これにより、ウィジェットのサイズが更新され、スクロールバーの最大値(maximum)も再計算されます。
        //    （この過程でスクロール値は0にリセットされます）
        QGraphicsView::resizeEvent(event);

        // 3. レイアウトが完全に確定した「後」で、保存しておいた割合を使って正しい位置を復元します。
        //    Qtによって0にリセットされた値を、瞬時に正しい計算値で上書きします。
        if (horizontalScrollBar()->maximum() > 0) {
            horizontalScrollBar()->setValue(m_scrollRatioX * horizontalScrollBar()->maximum());
        }
        if (verticalScrollBar()->maximum() > 0) {
            verticalScrollBar()->setValue(m_scrollRatioY * verticalScrollBar()->maximum());
        }
        qDebug() << "[PanoramaView] Restored scroll position.";

    } else {
        // --- 標準モード時 ---
        // 通常のQGraphicsViewの動作に任せます。
        QGraphicsView::resizeEvent(event);
    }

    qDebug() << "[PanoramaView] resizeEvent finished. Flag set to false.";
    m_isResizing = false;
}
