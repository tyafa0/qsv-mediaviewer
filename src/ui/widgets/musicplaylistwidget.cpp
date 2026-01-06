#include "musicplaylistwidget.h"
#include "mediaitemdelegate.h"

#include <QVBoxLayout>
#include <QTabBar>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDebug>

MusicPlaylistWidget::MusicPlaylistWidget(PlaylistManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_playlistManager(manager)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(false); // 閉じるボタンは自前メニューで制御
    m_tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addWidget(m_tabWidget);

    // シグナル接続
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MusicPlaylistWidget::onTabChanged);
    connect(m_tabWidget->tabBar(), &QTabBar::tabBarDoubleClicked, this, &MusicPlaylistWidget::onTabDoubleClicked);
    connect(m_tabWidget->tabBar(), &QTabBar::customContextMenuRequested, this, &MusicPlaylistWidget::onTabContextMenu);

    // 初期タブの生成 (PlaylistManagerの初期状態に合わせる想定だが、
    // ここではMainWindow側で既に初期化されているManagerを受け取る前提で、初期タブ生成はMainWindowのループで行うか、
    // あるいはここでManagerの数に合わせる。
    // 今回は「MainWindowがManagerを初期化直後」と仮定し、UIを空から構築するロジックにする)

    // ただし、MainWindowの初期化フローで既に manager->addPlaylist() が呼ばれている可能性があるため、
    // Managerと同期をとるのが理想。
    // 簡易的に、最初は空として MainWindow から addPlaylist を呼んでもらう形をとる。
}

MusicPlaylistWidget::~MusicPlaylistWidget()
{
}

// --- Public Methods ---

void MusicPlaylistWidget::addPlaylist()
{
    // 1. Managerに行を追加
    m_playlistManager->addPlaylist();

    // 2. UIを作成
    QListWidget* list = createListWidget();
    m_listWidgets.append(list);

    int newIndex = m_tabWidget->addTab(list, QString("Playlist%1").arg(m_tabWidget->count()));
    m_tabWidget->setCurrentIndex(newIndex);

    emit playlistCountChanged();
}

void MusicPlaylistWidget::removePlaylist(int index)
{
    if (m_tabWidget->count() <= 1 || index < 0 || index >= m_tabWidget->count()) return;

    // 削除対象のリスト
    // QListWidget* list = m_listWidgets[index];

    // Managerから削除
    m_playlistManager->removePlaylist(index);

    // UIリストから削除
    m_listWidgets.removeAt(index);

    // タブ削除
    QWidget* page = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    delete page; // メモリ解放

    emit playlistCountChanged();
    emit previewUpdateRequested(); // プレビュー更新依頼
}

