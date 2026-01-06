#ifndef SLIDESHOWWIDGET_H
#define SLIDESHOWWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QTimer>

class SlideshowWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SlideshowWidget(QWidget *parent = nullptr);
    ~SlideshowWidget();

    // 設定
    void setImageExtensions(const QStringList &extensions);

    // 操作
    void addNewTab();
    void closeTab(int index);
    void renameTab(int index);
    void saveCurrentList();
    void loadList();
    void loadListFromFile(const QString &filePath); // D&Dや起動時用
    QString tabText(int index) const;

    // ファイル追加（非同期チャンク処理を含む）
    void addFilesToCurrentList(const QStringList &files);

    // 状態取得
    int count() const;
    int currentIndex() const;
    QWidget* currentWidget() const;
    QListWidget* currentListWidget() const;
    QList<QListWidget*> allListWidgets() const; // UI設定同期用

public slots:
    void applyUiSettings(int fontSize, bool reorderEnabled);

signals:
    // 外部への通知
    void currentListChanged(QListWidget* list); // タブ切り替え時

    // ファイル追加処理の状態通知
    void fileLoadStarted();
    void fileLoadFinished();
    // リストへの追加が完了した際の通知 (wasInitiallyEmpty: 追加前に空だったか)
    void filesAddedToPlaylist(bool wasInitiallyEmpty);

    // UIアクション要求
    void requestFileOpen();
    void requestFolderOpen();

    // アイテム操作通知
    void itemDoubleClicked(QListWidgetItem* item);

    void listContextMenuRequested(const QPoint &pos);

private slots:
    void onTabCloseRequested(int index);
    void onTabDoubleClicked(int index);
    void onCurrentChanged(int index);
    void onListContextMenu(const QPoint &pos);
    void onTabContextMenu(const QPoint &pos);

    // 内部チャンク処理
    void processAddFilesChunk(const QStringList &files, int startIndex, bool wasInitiallyEmpty);

private:
    QTabWidget *m_tabWidget;
    QStringList m_imageExtensions;
    int m_asyncOperations; // 非同期処理のカウンタ

    QListWidget* createListWidget();
    void setupListConnections(QListWidget* list);
};

#endif // SLIDESHOWWIDGET_H
