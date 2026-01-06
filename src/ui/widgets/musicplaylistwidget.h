#ifndef MUSICPLAYLISTWIDGET_H
#define MUSICPLAYLISTWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include "playlistmanager.h"

class MusicPlaylistWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MusicPlaylistWidget(PlaylistManager *manager, QWidget *parent = nullptr);
    ~MusicPlaylistWidget();

    // 外部からの操作
    void addPlaylist();
    void removePlaylist(int index);
    void loadPlaylist();
    void deleteSelectedItems();

    // 状態取得
    int count() const;
    int currentIndex() const;
    void setCurrentIndex(int index);
    QListWidget* currentListWidget() const;
    QListWidget* listWidget(int index) const;
    QString tabText(int index) const;
    void setTabText(int index, const QString &text);
    QList<QListWidget*> allListWidgets() const; // UI同期用

public slots:
    void applyUiSettings(int fontSize, bool reorderEnabled); // フォント等の適用
    void saveCurrentPlaylist();

signals:
    // MainWindowへ通知するイベント
    void playlistCountChanged();            // タブ数変更時 (ControlBar更新用)
    void currentChanged(int index);         // タブ切り替え時
    void customizeContextMenu(QMenu* menu);
    void tabRenamed(int index, const QString &name);
    void playRequest(int playlistIndex, int trackIndex); // 再生要求
    void stopRequest();                     // 停止要求 (削除時など)
    void filesAdded();                      // ファイル追加時
    void requestOpenFile();                 // "ファイルを開く"メニュー
    void requestOpenFolder();               // "フォルダを開く"メニュー
    void previewUpdateRequested();          // プレビュー更新が必要な時

private slots:
    void onTabChanged(int index);
    void onTabDoubleClicked(int index);
    void onTabContextMenu(const QPoint &pos);
    void onListContextMenu(const QPoint &pos);
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    QTabWidget *m_tabWidget;
    PlaylistManager *m_playlistManager;
    QList<QListWidget*> m_listWidgets;

    // ヘルパー
    QListWidget* createListWidget();
    void setupListConnections(QListWidget* list);
};

#endif // MUSICPLAYLISTWIDGET_H
