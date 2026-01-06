#include "foldertreewidget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QDir>
#include <QApplication>

FolderTreeWidget::FolderTreeWidget(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_treeView = new QTreeView(this);
    m_treeView->setDragEnabled(true); // D&D有効化
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    // モデルの初期化
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setRootPath(QDir::rootPath());
    m_fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    m_treeView->setModel(m_fileModel);

    // カラム設定 (名前以外は隠す)
    m_treeView->header()->setVisible(false);
    for (int i = 1; i < m_fileModel->columnCount(); ++i) {
        m_treeView->hideColumn(i);
    }

    layout->addWidget(m_treeView);

    // シグナル接続
    connect(m_treeView, &QTreeView::doubleClicked, this, &FolderTreeWidget::onDoubleClicked);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &FolderTreeWidget::onCustomContextMenuRequested);

    // 選択変更監視 (必要であれば)
    // connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged, ...);
}

FolderTreeWidget::~FolderTreeWidget()
{
}

void FolderTreeWidget::setFilterExtensions(const QStringList &audio, const QStringList &video,
                                           const QStringList &image, const QStringList &playlist)
{
    m_allExtensions = audio + video + image + playlist;
    QStringList filters;
    for (const QString &ext : m_allExtensions) {
        filters << "*." + ext;
    }
    m_fileModel->setNameFilters(filters);
    m_fileModel->setNameFilterDisables(false); // フィルタ外のファイルを非表示にする
}

// --- 操作 ---

void FolderTreeWidget::goUp()
{
    QDir currentDir(m_fileModel->rootPath());
    if (currentDir.cdUp()) {
        setRootPath(currentDir.path());
    }
}

void FolderTreeWidget::goHome()
{
    setRootPath(QDir::homePath());
}

void FolderTreeWidget::setRootPath(const QString &path)
{
    m_treeView->setRootIndex(m_fileModel->index(path));
    // モデル自体のルートパスは変更しない（パフォーマンスのため）
    // ビューのルートインデックスを変更する方式をとる
}

void FolderTreeWidget::navigateTo(const QString &path)
{
    QModelIndex index = m_fileModel->index(path);
    if (index.isValid()) {
        m_treeView->scrollTo(index);
        m_treeView->setCurrentIndex(index);
        m_treeView->expand(index);
    }
}

QString FolderTreeWidget::currentPath() const
{
    QModelIndex index = m_treeView->currentIndex();
    if (index.isValid()) {
        return m_fileModel->filePath(index);
    }
    return m_fileModel->filePath(m_treeView->rootIndex());
}

// --- スロット ---

void FolderTreeWidget::onDoubleClicked(const QModelIndex &index)
{
    QString path = m_fileModel->filePath(index);
    QFileInfo fi(path);

    if (fi.isDir()) {
        emit folderActivated(path);
    } else {
        emit fileActivated(path);
    }
}

void FolderTreeWidget::onCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = m_treeView->indexAt(pos);
    QMenu contextMenu(this);

    QString clickedPath;
    QString clickedName;

    if (index.isValid()) {
        clickedPath = m_fileModel->filePath(index);
        clickedName = m_fileModel->fileName(index);

        QFileInfo fi(clickedPath);

        if (fi.isDir()) {
            // フォルダ用メニュー
            contextMenu.addAction(QString("「%1」をプレイリストに追加").arg(clickedName),
                                  [this, clickedPath](){ emit requestAddToPlaylist(clickedPath); });

            contextMenu.addAction(QString("「%1」をスライドショーに追加").arg(clickedName),
                                  [this, clickedPath](){ emit requestAddToSlideshow(clickedPath); });

            contextMenu.addAction(QString("「%1」を本棚で開く").arg(clickedName),
                                  [this, clickedPath](){ emit requestOpenInBookshelf(clickedPath); });

            contextMenu.addAction(QString("「%1」をルートに設定").arg(clickedName),
                                  [this, clickedPath](){ setRootPath(clickedPath); }); // 内部処理

            contextMenu.addSeparator();
        } else {
            // ファイル用メニュー
            contextMenu.addAction(QString("「%1」をプレイリストに追加").arg(clickedName),
                                  [this, clickedPath](){ emit requestAddToPlaylist(clickedPath); });

            // 画像ならスライドショー追加なども可能
            QString suffix = fi.suffix().toLower();
            // 簡易判定: 拡張子リストを持っているならチェックしても良い
            contextMenu.addSeparator();
        }
    }

    // --- 共通メニュー ---

    // 上へ移動
    QDir currentRoot(m_fileModel->filePath(m_treeView->rootIndex()));
    if (currentRoot.cdUp()) {
        contextMenu.addAction("上の階層へ移動", this, &FolderTreeWidget::goUp);
    }

    contextMenu.addAction("ホームディレクトリへ移動", this, &FolderTreeWidget::goHome);
    contextMenu.addSeparator();

    // ルート選択
    contextMenu.addAction("表示するフォルダを選択...", [this](){
        QString current = m_fileModel->filePath(m_treeView->rootIndex());
        QString dir = QFileDialog::getExistingDirectory(this, "フォルダを選択", current);
        if (!dir.isEmpty()) setRootPath(dir);
    });

    // ドライブ一覧
    QMenu *driveMenu = contextMenu.addMenu("ドライブ/ボリュームの切り替え");
    for (const QFileInfo &drive : QDir::drives()) {
        driveMenu->addAction(drive.filePath(), [this, drive](){
            setRootPath(drive.filePath());
        });
    }

    contextMenu.exec(m_treeView->viewport()->mapToGlobal(pos));
}
