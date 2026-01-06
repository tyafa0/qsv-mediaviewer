#include "bookshelfwidget.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QtConcurrent/qtconcurrentrun.h>
#include <QImageReader>
#include <QFileInfo>
#include <QDateTime>
#include <QRandomGenerator>

BookshelfWidget::BookshelfWidget(QWidget *parent)
    : QWidget(parent)
    , m_thumbnailsVisible(true)
    , m_showImages(false)
    , m_syncDateFont(true)
    , m_currentFontSize(10)
    , m_currentSortMode(SortName) // ★ デフォルトは名前順
    , m_sortAscending(true)
{
    // レイアウトとリストウィジェットの初期化
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setDragDropMode(QAbstractItemView::NoDragDrop);
    layout->addWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &BookshelfWidget::onItemDoubleClicked);

    // サムネイルキャッシュの設定 (例: 最大100MB)
    m_thumbnailCache.setMaxCost(1024 * 100);

    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
}

BookshelfWidget::~BookshelfWidget()
{
    // 実行中のウォッチャーがあればキャンセル待機などをここで行う
    for(auto watcher : m_activeWatchers) {
        if(!watcher->isFinished()) watcher->waitForFinished();
        delete watcher;
    }
}

void BookshelfWidget::navigateToPath(const QString &path)
{
    // ▼▼▼ 修正: ファイルパスが来てもディレクトリとして扱う ▼▼▼
    QFileInfo fi(path);
    QString dirPath = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();

    QDir dir(dirPath);
    if (!dir.exists()) return;

    // ▼▼▼ 追加: 既に同じディレクトリにいるならリロードしない（負荷対策） ▼▼▼
    // ※ただし、特定の画像を選択状態にしたい場合は、ここで selection の更新処理だけ行う手もあります
    if (m_currentPath == dir.absolutePath()) {
        return;
    }

    // 前のパスのスクロール位置を保存
    if (!m_currentPath.isEmpty()) {
        m_scrollHistory[m_currentPath] = m_listWidget->verticalScrollBar()->value();
    }

    m_currentPath = dir.absolutePath(); // absolutePath() で正規化しておくのが無難
    updateView();

    emit directoryChanged(m_currentPath);
}

void BookshelfWidget::setIcons(const QMap<QString, QIcon> &icons)
{
    m_icons = icons;
    updateView(); // 再描画
}

void BookshelfWidget::setImageExtensions(const QStringList &extensions)
{
    m_imageExtensions = extensions;
}

void BookshelfWidget::refresh()
{
    updateView();
}

QString BookshelfWidget::currentPath() const
{
    return m_currentPath;
}

// --- Public Slots (UI Options) ---

void BookshelfWidget::setFontSize(int size)
{
    m_currentFontSize = size;
    // リスト全体のフォント設定
    QFont f = m_listWidget->font();
    f.setPointSize(size);
    m_listWidget->setFont(f);

    // 各アイテムウィジェットの更新が必要ならここで行う
    // (簡易的に再描画を呼ぶか、itemsをループして更新する)
    updateView();
}

void BookshelfWidget::setThumbnailsVisible(bool visible)
{
    if (m_thumbnailsVisible == visible) return;
    m_thumbnailsVisible = visible;
    updateView();
}

void BookshelfWidget::setShowImages(bool show)
{
    if (m_showImages == show) return;
    m_showImages = show;
    updateView();
}

void BookshelfWidget::setSortOrder(SortMode mode, bool ascending)
{
    if (m_currentSortMode == mode && m_sortAscending == ascending && mode != SortShuffle) {
        return;
    }
    m_currentSortMode = mode;
    m_sortAscending = ascending;
    updateView(); // 設定変更後にリストを更新
}

void BookshelfWidget::setSyncDateFont(bool sync)
{
    m_syncDateFont = sync;
    updateView();
}

