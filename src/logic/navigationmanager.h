#ifndef NAVIGATIONMANAGER_H
#define NAVIGATIONMANAGER_H

#include <QObject>
#include <QFileSystemModel>
#include <QStringList>
#include <QTreeView>

class NavigationManager : public QObject
{
    Q_OBJECT

public:
    // UIのTree ViewとModelをコンストラクタで受け取ります
    explicit NavigationManager(QTreeView* treeView, QFileSystemModel* model, QObject *parent = nullptr);
    ~NavigationManager();

    // --- ゲッター ---
    QString getCurrentLocation() const { return m_currentLocation; }
    QFileSystemModel* getModel() const { return m_model; }

public slots:
    // --- UI (ボタン) から呼び出されるスロット ---
    void goBack();
    void goForward();
    void goUp();

    // --- UI (アドレスバー) から呼び出されるスロット ---
    void navigateTo(const QString& location, bool addToHistory = true);

    // --- UI (TreeView) から呼び出されるスロット ---
    void onTreeDoubleClicked(const QModelIndex &index);
    void onTreeSelectionChanged(const QModelIndex &current);

    // --- UI (コンテキストメニュー) から呼び出されるスロット ---
    void setRootPath(const QString& path);
    void goHome();

signals:
    // --- MainWindow (UI) に状態変化を通知するシグナル ---
    void navigationStateChanged(bool canGoBack, bool canGoForward, bool canGoUp);
    void currentPathChanged(const QString& path);

    // --- MainWindow (ロジック) にファイル操作を通知するシグナル ---
    void fileActivated(const QString& filePath); // ファイルがダブルクリックされた
    void fileSelected(const QString& filePath);  // ファイルが選択された

private:
    void addPathToHistory(const QString& path);
    QString getParentPath(const QString& path) const;

    // --- データメンバ ---
    QTreeView* m_treeView;
    QFileSystemModel* m_model;

    QString m_currentLocation;
    QStringList m_history;
    int m_historyIndex;
};

#endif // NAVIGATIONMANAGER_H