void MusicPlaylistWidget::saveCurrentPlaylist()
{
    int index = m_tabWidget->currentIndex();
    if (index < 0) return;

    QString filePath = QFileDialog::getSaveFileName(this, "プレイリストを保存", QDir::homePath(), "SupportViewer Playlist (*.qpl)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    QStringList files = m_playlistManager->getPlaylist(index);
    for (const QString& path : files) {
        out << path << "\n";
    }
}

void MusicPlaylistWidget::loadPlaylist()
{
    QString filePath = QFileDialog::getOpenFileName(this, "プレイリストを読み込む", QDir::homePath(), "Playlist (*.qpl)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString header = in.readLine(); // ヘッダー読み込み (もしあれば)
    // ※ #QPLAYLIST などのヘッダーチェックをしている場合はここに記述

    QStringList files;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // ★ 追加: ファイルが存在するかチェック
        QFileInfo fi(line);
        if (fi.exists() && fi.isFile()) {
            files.append(line);
        }
    }

    if (!files.isEmpty()) {
        // 現在のタブに追加、または新規タブで開くなどの処理
        m_playlistManager->addFilesToPlaylist(files, m_tabWidget->currentIndex());
    }
}

void MusicPlaylistWidget::deleteSelectedItems()
{
    // 現在表示中のリストを取得
    QListWidget* list = currentListWidget();
    if (!list) return;

    // 現在のプレイリストインデックスを取得
    int index = currentIndex();
    if (index < 0) return;

    // 選択されているアイテムを取得
    auto selected = list->selectedItems();
    if (selected.isEmpty()) return;

    // ★追加: 削除対象に再生中のアイテムが含まれているかチェックし、あれば停止要求を出す
    bool isPlayingItemInvolved = false;
    for (auto item : selected) {
        // MediaItemDelegate::IsActiveRole (Qt::UserRole + 1) が true なら再生中
        if (item->data(MediaItemDelegate::IsActiveRole).toBool()) {
            isPlayingItemInvolved = true;
            break;
        }
    }

    if (isPlayingItemInvolved) {
        emit stopRequest();
    }

    // 削除する行番号のリストを作成
    QList<int> rows;
    for(auto item : selected) {
        rows.append(list->row(item));
    }

    // インデックスがずれないように降順（後ろから）ソート
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    // データ管理クラスに削除を依頼
    m_playlistManager->removeIndices(index, rows);
}

// --- Getters / Setters ---

int MusicPlaylistWidget::count() const { return m_tabWidget->count(); }
int MusicPlaylistWidget::currentIndex() const { return m_tabWidget->currentIndex(); }
void MusicPlaylistWidget::setCurrentIndex(int index) { m_tabWidget->setCurrentIndex(index); }
QListWidget* MusicPlaylistWidget::currentListWidget() const {
    return m_listWidgets.value(m_tabWidget->currentIndex(), nullptr);
}
QListWidget* MusicPlaylistWidget::listWidget(int index) const {
    return m_listWidgets.value(index, nullptr);
}
QString MusicPlaylistWidget::tabText(int index) const { return m_tabWidget->tabText(index); }
void MusicPlaylistWidget::setTabText(int index, const QString &text) { m_tabWidget->setTabText(index, text); }
QList<QListWidget*> MusicPlaylistWidget::allListWidgets() const { return m_listWidgets; }

// --- Internal Logic ---

QListWidget* MusicPlaylistWidget::createListWidget()
{
    QListWidget* list = new QListWidget(this);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setDragEnabled(true);
    list->setAcceptDrops(true);
    list->setDropIndicatorShown(true);
    list->setDragDropMode(QAbstractItemView::InternalMove); // デフォルト
    list->setItemDelegate(new MediaItemDelegate(this));

    setupListConnections(list);
    return list;
}

void MusicPlaylistWidget::setupListConnections(QListWidget* list)
{
    connect(list, &QListWidget::itemDoubleClicked, this, &MusicPlaylistWidget::onItemDoubleClicked);
    connect(list, &QListWidget::customContextMenuRequested, this, &MusicPlaylistWidget::onListContextMenu);

    // 行移動の同期
    connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this, list](const QModelIndex&, int start, int end, const QModelIndex&, int dest){
        int index = m_listWidgets.indexOf(list);
        if (index != -1) {
            // QListWidgetのInternalMoveは destinationRow が挿入先。
            // 詳細は複雑だが、簡易的にManagerへ通知するインタフェースが必要
            // ここでは MainWindow の実装に合わせて syncPlaylistOrder を呼ぶ想定
            // m_playlistManager->syncPlaylistOrder(index, start, dest);
            // ※ ラムダ内でsender()特定は難しいので変数キャプチャを使用

            // NOTE: QListWidget::rowsMoved の dest は挿入位置。
            // 移動ロジックは PlaylistManager 側で実装されている前提。
            m_playlistManager->syncPlaylistOrder(index, start, dest);
        }
    });
}