void BookshelfWidget::sortFileInfos(QFileInfoList &list)
{
    if (m_currentSortMode == SortShuffle) {
        // シャッフル (C++17以降なら std::shuffle も可ですが、Qtでの簡易実装)
        auto *generator = QRandomGenerator::global();
        // 単純なランダムスワップ
        for (int i = 0; i < list.size(); ++i) {
            int j = generator->bounded(list.size());
            list.swapItemsAt(i, j);
        }
        return; // シャッフルの場合、昇順・降順は無視
    }

    // std::sort とラムダ式を使ってソート
    std::sort(list.begin(), list.end(), [this](const QFileInfo &a, const QFileInfo &b) -> bool {
        bool result = true;

        switch (m_currentSortMode) {
        case SortName:
            // QCollatorを使って自然順比較 (1.png < 2.png < 10.png)
            result = (m_collator.compare(a.fileName(), b.fileName()) < 0);
            break;

        case SortDate:
            // 更新日時で比較
            result = (a.lastModified() < b.lastModified());
            break;

        case SortSize:
            // ファイルサイズで比較
            result = (a.size() < b.size());
            break;

        default:
            break;
        }

        // 降順なら結果を反転
        return m_sortAscending ? result : !result;
    });
}

// --- Private Logic ---

void BookshelfWidget::updateView()
{
    m_listWidget->clear();
    QDir dir(m_currentPath);

    // 1. 親ディレクトリ (..) - ソート対象外で常に先頭
    if (dir.cdUp()) {
        QWidget* widget = createItemWidget("..", dir.path(), getIcon("arrow_upward"), false);
        QListWidgetItem* item = new QListWidgetItem(m_listWidget);
        item->setData(Qt::UserRole, dir.path());
        item->setData(Qt::UserRole + 1, false);
        item->setSizeHint(widget->sizeHint());
        m_listWidget->setItemWidget(item, widget);
        dir.cd(m_currentPath);
    }

    // ★ 変更: QFileInfoListを取得してソートする方式に変更

    // 2. サブディレクトリ
    // ディレクトリは常に名前順が良い場合が多いですが、設定に従うなら以下のようにします
    QFileInfoList subDirInfos = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    sortFileInfos(subDirInfos); // ★ ソート実行

    QIcon folderIcon = getIcon("no_image");

    for (const QFileInfo& info : subDirInfos) {
        QString fullPath = info.absoluteFilePath();
        // 名前表示用 (フォルダ名はそのまま)
        QWidget* widget = createItemWidget(info.fileName(), fullPath, folderIcon, false);

        QListWidgetItem* item = new QListWidgetItem(m_listWidget);
        item->setData(Qt::UserRole, fullPath);
        item->setData(Qt::UserRole + 1, false);
        item->setSizeHint(widget->sizeHint());
        m_listWidget->setItemWidget(item, widget);
    }

    // 3. 画像ファイル
    if (m_showImages) {
        QStringList filters;
        for(const QString &ext : m_imageExtensions) filters << "*." + ext;

        // ★ ファイル情報を取得
        QFileInfoList fileInfos = dir.entryInfoList(filters, QDir::Files);
        sortFileInfos(fileInfos); // ★ ソート実行 (ここで自然順ソートなどが適用される)

        QIcon imgIcon = getIcon("image");

        for (const QFileInfo& info : fileInfos) {
            QString fullPath = info.absoluteFilePath();
            QWidget* widget = createItemWidget(info.fileName(), fullPath, imgIcon, true);

            QListWidgetItem* item = new QListWidgetItem(m_listWidget);
            item->setData(Qt::UserRole, fullPath);
            item->setData(Qt::UserRole + 1, true);
            item->setSizeHint(widget->sizeHint());
            m_listWidget->setItemWidget(item, widget);
        }
    }

    // スクロール位置の復元
    if (m_scrollHistory.contains(m_currentPath)) {
        m_listWidget->verticalScrollBar()->setValue(m_scrollHistory.value(m_currentPath));
    } else {
        m_listWidget->verticalScrollBar()->setValue(0);
    }
}

