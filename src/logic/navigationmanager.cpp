#include "navigationmanager.h"
#include <QDir>
#include <QDebug>

NavigationManager::NavigationManager(QTreeView* treeView, QFileSystemModel* model, QObject *parent)
    : QObject(parent)
    , m_treeView(treeView)
    , m_model(model)
    , m_historyIndex(-1)
{
    if (!m_treeView || !m_model) {
        qWarning() << "NavigationManager: TreeView or Model is null!";
        return;
    }

    // 初期位置をホームに設定
    m_currentLocation = m_model->rootPath();
    m_treeView->setRootIndex(m_model->index(m_currentLocation));
}

NavigationManager::~NavigationManager()
{
}

void NavigationManager::navigateTo(const QString& location, bool addToHistory)
{
    if (location.isEmpty() || !QDir(location).exists()) return;

    m_currentLocation = location;
    m_treeView->setRootIndex(m_model->index(location)); // TreeViewのルートを変更

    if (addToHistory) {
        addPathToHistory(location);
    }

    // UIに状態変更を通知
    emit currentPathChanged(m_currentLocation);
    emit navigationStateChanged(m_historyIndex > 0,
                                m_historyIndex < m_history.size() - 1,
                                !getParentPath(m_currentLocation).isEmpty());
}

void NavigationManager::goBack()
{
    if (m_historyIndex > 0) {
        m_historyIndex--;
        navigateTo(m_history.at(m_historyIndex), false); // 履歴移動時は履歴に追加しない
    }
}

void NavigationManager::goForward()
{
    if (m_historyIndex < m_history.size() - 1) {
        m_historyIndex++;
        navigateTo(m_history.at(m_historyIndex), false);
    }
}

void NavigationManager::goUp()
{
    QString parentPath = getParentPath(m_currentLocation);
    if (!parentPath.isEmpty()) {
        navigateTo(parentPath, true); // 「上へ」は履歴に追加
    }
}

void NavigationManager::onTreeDoubleClicked(const QModelIndex &index)
{
    QString filePath = m_model->filePath(index);
    emit fileActivated(filePath);
}

void NavigationManager::onTreeSelectionChanged(const QModelIndex &current)
{
    QString filePath = m_model->filePath(current);

    // 現在の場所を更新 (履歴には追加しない)
    if (m_model->isDir(current)) {
        m_currentLocation = filePath;
        emit navigationStateChanged(m_historyIndex > 0,
                                    m_historyIndex < m_history.size() - 1,
                                    !getParentPath(m_currentLocation).isEmpty());
    } else {
        emit fileSelected(filePath); // ファイルが選択されたことを通知
    }
}

void NavigationManager::setRootPath(const QString& path)
{
    // メニューなどからルートパスが変更された場合
    navigateTo(path, true);
}

void NavigationManager::goHome()
{
    navigateTo(QDir::homePath(), true);
}

void NavigationManager::addPathToHistory(const QString& path)
{
    if (m_history.value(m_historyIndex) == path) return; // 同じパスは追加しない

    // 現在の位置より後ろにある履歴（「進む」の履歴）を削除
    if (m_historyIndex < m_history.size() - 1) {
        m_history = m_history.mid(0, m_historyIndex + 1);
    }

    m_history.append(path);
    m_historyIndex = m_history.size() - 1;
}

QString NavigationManager::getParentPath(const QString& path) const
{
    QDir dir(path);
    if (dir.cdUp()) {
        return dir.path();
    }
    return QString(); // ルートディレクトリなど
}