void MusicPlaylistWidget::applyUiSettings(int fontSize, bool reorderEnabled)
{
    for (QListWidget* list : m_listWidgets) {
        QFont f = list->font();
        f.setPointSize(fontSize);
        list->setFont(f);
        list->setDragDropMode(reorderEnabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
    }
}

// --- Slots ---

void MusicPlaylistWidget::onTabChanged(int index)
{
    m_playlistManager->setCurrentPlaylistIndex(index);
    emit currentChanged(index);
}

void MusicPlaylistWidget::onTabDoubleClicked(int index)
{
    if (index < 0) return;
    bool ok;
    QString text = QInputDialog::getText(this, "名前変更", "新しい名前:", QLineEdit::Normal, m_tabWidget->tabText(index), &ok);
    if (ok && !text.isEmpty()) {
        m_tabWidget->setTabText(index, text);
        emit tabRenamed(index, text);
    }
}
void MusicPlaylistWidget::onTabContextMenu(const QPoint &pos)
{
    int index = m_tabWidget->tabBar()->tabAt(pos);
    QMenu menu(this);

    if (index >= 0) {
        QString name = tabText(index);

        menu.addAction("名前変更...", [this, index](){ onTabDoubleClicked(index); });
        QAction* closeAct = menu.addAction("削除", [this, index](){ removePlaylist(index); });
        if (m_tabWidget->count() <= 1) closeAct->setEnabled(false);
        menu.addSeparator();

        // ★ 追加: プレイリスト名を明記したアクション
        menu.addAction(QString("「%1」を再生").arg(name), [this, index](){
            emit playRequest(index, 0);
        });
        menu.addSeparator();

        menu.addAction(QString("「%1」にファイルを追加...").arg(name), [this, index](){
            // requestOpenFile シグナルは引数なしなので、ここでは内部ロジックを呼ぶか
            // MainWindow側でindexを受け取るオーバーロードが必要。
            // 既存の requestOpenFile は現在のアクティブなタブに追加するため、
            // 一旦タブを切り替えるか、シグナルを拡張するのが正しいですが、
            // ここでは簡易的に「タブを切り替えてから要求する」方式をとります。
            setCurrentIndex(index);
            emit requestOpenFile();
        });

        menu.addAction(QString("「%1」にフォルダを追加...").arg(name), [this, index](){
            setCurrentIndex(index);
            emit requestOpenFolder();
        });

        menu.addSeparator();
    }

    menu.addAction("新しいプレイリストを追加", this, &MusicPlaylistWidget::addPlaylist);

    // 読み込みはカレントに対して行うため、タブ上ならそのタブをカレントにする
    if (index >= 0) {
        menu.addAction("プレイリストを読み込み...", [this, index](){
            setCurrentIndex(index);
            loadPlaylist();
        });
    }

    // プレビュー設定などを追加するシグナル
    menu.addSeparator();
    emit customizeContextMenu(&menu);

    menu.exec(m_tabWidget->tabBar()->mapToGlobal(pos));
}

void MusicPlaylistWidget::onListContextMenu(const QPoint &pos)
{
    QListWidget* list = qobject_cast<QListWidget*>(sender());
    if (!list) return;
    int index = m_listWidgets.indexOf(list);

    QMenu menu(this);

    // ファイル操作
    menu.addAction("ファイルを開く...", this, &MusicPlaylistWidget::requestOpenFile);
    menu.addAction("フォルダから開く...", this, &MusicPlaylistWidget::requestOpenFolder);
    menu.addSeparator();
    menu.addAction("新しいプレイリストを追加", this, &MusicPlaylistWidget::addPlaylist);

    auto selected = list->selectedItems();
    if (!selected.isEmpty()) {
        menu.addSeparator();
        menu.addAction(QString("選択した%1件を削除").arg(selected.count()), [this, index, list](){
            bool isPlayingItemInvolved = false;
            for (auto item : list->selectedItems()) {
                if (item->data(MediaItemDelegate::IsActiveRole).toBool()) {
                    isPlayingItemInvolved = true;
                    break;
                }
            }
            if (isPlayingItemInvolved) {
                emit stopRequest();
            }

            // 削除ロジック
            QList<int> rows;
            for(auto item : list->selectedItems()) rows.append(list->row(item));
            // 降順ソート
            std::sort(rows.begin(), rows.end(), std::greater<int>());

            m_playlistManager->removeIndices(index, rows);
        });
    }

    if (list->count() > 0) {
        menu.addAction("すべて削除", [this, index](){
            emit stopRequest(); // 全削除は停止推奨
            m_playlistManager->clearPlaylist(index);
        });
    }
    menu.addSeparator();
    emit customizeContextMenu(&menu);

    menu.exec(list->mapToGlobal(pos));
}

void MusicPlaylistWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    QListWidget* list = item->listWidget();
    int listIndex = m_listWidgets.indexOf(list);
    int trackIndex = list->row(item);

    emit playRequest(listIndex, trackIndex);
}
