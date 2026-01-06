#include "slideshowwidget.h"
#include "mediaitemdelegate.h"

#include <QVBoxLayout>
#include <QTabBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>

SlideshowWidget::SlideshowWidget(QWidget *parent)
    : QWidget(parent)
    , m_asyncOperations(0)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(false); // コンテキストメニューで制御
    m_tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addWidget(m_tabWidget);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &SlideshowWidget::onCurrentChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &SlideshowWidget::onTabCloseRequested);
    connect(m_tabWidget->tabBar(), &QTabBar::tabBarDoubleClicked, this, &SlideshowWidget::onTabDoubleClicked);
    connect(m_tabWidget->tabBar(), &QTabBar::customContextMenuRequested, this, &SlideshowWidget::onTabContextMenu);

    // 初期タブを1つ作成
    addNewTab();
}

SlideshowWidget::~SlideshowWidget()
{
}

void SlideshowWidget::setImageExtensions(const QStringList &extensions)
{
    m_imageExtensions = extensions;
}

// --- Public Operations ---

void SlideshowWidget::addNewTab()
{
    QListWidget* list = createListWidget();
    int index = m_tabWidget->addTab(list, QString("Slideshow%1").arg(m_tabWidget->count()));
    m_tabWidget->setCurrentIndex(index);
}

void SlideshowWidget::closeTab(int index)
{
    if (m_tabWidget->count() <= 1) return;

    QWidget* page = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    delete page;
}

void SlideshowWidget::renameTab(int index)
{
    if (index < 0) return;
    bool ok;
    QString text = QInputDialog::getText(this, "名前変更", "新しい名前:", QLineEdit::Normal, m_tabWidget->tabText(index), &ok);
    if (ok && !text.isEmpty()) {
        m_tabWidget->setTabText(index, text);
    }
}

void SlideshowWidget::saveCurrentList()
{
    QListWidget* list = currentListWidget();
    if (!list || list->count() == 0) return;

    QString filePath = QFileDialog::getSaveFileName(this, "リストを保存", QDir::homePath(), "Slideshow List (*.qsl)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "#QSLIDESHOWLIST\n";
    for(int i=0; i<list->count(); ++i) {
        out << list->item(i)->data(Qt::UserRole).toString() << "\n";
    }
}

void SlideshowWidget::loadList()
{
    QString filePath = QFileDialog::getOpenFileName(this, "リストを読み込む", QDir::homePath(), "Slideshow List (*.qsl)");
    if (!filePath.isEmpty()) {
        loadListFromFile(filePath);
    }
}

void SlideshowWidget::loadListFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    if (in.readLine() != "#QSLIDESHOWLIST") return;

    QStringList files;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) files.append(line);
    }

    if (files.isEmpty()) return;

    // 追加か上書きか確認
    QListWidget* list = currentListWidget();
    bool overwrite = false;
    if (list->count() > 0) {
        auto btn = QMessageBox::question(this, "確認", "現在のリストを上書きしますか？\n(Noで追加)", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (btn == QMessageBox::Cancel) return;
        if (btn == QMessageBox::Yes) overwrite = true;
    }

    if (overwrite) list->clear();
    addFilesToCurrentList(files);
}

QString SlideshowWidget::tabText(int index) const
{
    return m_tabWidget->tabText(index);
}

void SlideshowWidget::addFilesToCurrentList(const QStringList &files)
{
    QListWidget* list = currentListWidget();
    if (!list || files.isEmpty()) return;

    bool wasEmpty = (list->count() == 0);

    m_asyncOperations++;
    emit fileLoadStarted(); // ローディング開始通知

    // チャンク処理開始
    processAddFilesChunk(files, 0, wasEmpty);
}

void SlideshowWidget::processAddFilesChunk(const QStringList &files, int startIndex, bool wasInitiallyEmpty)
{
    const int chunkSize = 50;
    QListWidget* currentList = currentListWidget();

    if (!currentList) {
        m_asyncOperations--;
        emit fileLoadFinished();
        return;
    }

    int endIndex = qMin(startIndex + chunkSize, files.size());

    for (int i = startIndex; i < endIndex; ++i) {
        const QString& filePath = files.at(i);
        QFileInfo fi(filePath);
        if (!fi.exists() || !fi.isFile()) {
            continue; // 存在しない、またはファイルでない場合はスキップ
        }

        QString fileName = fi.fileName();
        QListWidgetItem* item = new QListWidgetItem(fileName);
        item->setData(Qt::UserRole, filePath);
        item->setData(Qt::UserRole + 1, false);
        item->setToolTip(fileName);
        currentList->addItem(item);
    }

    if (endIndex < files.size()) {
        QTimer::singleShot(0, this, [=](){ processAddFilesChunk(files, endIndex, wasInitiallyEmpty); });
    } else {
        // 完了
        m_asyncOperations--;
        if (m_asyncOperations == 0) {
            emit fileLoadFinished();
        }

        emit filesAddedToPlaylist(wasInitiallyEmpty);
    }
}

