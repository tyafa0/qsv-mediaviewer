#ifndef BOOKSHELFWIDGET_H
#define BOOKSHELFWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QDir>
#include <QMutex>
#include <QCache>
#include <QCollator>
#include <QFutureWatcher>

#include "utils/common_types.h"

class BookshelfWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BookshelfWidget(QWidget *parent = nullptr);
    ~BookshelfWidget();

    void setSortOrder(SortMode mode, bool ascending = true);

    // 外部から操作するためのパブリックメソッド
    void navigateToPath(const QString &path);
    void setIcons(const QMap<QString, QIcon> &icons);
    void setImageExtensions(const QStringList &extensions);
    void refresh(); // 表示更新
    QString currentPath() const;

public slots:
    // UIオプションからの変更を受け取るスロット
    void setFontSize(int size);
    void setThumbnailsVisible(bool visible);
    void setShowImages(bool show);
    void setSyncDateFont(bool sync);

signals:
    // MainWindowへ通知するシグナル
    void directoryChanged(const QString &path);       // ディレクトリ移動時
    void imageFileSelected(const QString &filePath);  // 画像ファイル選択時（プレビュー用）
    void folderSelected(const QString &path);         // フォルダ選択時（ナビゲーション用）

private slots:
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    // UIコンポーネント
    QListWidget *m_listWidget;

    // データ・設定
    QString m_currentPath;
    QStringList m_imageExtensions;
    bool m_thumbnailsVisible;
    bool m_showImages;
    bool m_syncDateFont;
    int m_currentFontSize;
    QMap<QString, QIcon> m_icons;

    // 状態保持
    QMap<QString, int> m_scrollHistory;

    // サムネイル処理関連
    QCache<QString, QPixmap> m_thumbnailCache;
    QMutex m_thumbnailCacheMutex;
    QList<QFutureWatcher<QPixmap>*> m_activeWatchers;

    // --- ★ 追加: ソート用メンバ ---
    QCollator m_collator;       // 自然順ソート用クラス
    SortMode m_currentSortMode; // 現在のソートモード
    bool m_sortAscending;       // 昇順(true)か降順(false)か
    // --- ★ 追加: ソート実行ヘルパー関数 ---
    void sortFileInfos(QFileInfoList &list);

    // 内部ヘルパー関数
    void updateView();
    QWidget* createItemWidget(const QString &name, const QString &path, const QIcon &icon, bool isImageFile);
    void loadThumbnailAsync(const QString &dirPath, const QSize &size, bool isImageFile);
    QString findFirstImageIn(const QString &dirPath);
    QIcon getIcon(const QString &name) const;
};

#endif // BOOKSHELFWIDGET_H