QWidget* BookshelfWidget::createItemWidget(const QString &name, const QString &path, const QIcon &defaultIcon, bool isImageFile)
{
    QWidget* widget = new QWidget();

    // アイコン
    QLabel* iconLabel = new QLabel(widget);
    iconLabel->setObjectName("iconLabel"); // 後で検索するために名前を付ける
    iconLabel->setVisible(m_thumbnailsVisible);
    QSize iconSize(64, 64);
    iconLabel->setFixedSize(iconSize);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(defaultIcon.pixmap(iconSize));

    // テキスト
    QLabel* nameLabel = new QLabel(name, widget);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(m_currentFontSize);
    nameLabel->setFont(nameFont);

    QLabel* dateLabel = new QLabel(widget);
    QFont dateFont = dateLabel->font();
    if (m_syncDateFont) {
        dateFont.setPointSize(qMax(8, m_currentFontSize - 2));
    }
    dateLabel->setFont(dateFont);

    if (!path.isEmpty() && name != "..") {
        QFileInfo fi(path);
        dateLabel->setText(fi.lastModified().toString("yyyy/MM/dd hh:mm"));
        // サムネイル読み込み開始
        if (m_thumbnailsVisible) {
            loadThumbnailAsync(path, iconSize, isImageFile);
        }
    }

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(dateLabel);
    textLayout->setSpacing(0);
    textLayout->setContentsMargins(m_thumbnailsVisible ? 5 : 0, 0, 0, 0);

    QHBoxLayout* mainLayout = new QHBoxLayout(widget);
    mainLayout->addWidget(iconLabel);
    mainLayout->addLayout(textLayout);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    widget->setLayout(mainLayout);

    return widget;
}

void BookshelfWidget::loadThumbnailAsync(const QString &path, const QSize &size, bool isImageFile)
{
    // 非同期処理 (MainWindowの実装を移植)
    // 実際の実装では、ここで QtConcurrent::run を呼び出し、
    // 完了シグナルで onThumbnailLoaded を呼んで iconLabel に setPixmap する
    // (詳細は元の mainwindow.cpp の loadThumbnailAsync を参照し、
    //  findChild<QLabel*>("iconLabel") を使用して更新する)

    auto future = QtConcurrent::run([this, path, size, isImageFile]() -> QPixmap {
        QString targetPath = isImageFile ? path : findFirstImageIn(path);
        if (targetPath.isEmpty()) return QPixmap();

        // キャッシュ確認 (スレッドセーフに行う)
        QMutexLocker locker(&m_thumbnailCacheMutex);
        if (auto cached = m_thumbnailCache.object(targetPath)) {
            return *cached;
        }
        locker.unlock();

        QImageReader reader(targetPath);
        if (!reader.canRead()) return QPixmap();

        reader.setScaledSize(reader.size().scaled(size, Qt::KeepAspectRatio));
        QImage img = reader.read();
        if (img.isNull()) return QPixmap();

        QPixmap pix = QPixmap::fromImage(img);

        locker.relock();
        m_thumbnailCache.insert(targetPath, new QPixmap(pix));
        return pix;
    });

    auto watcher = new QFutureWatcher<QPixmap>(this);
    connect(watcher, &QFutureWatcher<QPixmap>::finished, this, [this, watcher, path, size](){
        QPixmap res = watcher->result();
        if (!res.isNull()) {
            // リスト内を検索して該当アイテムのアイコンを更新
            // (本来は ItemWidget 側で受ける設計の方が良いが、現状のロジックを維持)
            for(int i=0; i<m_listWidget->count(); ++i) {
                QListWidgetItem* item = m_listWidget->item(i);
                if (item->data(Qt::UserRole).toString() == path) {
                    QWidget* w = m_listWidget->itemWidget(item);
                    if (w) {
                        QLabel* icon = w->findChild<QLabel*>("iconLabel");
                        if (icon) icon->setPixmap(res.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    }
                    break;
                }
            }
        }
        watcher->deleteLater();
        m_activeWatchers.removeAll(watcher);
    });
    m_activeWatchers.append(watcher);
    watcher->setFuture(future);
}

QString BookshelfWidget::findFirstImageIn(const QString &dirPath)
{
    QDir dir(dirPath);
    QStringList filters;
    for(const QString &ext : m_imageExtensions) filters << "*." + ext;
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
    return files.isEmpty() ? QString() : dir.filePath(files.first());
}

void BookshelfWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    bool isImage = item->data(Qt::UserRole + 1).toBool();

    if (isImage) {
        // 画像なら選択通知 -> MainWindowがMediaViewで表示
        emit imageFileSelected(path);
    } else {
        // フォルダなら移動
        if (path == "..") {
            // navigateToPath内で処理される
        }
        navigateToPath(path);
    }
}

QIcon BookshelfWidget::getIcon(const QString &name) const
{
    // マップに登録されていればそれを返し、なければ空のアイコンを返す
    return m_icons.value(name, QIcon());
}