// --- Accessors ---

int SlideshowWidget::count() const { return m_tabWidget->count(); }
int SlideshowWidget::currentIndex() const { return m_tabWidget->currentIndex(); }
QWidget* SlideshowWidget::currentWidget() const {
    return m_tabWidget->currentWidget();
}
QListWidget* SlideshowWidget::currentListWidget() const {
    return qobject_cast<QListWidget*>(m_tabWidget->currentWidget());
}
QList<QListWidget*> SlideshowWidget::allListWidgets() const {
    QList<QListWidget*> lists;
    for(int i=0; i<m_tabWidget->count(); ++i) {
        if(auto l = qobject_cast<QListWidget*>(m_tabWidget->widget(i))) lists << l;
    }
    return lists;
}

// --- Slots ---

void SlideshowWidget::applyUiSettings(int fontSize, bool reorderEnabled)
{
    for(auto list : allListWidgets()) {
        QFont f = list->font();
        f.setPointSize(fontSize);
        list->setFont(f);
        list->setDragDropMode(reorderEnabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
    }
}

void SlideshowWidget::onTabCloseRequested(int index)
{
    closeTab(index);
}

void SlideshowWidget::onTabDoubleClicked(int index)
{
    renameTab(index);
}

void SlideshowWidget::onCurrentChanged(int index)
{
    Q_UNUSED(index);
    emit currentListChanged(currentListWidget());
}

void SlideshowWidget::onListContextMenu(const QPoint &pos)
{
    QListWidget* list = qobject_cast<QListWidget*>(sender());
    if (!list) return;

    QMenu menu(this);
    menu.addAction("ファイルを開く...", this, &SlideshowWidget::requestFileOpen);
    menu.addAction("フォルダを開く...", this, &SlideshowWidget::requestFolderOpen);
    menu.addSeparator();
    menu.addAction("リスト保存...", this, &SlideshowWidget::saveCurrentList);
    menu.addAction("リスト読込...", this, &SlideshowWidget::loadList);

    auto selected = list->selectedItems();
    if (!selected.isEmpty()) {
        menu.addSeparator();
        menu.addAction("選択項目を削除", [this, list](){
            qDeleteAll(list->selectedItems());
            // 必要ならデータ変更通知
            // emit listDataChanged();
        });
    }
    if (list->count() > 0) {
        menu.addAction("すべて削除", [this, list](){
            list->clear();
            // emit listDataChanged();
        });
    }

    menu.exec(list->mapToGlobal(pos));
}

void SlideshowWidget::onTabContextMenu(const QPoint &pos)
{
    int index = m_tabWidget->tabBar()->tabAt(pos);
    QMenu menu(this);

    if (index >= 0) {
        menu.addAction("名前変更...", [this, index](){ renameTab(index); });
        QAction* closeAct = menu.addAction("閉じる", [this, index](){ closeTab(index); });
        if (m_tabWidget->count() <= 1) closeAct->setEnabled(false);
        menu.addSeparator();
    }

    menu.addAction("新しいリストを追加", this, &SlideshowWidget::addNewTab);
    menu.addAction("ファイルを開く...", this, &SlideshowWidget::requestFileOpen);
    menu.addAction("フォルダを開く...", this, &SlideshowWidget::requestFolderOpen);

    menu.exec(m_tabWidget->tabBar()->mapToGlobal(pos));
}

// 内部リストの生成
QListWidget* SlideshowWidget::createListWidget()
{
    QListWidget* list = new QListWidget(this);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setAcceptDrops(true);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    list->setItemDelegate(new MediaItemDelegate(this));

    setupListConnections(list);
    return list;
}

void SlideshowWidget::setupListConnections(QListWidget* list)
{
    // 修正: 内部シグナルを外部シグナルへ中継
    connect(list, &QListWidget::customContextMenuRequested, this, &SlideshowWidget::onListContextMenu);
    connect(list, &QListWidget::itemDoubleClicked, this, &SlideshowWidget::itemDoubleClicked);

    // 選択変更やデータ変更はImageViewControllerが直接モデルを監視するか、ここで中継する
    // 現状の設計では、ImageViewControllerが currentListChanged でリストを受け取り、そこで接続しているため
    // ここでの接続は最低限で良い
}
