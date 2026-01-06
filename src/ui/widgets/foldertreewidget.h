#ifndef FOLDERTREEWIDGET_H
#define FOLDERTREEWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>

class FolderTreeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FolderTreeWidget(QWidget *parent = nullptr);
    ~FolderTreeWidget();

    // 設定
    void setFilterExtensions(const QStringList &audio, const QStringList &video,
                             const QStringList &image, const QStringList &playlist);

    // 操作
    void goUp();
    void goHome();
    void setRootPath(const QString &path);
    void navigateTo(const QString &path); // 指定パスを選択・展開

    // 状態取得
    QString currentPath() const; // 現在選択中のパス（なければルート）

signals:
    // 外部への通知
    void fileActivated(const QString &path); // ダブルクリック (ファイル)
    void folderActivated(const QString &path); // ダブルクリック (フォルダ)
    void selectionChanged(const QString &path); // 選択変更

    // コンテキストメニューアクションの要求
    void requestAddToPlaylist(const QString &path);
    void requestAddToSlideshow(const QString &path);
    void requestOpenInBookshelf(const QString &path);
    void requestSetAsRoot(const QString &path);

private slots:
    void onDoubleClicked(const QModelIndex &index);
    void onCustomContextMenuRequested(const QPoint &pos);

private:
    QTreeView *m_treeView;
    QFileSystemModel *m_fileModel;

    // 拡張子フィルタ用
    QStringList m_allExtensions;
};

#endif // FOLDERTREEWIDGET_H
