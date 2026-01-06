#ifndef PANORAMAVIEW_H
#define PANORAMAVIEW_H

#include <QGraphicsView>

// このクラスは、パノラマモード時の特殊なリサイズ処理を自己完結させるための専用ビューです。
class PanoramaView : public QGraphicsView
{
    Q_OBJECT // Qtのオブジェクトとして機能するために必要なおまじない

public:
    // コンストラクタ
    explicit PanoramaView(QWidget *parent = nullptr);

    // MainWindowから現在のモードを通知してもらうための関数
    void setPanoramaMode(bool enabled);
    bool isResizing() const;

protected:
    // QGraphicsViewのresizeEventを我々のロジックで上書き（オーバーライド）します
    void resizeEvent(QResizeEvent *event) override;

private:
    bool m_isPanoramaMode = false; // 現在パノラマモードかどうかを保持するフラグ
    double m_scrollRatioX = 0.0;   // リサイズ直前の水平スクロール位置の割合
    double m_scrollRatioY = 0.0;   // リサイズ直前の垂直スクロール位置の割合
    bool m_isResizing = false;
};

#endif // PANORAMAVIEW_H
