#include "mainwindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#endif

#include "aboutdialog.h"
#include "bookshelfwidget.h"
#include "filescanner.h"
#include "foldertreewidget.h"
#include "musicplaylistwidget.h"
#include "settingsmanager.h"
#include "thememanager.h"
#include "ui_mainwindow.h"
#include "optionsdialog.h"
#include "mediamanager.h"
#include "playlistmanager.h"
#include "imageviewcontroller.h"
#include "mediaitemdelegate.h"

#include <QtConcurrent/QtConcurrent>
#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QCache>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QImageReader>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMediaMetaData>
#include <QMetaObject>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMutex>
#include <QMimeData>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <QWindow>

#if defined(Q_OS_WIN)
#include <shlobj.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , currentPlaylistIndex(0)
    , isMediaViewFullScreen(false)
    , m_previewsArePinned(false)
    , m_isAlwaysOnTop(false)
    , m_playingPlaylistIndex(-1)
    , m_currentlyPlayingMusicItem(nullptr)
    , m_currentlyDisplayedSlideItem(nullptr)
{
    // setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    // setMouseTracking(true);

    // 1. UIのセットアップ
    ui->setupUi(this);
    setAcceptDrops(true);
    
    // 2. コントロールバーとコンパクトウィンドウの初期化
    // setupCustomTitleBar();
    m_controlBar = new ControlBar(this);
    m_compactWindow = new CompactWindow(this);

    // 3. マネージャクラスのセットアップ
    setupManagers();

    // 4. アイコンの読み込み
    m_themeManager->loadAllIcons();

    // 5. UIコンポーネントの初期設定
    setupUiComponents();
    applyTheme(m_settingsManager->settings().theme);

    // 6. すべてのシグナル/スロットを接続
    setupConnections();

    // 7. すべてのショートカットキーを登録
    setupShortcuts();

    int initialVolume = qMin(m_settingsManager->settings().lastVolume, 100);
    if(m_mediaManager) m_mediaManager->setVolume(initialVolume);

    QTimer::singleShot(0, this, &MainWindow::performInitialUiUpdate);

    // if (ui->menubar) {
    //     ui->menubar->setStyleSheet("QMenuBar { background-color: #2d2d2d; color: white; }");
    // }

// #ifdef Q_OS_WIN
//     // ★重要: OSのネイティブ機能（スナップ、リサイズ、影）を有効化する
//     HWND hwnd = (HWND)this->winId();
//     DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
//     // WS_THICKFRAME (リサイズ機能) と WS_CAPTION (スナップ機能) を追加
//     ::SetWindowLong(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);

//     // DWMで影を描画
//     const MARGINS shadow = { 1, 1, 1, 1 };
//     DwmExtendFrameIntoClientArea(hwnd, &shadow);
// #endif
}

MainWindow::~MainWindow()
{
    delete m_imageViewController;
    delete m_compactWindow;
    delete ui;
}
void MainWindow::setupManagers()
{
    // --- プレイリスト管理 ---
    m_playlistManager = new PlaylistManager(this);

    // --- メディアビューワー管理 ---
    m_imageViewController = new ImageViewController(
        ui->mediaView,
        ui->filenameLineEdit,
        ui->viewControlSlider,
        ui->zoomSpinBox,
        ui->slideshowProgressBar,
        ui->slideshowIntervalSpinBox,
        ui->mediaStackedWidget,
        ui->videoPage,
        this
        );
    ui->mediaView->setScene(m_imageViewController->getScene());

    // --- 再生エンジン ---
    WId wid = ui->videoContainer->winId();
    m_mediaManager = new MediaManager(wid, this);

    // --- ファイルスキャン管理 ---
    m_fileScanner = new FileScanner(this);

    // --- テーマ管理 ---
    m_themeManager = new ThemeManager(this);
    m_themeManager->loadAllIcons();

    // --- 設定管理 ---
    m_settingsManager = new SettingsManager(this);
    m_settingsManager->loadSettings();
}

void MainWindow::setupUiComponents()
{
    // =========================================================
    // 1. データ構造とプレビューコンテナの初期化
    // =========================================================
    // ※ playlistWidgets (QList<QListWidget*>) への追加は廃止しました。
    //   MusicPlaylistWidget が内部で管理します。

    m_previewSlotMapping[0] = -1;
    m_previewSlotMapping[1] = -1;

    previewContainer = ui->previewContainer;
    previewContainer->setContextMenuPolicy(Qt::CustomContextMenu);

    previewGroups.append(ui->previewGroup_0);
    previewGroups.append(ui->previewGroup_1);
    previewGroups.append(ui->previewGroup_2);

    previewLabels.append(ui->previewLabel_0);
    previewLabels.append(ui->previewLabel_1);
    previewLabels.append(ui->previewLabel_2);

    previewListWidgets.append(ui->previewList_0);
    previewListWidgets.append(ui->previewList_1);
    previewListWidgets.append(ui->previewList_2);

    for (QLabel* label : previewLabels) {
        label->installEventFilter(this);
    }

    // =========================================================
    // 2. メディアビューワー (MediaView) の設定
    // =========================================================
    ui->mediaView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->mediaView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    ui->mediaView->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    ui->mediaView->setAlignment(Qt::AlignCenter); // 念のためアライメントも中央に

    ui->mediaView->setRenderHint(QPainter::Antialiasing, true);
    ui->mediaView->setRenderHint(QPainter::SmoothPixmapTransform, true);
    ui->mediaView->setAcceptDrops(true);
    ui->mediaView->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    ui->mediaView->setDragMode(QGraphicsView::ScrollHandDrag);

    ui->mediaView->viewport()->setAcceptDrops(true);
    ui->mediaView->installEventFilter(this);
    ui->mediaView->viewport()->installEventFilter(this);

    ui->filenameLineEdit->installEventFilter(this);
    ui->viewControlSlider->installEventFilter(this);

    QSizePolicy sp = ui->slideshowProgressBar->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    ui->slideshowProgressBar->setSizePolicy(sp);

    // =========================================================
    // 3. ビデオコンテナ (VideoContainer) の設定
    // =========================================================
    ui->videoContainer->installEventFilter(this);

    // ビデオ周りは黒設定を維持
    ui->videoContainer->setStyleSheet("background-color: black;");

    m_videoLoadingLabel = new QLabel("Loading video...", ui->videoContainer);
    m_videoLoadingLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_videoLoadingLabel->font();
    font.setPointSize(24);
    font.setBold(true);
    m_videoLoadingLabel->setFont(font);
    m_videoLoadingLabel->setStyleSheet("background-color: rgba(0, 0, 0, 128); color: white; border-radius: 10px;");
    m_videoLoadingLabel->adjustSize();
    m_videoLoadingLabel->setFixedSize(m_videoLoadingLabel->width() + 40, m_videoLoadingLabel->height() + 20);
    m_videoLoadingLabel->hide();

    m_videoBlackOverlay = new QLabel(ui->videoPage);
    m_videoBlackOverlay->setStyleSheet("background-color: black;");
    m_videoBlackOverlay->hide();
    m_videoBlackOverlay->installEventFilter(this);

    m_mouseInactivityTimer = new QTimer(this);
    m_mouseInactivityTimer->setInterval(2000); // 秒で隠す
    m_mouseInactivityTimer->setSingleShot(true);
    connect(m_mouseInactivityTimer, &QTimer::timeout, this, &MainWindow::hideFullScreenControls);

    // ビデオコンテナのマウス追跡を有効化（クリックしなくてもMoveイベントを拾うため）
    ui->videoContainer->setMouseTracking(true);
    ui->videoPage->setMouseTracking(true);

    // =========================================================
    // 4. フォルダツリー (FolderTree) の設定
    // =========================================================
    m_audioExtensions << "mp3" << "wav" << "ogg" << "flac";
    m_videoExtensions << "mp4" << "mkv" << "avi" << "mov" << "wmv";
    m_imageExtensions = {
        "bmp", "cur", "gif", "icns", "ico", "jfif", "jpeg", "jpg",
        "pbm", "pgm", "png", "ppm", "svg", "svgz", "tga", "tif",
        "tiff", "wbmp", "webp", "xbm", "xpm"
    };
    m_imageViewController->setImageExtensions(m_imageExtensions);
    m_playlistExtensions << "qpl" << "qsl";
    m_allMediaExtensions = m_audioExtensions + m_videoExtensions + m_imageExtensions + m_playlistExtensions;

    // 1. FolderTreeWidgetを作成
    m_folderTreeWidget = new FolderTreeWidget(this);
    // 拡張子フィルタを設定
    m_folderTreeWidget->setFilterExtensions(m_audioExtensions, m_videoExtensions, m_imageExtensions, m_playlistExtensions);

    // 2. ドックに追加
    ui->verticalLayout_13->addWidget(m_folderTreeWidget);

    // =========================================================
    // 5. ドックウィジェット・ツールバーの初期化
    // =========================================================
    m_folderTreeDock = ui->folderTreeDockWidget;
    m_playlistDock = ui->playlistDockWidget;
    m_slideshowDock = ui->slideshowDockWidget;
    m_bookshelfDock = ui->bookshelfDockWidget;

    m_folderTreeDock->hide();
    m_playlistDock->hide();
    m_slideshowDock->hide();
    m_bookshelfDock->hide();

    ui->controlToolBar->addWidget(m_controlBar);

    // 1. MusicPlaylistWidgetを作成 (PlaylistManagerを渡す)
    m_musicPlaylistWidget = new MusicPlaylistWidget(m_playlistManager, this);

    // 2. スプリッター(splitter_2)の先頭に追加
    ui->splitter_2->insertWidget(0, m_musicPlaylistWidget);

    // 3. 保存された設定に基づいてプレイリストを作成
    // PlaylistManagerは空で初期化されるため、ここで必要な数だけ追加する
    int targetCount = m_settingsManager->initialPlaylistCount();
    if (targetCount < 1) targetCount = 1; // 最低1つは確保

    for (int i = 0; i < targetCount; ++i) {
        m_musicPlaylistWidget->addPlaylist();
    }

    if (m_musicPlaylistWidget->count() > 0) {
        m_musicPlaylistWidget->setCurrentIndex(0);
    }

    // 4. ★ 追加: 初期比率を 4:1 に設定
    // QSplitter::setSizes は各ウィジェットのピクセル幅を指定しますが、
    // Qtはこれを比率として扱おうとします。
    // 合計値はウィンドウサイズに依存しますが、相対比率として機能します。
    ui->splitter_2->setSizes(QList<int>{500, 100});

    // タブの履歴初期化
    for(int i = 0; i < m_musicPlaylistWidget->count(); ++i) {
        m_tabActivationHistory.append(i);
    }

    // =========================================================
    // 6. 各リスト用オプションバーの作成
    // =========================================================
    QIcon textSizeIcon = m_themeManager->getIcon("text_size");

    // (A) プレイリスト用
    m_playlistOptions = new ListOptionsWidget(textSizeIcon, this);
    ui->verticalLayout_14->addWidget(m_playlistOptions);

    // (B) スライドショー用
    m_slideshowOptions = new ListOptionsWidget(textSizeIcon, this);

    // (C) フォルダツリー用
    m_folderTreeOptions = new ListOptionsWidget(textSizeIcon, this);
    m_folderTreeOptions->setReorderVisible(false);
    ui->verticalLayout_13->addWidget(m_folderTreeOptions);

    // オプションとMusicPlaylistWidgetの連動設定
    connect(m_playlistOptions, &ListOptionsWidget::fontSizeChanged, this, [this](int size){
        m_musicPlaylistWidget->applyUiSettings(size, m_playlistOptions->isChecked());
        // UI同期設定があれば他のリストにも波及させる
        if (m_settingsManager->settings().syncUiStateAcrossLists) {
            syncAllListsUiState(size, m_playlistOptions->isChecked());
        }
    });
    connect(m_playlistOptions, &ListOptionsWidget::reorderToggled, this, [this](bool checked){
        m_musicPlaylistWidget->applyUiSettings(m_playlistOptions->value(), checked);
        if (m_settingsManager->settings().syncUiStateAcrossLists) {
            syncAllListsUiState(m_playlistOptions->value(), checked);
        }
    });

    onPlaylistTabChanged(m_musicPlaylistWidget->currentIndex());

    // =========================================================
    // 7. 本棚 (Bookshelf) のセットアップ
    // =========================================================
    // 本棚のオプション用ウィジェットを作成
    m_bookshelfThumbnailsVisible = true;
    m_bookshelfOptions = new ListOptionsWidget(textSizeIcon, this);
    m_bookshelfOptions->setReorderVisible(false);

    m_bookshelfOptions->setContextMenuPolicy(Qt::CustomContextMenu);

    m_bookshelfSyncDateFontAction = new QAction("日付ラベルも連動してサイズ変更", this);
    m_bookshelfSyncDateFontAction->setCheckable(true);
    m_bookshelfSyncDateFontAction->setChecked(true);

    m_bookshelfShowImagesCheckbox = new QCheckBox("画像も表示する", this);
    m_bookshelfShowImagesCheckbox->setToolTip("参照中のディレクトリ内の画像も本棚に表示します");

    m_bookshelfToggleThumbnailButton = new QToolButton(this);
    m_bookshelfToggleThumbnailButton->setStyleSheet("QToolButton { border: none; background: transparent; }");
    m_bookshelfToggleThumbnailButton->setIcon(m_themeManager->getIcon("image"));
    m_bookshelfToggleThumbnailButton->setToolTip("サムネイルの表示/非表示");
    m_bookshelfToggleThumbnailButton->setIconSize(QSize(20, 20));

    m_bookshelfOptions->addOptionWidget(m_bookshelfToggleThumbnailButton);
    m_bookshelfOptions->addOptionWidget(m_bookshelfShowImagesCheckbox);

    // 1. .uiファイルで作成された既存のリストウィジェットを削除
    if (ui->listWidget) {
        delete ui->listWidget;
        ui->listWidget = nullptr;
    }

    // 2. 新しい BookshelfWidget を作成
    m_bookshelfWidget = new BookshelfWidget(this);
    m_bookshelfWidget->setImageExtensions(m_imageExtensions);

    // 3. BookshelfWidget をレイアウトの先頭に追加
    ui->verticalLayout_2->insertWidget(0, m_bookshelfWidget);

    // 4. その下にオプションバーを追加 (コンテナではなく m_bookshelfOptions を直接追加)
    ui->verticalLayout_2->addWidget(m_bookshelfOptions);

    // 5. 初期化
    m_bookshelfWidget->navigateToPath(QDir::homePath());


    // =========================================================
    // 8. スライドショー (Slideshow) の設定
    // =========================================================
    ui->slideshowProgressBar->hide();
    ui->slideshowProgressBar->setTextVisible(false);
    ui->slideshowIntervalSpinBox->installEventFilter(this);
    ui->slideshowIntervalSpinBox->setSingleStep(0.1);

    // 1. SlideshowWidgetを作成
    m_slideshowWidget = new SlideshowWidget(this);
    m_slideshowWidget->setImageExtensions(m_imageExtensions);

    // 2. レイアウトに追加 (リストを先に追加)
    ui->verticalLayout_11->addWidget(m_slideshowWidget);

    // 3. 追加: オプションをリストの下に追加
    ui->verticalLayout_11->addWidget(m_slideshowOptions);

    // =========================================================
    // 9. メニューバー・アクションの設定
    // =========================================================
    m_recentFilesMenu = new QMenu(this);
    ui->actionRecentFiles->setMenu(m_recentFilesMenu);

    m_viewPageActionGroup = new QActionGroup(this);
    m_viewPageActionGroup->addAction(ui->actionShowMediaPage);
    m_viewPageActionGroup->addAction(ui->actionShowVideoPage);
    m_viewPageActionGroup->setExclusive(true);

    // =========================================================
    // 10. コントロールバーのアニメーション設定
    // =========================================================
    m_controlBarFadeAnimation = new QPropertyAnimation(nullptr, "windowOpacity", this);
    m_controlBarFadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(m_controlBarFadeAnimation, &QPropertyAnimation::finished, this, [this](){
        // ターゲットがコンテナの場合のみ処理
        if (m_fullScreenContainer && m_controlBarFadeAnimation->targetObject() == m_fullScreenContainer) {
            if (m_fullScreenContainer->windowOpacity() == 0.0) {
                if (m_isVideoFullScreen) {
                    m_fullScreenContainer->hide();

                    // ★追加: コントロールバーが隠れたら、フォーカスをメインウィンドウに戻す
                    // これにより、隠れたボタンがSpaceキーに反応するのを防ぎ、
                    // MainWindowのショートカットキーを有効にします。
                    this->activateWindow();
                    this->setFocus();
                }
            }
        }
    });

    // =========================================================
    // 11. 起動時の最終調整
    // =========================================================
    ui->slideshowPlayPauseButton->setToolTip("Play Slideshow");
    syncControlBarButtons();
    ui->mediaStackedWidget->setCurrentWidget(ui->mediaPage);
    if (m_folderTreeWidget) {m_folderTreeWidget->setFocus();}

    m_controlBar->installEventFilter(this);

    // findChildren を使って、孫要素を含むすべてのウィジェットにフィルタを設定
    QList<QWidget*> allControls = m_controlBar->findChildren<QWidget*>();
    for (QWidget* child : allControls) {
        child->installEventFilter(this);
    }

    // CompactWindowのControlBarも同様
    if (m_compactWindow && m_compactWindow->controlBar()) {
        m_compactWindow->controlBar()->installEventFilter(this);

        QList<QWidget*> allCompactControls = m_compactWindow->controlBar()->findChildren<QWidget*>();
        for (QWidget* child : allCompactControls) {
            child->installEventFilter(this);
        }
    }

    setAnimated(false);
    updateDockWidgetBehavior();
    updatePlaylistButtonStates();

    int initialVolume = qMin(m_settingsManager->settings().lastVolume, 100);
    if(m_mediaManager) m_mediaManager->setVolume(initialVolume);
    updateControlBarStates();
    onMediaViewStatesChanged();

    if (m_settingsManager->settings().syncUiStateAcrossLists) {
        syncAllListsUiState(m_playlistOptions->value(), m_playlistOptions->isChecked());
    }

    setupBreadcrumbs();
    setupSortControls();
}

void MainWindow::setupConnections()
{
    // =========================================================
    // 1. 音楽プレイリスト (MusicPlaylistWidget) 関連
    // =========================================================

    // 再生要求 (Widget -> MainWindow -> PlaylistManager)
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::playRequest,
            this, [this](int playlistIndex, int trackIndex){
                m_nextPlayTrigger = PlayTrigger::UserAction; // ★ 追加: ユーザー操作としてマーク
                m_playlistManager->playTrackAtIndex(trackIndex, playlistIndex);
            });

    // 停止要求
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::stopRequest,
            this, &MainWindow::stopMusic);

    // タブ数変更 -> コントロールバーのボタン更新
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::playlistCountChanged,
            this, &MainWindow::syncControlBarButtons);

    // タブ名変更 -> コントロールバーのボタン名更新
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::tabRenamed,
            this, [this](int index, const QString &name){
                m_controlBar->updatePlaylistButtonText(index, name);
            });

    // タブ切り替え -> アクティブなリスト更新、ControlBar同期
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::currentChanged,
            this, &MainWindow::onPlaylistTabChanged);

    // ファイル追加メニュー
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::requestOpenFile, this, &MainWindow::openFile);
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::requestOpenFolder, this, &MainWindow::openFolder);

    // プレビュー更新依頼
    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::previewUpdateRequested, this, &MainWindow::updatePreviews);

    connect(m_musicPlaylistWidget, &MusicPlaylistWidget::customizeContextMenu,
            this, &MainWindow::appendPreviewMenuActions);


    // =========================================================
    // 2. プレビューコンテナ関連
    // =========================================================
    connect(previewContainer, &QWidget::customContextMenuRequested,
            this, &MainWindow::showPreviewContextMenu);
    for (int i = 0; i < previewListWidgets.size(); ++i) {
        connect(previewListWidgets[i], &QListWidget::itemDoubleClicked,
                this, [this, i](QListWidgetItem *item){

                    // マッピングから実際のプレイリストIDを取得
                    int realPlaylistIndex = m_previewSlotMapping[i];
                    if (realPlaylistIndex != -1) {
                        int trackIndex = previewListWidgets[i]->row(item);

                        m_nextPlayTrigger = PlayTrigger::UserAction; // ★ 追加: ユーザー操作としてマーク
                        m_playlistManager->playTrackAtIndex(trackIndex, realPlaylistIndex);
                    }
                });
    }


    // =========================================================
    // 3. スライドショー (Slideshow) 関連
    // =========================================================
    // 初期リストをコントローラに設定
    m_imageViewController->setSlideshowList(m_slideshowWidget->currentListWidget());

    // リスト切り替え時の連携
    connect(m_slideshowWidget, &SlideshowWidget::currentListChanged, this, [this](QListWidget* list){
        m_imageViewController->setSlideshowList(list);

        // オプション設定の適用（個別モード時）
        if (!m_settingsManager->settings().syncUiStateAcrossLists && list) {
            m_slideshowOptions->setFontSize(list->font().pointSize());
            bool isReorderEnabled = (list->dragDropMode() == QAbstractItemView::InternalMove);
            m_slideshowOptions->setReorderEnabled(isReorderEnabled);
        }
    });

    // ファイル追加通知
    connect(m_slideshowWidget, &SlideshowWidget::filesAddedToPlaylist, this, [this](bool wasInitiallyEmpty){
        m_imageViewController->onSlideshowPlaylistChanged();

        if (wasInitiallyEmpty) {
            QListWidget* list = m_slideshowWidget->currentListWidget();
            if (list && list->count() > 0) {
                QListWidgetItem* firstItem = list->item(0);
                m_imageViewController->switchToSlideshowListMode(firstItem);

                QString listName = "Slideshow";
                if (m_slideshowWidget) {
                    listName = m_slideshowWidget->tabText(m_slideshowWidget->currentIndex());
                }
                updateBreadcrumbs(listName);
            }
        }
    });

    // ローディング状態
    connect(m_slideshowWidget, &SlideshowWidget::fileLoadStarted, this, [this](){
        m_imageViewController->setLoading(true);
        QApplication::setOverrideCursor(Qt::WaitCursor);
    });
    connect(m_slideshowWidget, &SlideshowWidget::fileLoadFinished, this, [this](){
        m_imageViewController->setLoading(false);
        QApplication::restoreOverrideCursor();
    });

    // アイテム操作
    connect(m_slideshowWidget, &SlideshowWidget::itemDoubleClicked,
            this, &MainWindow::onSlideshowListItemDoubleClicked);

    // メニュー要求
    connect(m_slideshowWidget, &SlideshowWidget::requestFileOpen, this, [this](){
        QStringList files = QFileDialog::getOpenFileNames(this, "画像を開く", QDir::homePath(), "画像 (*.jpg *.png *.bmp *.gif)");
        if(!files.isEmpty()) m_slideshowWidget->addFilesToCurrentList(files);
    });
    connect(m_slideshowWidget, &SlideshowWidget::requestFolderOpen, this, &MainWindow::openFolder);

    // コントロールバー操作
    connect(ui->slideshowIntervalSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_imageViewController, &ImageViewController::setSlideshowInterval);
    connect(ui->slideshowPlayPauseButton, &QPushButton::clicked,
            m_imageViewController, &ImageViewController::toggleSlideshow);
    connect(ui->slideshowEffectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index){
                Q_UNUSED(index);
                // コンボボックスに埋め込まれた ID (0=なし, 1=フェード, 2=スライド) を取得して送信
                int effectId = ui->slideshowEffectComboBox->currentData().toInt();
                m_imageViewController->setSlideshowEffect(effectId);
            });

    // オプション連携
    connect(m_slideshowOptions, &ListOptionsWidget::fontSizeChanged, this, [this](int size){
        m_slideshowWidget->applyUiSettings(size, m_slideshowOptions->isChecked());
        if (m_settingsManager->settings().syncUiStateAcrossLists) syncAllListsUiState(size, m_slideshowOptions->isChecked());
    });
    connect(m_slideshowOptions, &ListOptionsWidget::reorderToggled, this, [this](bool checked){
        m_slideshowWidget->applyUiSettings(m_slideshowOptions->value(), checked);
        if (m_settingsManager->settings().syncUiStateAcrossLists) syncAllListsUiState(m_slideshowOptions->value(), checked);
    });


    // =========================================================
    // 4. 本棚 (Bookshelf) 関連
    // =========================================================
    connect(m_bookshelfOptions, &ListOptionsWidget::fontSizeChanged,
            m_bookshelfWidget, &BookshelfWidget::setFontSize);

    connect(m_bookshelfShowImagesCheckbox, &QCheckBox::toggled,
            m_bookshelfWidget, &BookshelfWidget::setShowImages);

    connect(m_bookshelfSyncDateFontAction, &QAction::toggled,
            m_bookshelfWidget, &BookshelfWidget::setSyncDateFont);

    connect(m_bookshelfToggleThumbnailButton, &QToolButton::clicked, this, [this](){
        bool visible = !m_bookshelfThumbnailsVisible;
        m_bookshelfThumbnailsVisible = visible;
        m_bookshelfWidget->setThumbnailsVisible(visible);
        m_bookshelfToggleThumbnailButton->setIcon(m_themeManager->getIcon(visible ? "image" : "no_image"));
    });

    connect(m_bookshelfWidget, &BookshelfWidget::directoryChanged, this, [this](const QString &path){
        updateBreadcrumbs(path);

        // キャッシュから前回のファイルを取得
        QString lastFile = m_settingsManager->getDirectoryHistory(path);

        qDebug() << "[MainWindow] Signal: directoryChanged triggered.";
        qDebug() << "             Restoring from History:" << (lastFile.isEmpty() ? "<EMPTY>" : lastFile);
        // ----------------------

        // 指定ファイルでロード
        m_imageViewController->loadDirectory(QDir(path), lastFile);
    });

    connect(m_imageViewController, &ImageViewController::currentImageChanged, this, [this](const QString& filePath){
        QFileInfo fi(filePath);
        QString dirPath = fi.absolutePath();

        // 1. 履歴保存
        m_settingsManager->setDirectoryHistory(dirPath, filePath);

        // 2. ★追加: ファイル名表示の更新
        if (ui->filenameLineEdit) {
            ui->filenameLineEdit->setText(fi.fileName());
        }

        // 3. ★追加: 本棚も同期させる（ディレクトリが変わった場合のため）
        if (m_bookshelfWidget) {
            m_bookshelfWidget->navigateToPath(filePath);
        }
    });

    connect(m_bookshelfWidget, &BookshelfWidget::imageFileSelected,
            this, [this](const QString &path){
                QFileInfo fi(path);
                m_imageViewController->loadDirectory(fi.dir(), path);
                ui->mediaStackedWidget->setCurrentWidget(ui->mediaPage);
            });

    connect(m_bookshelfOptions, &QWidget::customContextMenuRequested,
            this, &MainWindow::onBookshelfOptionsContextMenu);


    // =========================================================
    // 5. フォルダツリー (FolderTree) 関連
    // =========================================================
    // ファイルダブルクリック時
    connect(m_folderTreeWidget, &FolderTreeWidget::fileActivated, this, &MainWindow::onNavFileActivated);

    // コンテキストメニューからの要求
    connect(m_folderTreeWidget, &FolderTreeWidget::requestAddToPlaylist, this, [this](const QString &path){
        startFileScan({QUrl::fromLocalFile(path)}); // デフォルトプレイリストへ
    });

    connect(m_folderTreeWidget, &FolderTreeWidget::requestAddToSlideshow, this, [this](const QString &path){
        // スライドショーへ追加 (startFileScan経由または直接)
        // ここでは画像として扱う
        m_slideshowWidget->addFilesToCurrentList({path});
    });

    connect(m_folderTreeWidget, &FolderTreeWidget::requestOpenInBookshelf, this, [this](const QString &path){
        m_bookshelfWidget->navigateToPath(path);
        m_imageViewController->loadDirectory(QDir(path), QString());
        // 必要なら本棚ドックを表示
        m_bookshelfDock->show();
        m_bookshelfDock->raise();
    });

    // オプション連携
    connect(m_folderTreeOptions, &ListOptionsWidget::fontSizeChanged, this, [this](int fontSize){
        if (m_settingsManager->settings().syncUiStateAcrossLists) {
            syncAllListsUiState(fontSize, m_playlistOptions->isChecked());
        } else {
            // 個別にフォント適用
            QFont f = m_folderTreeWidget->font();
            f.setPointSize(fontSize);
            m_folderTreeWidget->setFont(f);
        }
    });

    // =========================================================
    // 6. コントロールバー & コンパクトウィンドウ
    // =========================================================
    connect(m_controlBar, &ControlBar::playlistButtonClicked, this, &MainWindow::startPlaybackOnPlaylist);
    connect(m_controlBar, &ControlBar::playPauseClicked, this, &MainWindow::handlePlayPauseClicked);
    connect(m_controlBar, &ControlBar::stopClicked, this, &MainWindow::handleStopClicked);
    connect(m_controlBar, &ControlBar::nextClicked, this, &MainWindow::handleNextClicked);
    connect(m_controlBar, &ControlBar::prevClicked, this, &MainWindow::handlePrevClicked);
    connect(m_controlBar, &ControlBar::repeatClicked, this, &MainWindow::handleRepeatClicked);
    connect(m_controlBar, &ControlBar::shuffleClicked, this, &MainWindow::handleShuffleClicked);
    connect(m_controlBar, &ControlBar::muteClicked, m_mediaManager, &MediaManager::handleMuteClicked);
    connect(m_controlBar, &ControlBar::positionSliderMoved, m_mediaManager, &MediaManager::setPosition);
    connect(m_controlBar, &ControlBar::positionSliderPressed, m_mediaManager, &MediaManager::handlePositionSliderPressed);
    connect(m_controlBar, &ControlBar::positionSliderReleased, m_mediaManager, &MediaManager::handlePositionSliderReleased);
    connect(m_controlBar, &ControlBar::volumeChanged, m_mediaManager, &MediaManager::setVolume);

    ControlBar* compactControlBar = m_compactWindow->controlBar();
    connect(compactControlBar, &ControlBar::playPauseClicked, this, &MainWindow::handlePlayPauseClicked);
    connect(compactControlBar, &ControlBar::stopClicked, this, &MainWindow::handleStopClicked);
    connect(compactControlBar, &ControlBar::nextClicked, this, &MainWindow::handleNextClicked);
    connect(compactControlBar, &ControlBar::prevClicked, this, &MainWindow::handlePrevClicked);
    connect(compactControlBar, &ControlBar::repeatClicked, this, &MainWindow::handleRepeatClicked);
    connect(compactControlBar, &ControlBar::shuffleClicked, this, &MainWindow::handleShuffleClicked);
    connect(compactControlBar, &ControlBar::muteClicked, m_mediaManager, &MediaManager::handleMuteClicked);
    connect(compactControlBar, &ControlBar::positionSliderMoved, m_mediaManager, &MediaManager::setPosition);
    connect(compactControlBar, &ControlBar::positionSliderPressed, m_mediaManager, &MediaManager::handlePositionSliderPressed);
    connect(compactControlBar, &ControlBar::positionSliderReleased, m_mediaManager, &MediaManager::handlePositionSliderReleased);
    connect(compactControlBar, &ControlBar::volumeChanged, m_mediaManager, &MediaManager::setVolume);
    connect(compactControlBar, &ControlBar::playlistButtonClicked, this, &MainWindow::startPlaybackOnPlaylist);

    connect(m_compactWindow, &CompactWindow::switchToFullModeRequested, this, &MainWindow::switchToFullMode);
    connect(m_compactWindow, &CompactWindow::closed, this, &MainWindow::switchToFullMode);

    // =========================================================
    // 7. 各種マネージャとの連携
    // =========================================================
    // MediaManager
    connect(m_mediaManager, &MediaManager::playbackStateChanged, this, &MainWindow::updateControlBarStates);
    connect(m_mediaManager, &MediaManager::trackFinished, this, [this](){
        m_playlistManager->requestNextTrack();
    });
    connect(m_mediaManager, &MediaManager::loadingStateChanged, this, &MainWindow::onMediaLoadingStateChanged);
    connect(m_mediaManager, &MediaManager::volumeChanged, this, &MainWindow::onMediaVolumeChanged);
    connect(m_mediaManager, &MediaManager::positionChanged, m_controlBar, &ControlBar::setProgressPosition);
    connect(m_mediaManager, &MediaManager::durationChanged, m_controlBar, &ControlBar::setProgressDuration);
    connect(m_mediaManager, &MediaManager::timeLabelChanged, m_controlBar, &ControlBar::setTimeLabel);
    connect(m_mediaManager, &MediaManager::positionChanged, compactControlBar, &ControlBar::setProgressPosition);
    connect(m_mediaManager, &MediaManager::durationChanged, compactControlBar, &ControlBar::setProgressDuration);
    connect(m_mediaManager, &MediaManager::timeLabelChanged, compactControlBar, &ControlBar::setTimeLabel);

    // PlaylistManager
    connect(m_playlistManager, &PlaylistManager::trackReadyToPlay, this, &MainWindow::onTrackReadyToPlay);
    connect(m_playlistManager, &PlaylistManager::playbackShouldStop, this, &MainWindow::onPlaybackShouldStop);
    connect(m_playlistManager, &PlaylistManager::playlistDataChanged, this, &MainWindow::onPlaylistDataChanged);
    connect(m_playlistManager, &PlaylistManager::loadingStateChanged, this, &MainWindow::onMediaLoadingStateChanged);

    // ImageViewController
    connect(m_imageViewController, &ImageViewController::mediaViewStatesChanged, this, &MainWindow::onMediaViewStatesChanged);
    connect(m_imageViewController, &ImageViewController::setItemHighlighted, this, &MainWindow::onSetItemHighlighted);

    // ▼▼▼ 修正: ディレクトリが変わったとき (Up/Back/Forwardボタンなど) ▼▼▼
    // 以前は updateBreadcrumb だけでしたが、本棚の移動もここで行います
    connect(m_imageViewController, &ImageViewController::currentDirectoryChanged, this, [this](const QString &path){
        // 1. パンくずリスト更新
        updateBreadcrumbs(path);

        // 2. ★追加: 本棚をそのディレクトリへ移動
        if (m_bookshelfWidget) {
            m_bookshelfWidget->navigateToPath(path);
        }
    });

    // ▼▼▼ 修正: 表示画像が変わったとき (矢印キーやクリック移動など) ▼▼▼
    // 以前は履歴保存だけでしたが、ファイル名表示の更新を追加します
    connect(m_imageViewController, &ImageViewController::currentImageChanged, this, [this](const QString& filePath){
        QFileInfo fi(filePath);
        QString dirPath = fi.absolutePath();

        // 1. 履歴保存
        m_settingsManager->setDirectoryHistory(dirPath, filePath);

        // 2. ★追加: ファイル名表示 (lineEdit) の更新
        if (ui->filenameLineEdit) {
            ui->filenameLineEdit->setText(fi.fileName());
        }

        // 3. ★追加: 念のため本棚も追従させる (ディレクトリ跨ぎの移動などの保険)
        // ※ BookshelfWidget::navigateToPath は「同じパスならリロードしない」対策済みなので頻繁に呼んでも大丈夫です
        if (m_bookshelfWidget) {
            m_bookshelfWidget->navigateToPath(filePath);
        }
    });

    connect(m_imageViewController, &ImageViewController::filesDropped, this, [this](const QList<QUrl>& urls){
        // ★ 修正: IVCへのドロップ時も、再生フラグとトリガーをセットする
        m_autoPlayNextScan = true;
        m_nextPlayTrigger = PlayTrigger::OpenFile;

        // ターゲット指定なし（-1）でスキャン開始 -> アクティブなリストに追加される
        startFileScan(urls, -1);
    });

    connect(m_themeManager, &ThemeManager::themeChanged, this, [this](const QString&){
        // テーマ変更時は全アイコンの再設定が必要
        updateAllWidgetIcons();
        updateControlBarStates();

        // 設定ファイルへの保存は applySettings 経由で行われるが、
        // トグルボタン等からの変更の場合にここで保存する必要があるなら記述
        QSettings settings(SettingsManager::author, "QSupportViewer");
        settings.beginGroup("General");
        settings.setValue("theme", m_themeManager->currentTheme());
        settings.endGroup();
    });

    // =========================================================
    // 8. メニューアクション & UIイベント
    // =========================================================
    connect(ui->actionPlay, &QAction::triggered, this, &MainWindow::togglePlayback);
    connect(ui->actionStop, &QAction::triggered, this, &MainWindow::stopMusic);
    connect(ui->actionNext, &QAction::triggered, this, &MainWindow::handleNextClicked);
    connect(ui->actionPrev, &QAction::triggered, this, &MainWindow::handlePrevClicked);
    connect(ui->actionOpenFile, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionOpenFolder, &QAction::triggered, this, &MainWindow::openFolder);
    connect(ui->actionOptions, &QAction::triggered, this, &MainWindow::openOptionsDialog);
    connect(ui->actionAlwaysOnTop, &QAction::toggled, this, &MainWindow::toggleAlwaysOnTop);
    connect(ui->actionCompactMode, &QAction::triggered, this, &MainWindow::switchToCompactMode);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    connect(ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);

    connect(ui->mediaMenu, &QMenu::aboutToShow, this, &MainWindow::updateRecentFilesMenu);
    connect(ui->actionToggleTheme, &QAction::triggered, this, &MainWindow::onToggleTheme);
    connect(m_folderTreeDock, &QDockWidget::visibilityChanged, ui->actionShowFolderTree, &QAction::setChecked);
    connect(m_playlistDock, &QDockWidget::visibilityChanged, ui->actionShowPlaylist, &QAction::setChecked);
    connect(m_slideshowDock, &QDockWidget::visibilityChanged, ui->actionShowSlideshow, &QAction::setChecked);
    connect(m_bookshelfDock, &QDockWidget::visibilityChanged, ui->actionShowBookshelf, &QAction::setChecked);

    // 排他的Dock表示
    connect(ui->actionShowFolderTree, &QAction::triggered, this, [=](bool checked) {
        if (checked) {
            m_folderTreeDock->show();
            m_playlistDock->hide();
            m_slideshowDock->hide();
            m_bookshelfDock->hide();
        } else {
            m_folderTreeDock->hide();
        }
    });
    connect(ui->actionShowPlaylist, &QAction::triggered, this, [=](bool checked) {
        if (checked) {
            m_playlistDock->show();
            m_folderTreeDock->hide();
            m_slideshowDock->hide();
            m_bookshelfDock->hide();
        } else {
            m_playlistDock->hide();
        }
    });
    connect(ui->actionShowSlideshow, &QAction::triggered, this, [=](bool checked) {
        if (checked) {
            m_slideshowDock->show();
            m_folderTreeDock->hide();
            m_playlistDock->hide();
            m_bookshelfDock->hide();
        } else {
            m_slideshowDock->hide();
        }
    });
    connect(ui->actionShowBookshelf, &QAction::triggered, this, [=](bool checked) {
        if (checked) {
            m_bookshelfDock->show();
            m_folderTreeDock->hide();
            m_playlistDock->hide();
            m_slideshowDock->hide();
        } else {
            m_bookshelfDock->hide();
        }
    });

    connect(ui->videoFilePathLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onFilePathEntered);
    connect(ui->togglePanoramaButton, &QPushButton::clicked, m_imageViewController, &ImageViewController::onTogglePanoramaMode);
    connect(ui->toggleFitModeButton, &QPushButton::clicked, m_imageViewController, &ImageViewController::onToggleFitMode);
    connect(ui->toggleLayoutDirectionButton, &QPushButton::clicked, m_imageViewController, &ImageViewController::onLayoutDirectionToggled);
    connect(ui->viewControlSlider, &QSlider::valueChanged, m_imageViewController, &ImageViewController::onViewControlSliderMoved);
    connect(ui->zoomSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_imageViewController, &ImageViewController::onZoomSpinBoxChanged);
    connect(ui->backButton, &QPushButton::clicked, m_imageViewController, &ImageViewController::goBack);
    connect(ui->forwardButton, &QPushButton::clicked, m_imageViewController, &ImageViewController::goForward);
    connect(ui->upButton, &QPushButton::clicked, m_imageViewController, &ImageViewController::goUp);

    connect(ui->actionShowMediaPage, &QAction::triggered, this, [=]() {
        ui->mediaStackedWidget->setCurrentWidget(ui->mediaPage);
    });
    connect(ui->actionShowVideoPage, &QAction::triggered, this, [=]() {
        ui->mediaStackedWidget->setCurrentWidget(ui->videoPage);
    });
    connect(ui->mediaStackedWidget, &QStackedWidget::currentChanged, this, [=](int index) {
        if (index == ui->mediaStackedWidget->indexOf(ui->mediaPage)) {
            ui->actionShowMediaPage->setChecked(true);
        } else if (index == ui->mediaStackedWidget->indexOf(ui->videoPage)) {
            ui->actionShowVideoPage->setChecked(true);
        }
    });
    connect(ui->toggleVideoToMediaButton, &QPushButton::clicked, this, [=]() {
        ui->mediaStackedWidget->setCurrentWidget(ui->mediaPage);
    });
    connect(ui->toggleMediaToVideoButton, &QPushButton::clicked, this, [=]() {
        ui->mediaStackedWidget->setCurrentWidget(ui->videoPage);
    });

    // =========================================================
    // 9. FileScanner (ファイルスキャン) 関連
    // =========================================================
    connect(m_fileScanner, &FileScanner::scanStarted, this, [this](){
        m_imageViewController->setLoading(true);
        QApplication::setOverrideCursor(Qt::WaitCursor);
    });

    connect(m_fileScanner, &FileScanner::scanFinished, this,
            [this](const QStringList &audioVideo, const QStringList &images, int dropTarget, bool limitReached){

                m_imageViewController->setLoading(false);
                QApplication::restoreOverrideCursor();

                if (limitReached) {
                    QMessageBox::information(this, "ファイル追加制限",
                                             QString("一度に追加できるファイル数の上限 (%1 件) に達したため、\n"
                                                     "リストへの追加を中断しました。").arg(m_settingsManager->settings().fileScanLimit));
                }

                if (dropTarget != -1) {
                    // 特定のプレイリストへのドロップ時
                    if (!audioVideo.isEmpty()) {
                        m_playlistManager->addFilesToPlaylist(audioVideo, dropTarget);
                    }
                    m_autoPlayNextScan = false;
                } else {
                    // 汎用ドロップ時
                    if (!audioVideo.isEmpty()) {
                        // 追加前のカウントを保持
                        int startIndex = 0;
                        if (QListWidget* list = m_musicPlaylistWidget->currentListWidget()) {
                            startIndex = list->count();
                        }

                        // ★重要★: PlaylistManagerは追加と同時に再生を開始する場合があるため、
                        // 「追加する前」にトリガーをセットしておく必要があります。
                        if (m_autoPlayNextScan) {
                            qDebug() << "[Debug] scanFinished: Pre-setting Trigger to OpenFile";
                            m_nextPlayTrigger = PlayTrigger::OpenFile;
                        }

                        // ここで PM が内部的に playTrackAtIndex を呼ぶ可能性がある
                        m_playlistManager->addFilesToPlaylist(audioVideo, currentPlaylistIndex);

                        // PMが自動再生しなかった場合（既に再生中だった場合など）の保険として、
                        // 明示的な再生コマンドも残しておく
                        if (m_autoPlayNextScan) {
                            // もし再生中でなければ、上記 addFilesToPlaylist で再生が始まっているはずだが、
                            // 念のため再生されていない場合のみ再生する、といった制御も可能。
                            // ここでは強制的に再生コマンドを送っても問題ない（PM側で制御される）
                            m_playlistManager->playTrackAtIndex(startIndex, currentPlaylistIndex);
                        }
                    }
                    if (!images.isEmpty()) {
                        addFilesToSlideshowPlaylist(images);
                    }
                    m_autoPlayNextScan = false;
                }
            });
}

void MainWindow::setupShortcuts(){
    qDebug() << "Setting up keyboard shortcuts...";
    QShortcut *playPauseShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(playPauseShortcut, &QShortcut::activated, this, &MainWindow::togglePlayback);
    QShortcut *nextShortcut = new QShortcut(QKeySequence(QKeySequence(Qt::CTRL | Qt::Key_Right)), this);
    connect(nextShortcut, &QShortcut::activated, this, &MainWindow::handleNextClicked);
    QShortcut *prevShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left), this);
    connect(prevShortcut, &QShortcut::activated, this, &MainWindow::handlePrevClicked);
    QShortcut *stopShortcut = new QShortcut(QKeySequence(Qt::Key_S), this);
    connect(stopShortcut, &QShortcut::activated, this, &MainWindow::stopMusic);
    QShortcut *repeatShortcut = new QShortcut(QKeySequence(Qt::Key_R), this);
    connect(repeatShortcut, &QShortcut::activated, this, &MainWindow::handleRepeatClicked);
    QShortcut *muteShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(muteShortcut, &QShortcut::activated, m_mediaManager, &MediaManager::handleMuteClicked);
    QShortcut *volumeUpShortcut = new QShortcut(QKeySequence(Qt::Key_Plus), this);
    connect(volumeUpShortcut, &QShortcut::activated, this, [this](){
        int newVolume = qMin(500, m_mediaManager->getCurrentVolume() + 5);
        m_mediaManager->setVolume(newVolume);
    });
    QShortcut *volumeDownShortcut = new QShortcut(QKeySequence(Qt::Key_Minus), this);
    connect(volumeDownShortcut, &QShortcut::activated, this, [this](){
        int newVolume = qMax(0, m_mediaManager->getCurrentVolume() - 5);
        m_mediaManager->setVolume(newVolume);
    });
    QShortcut *seekForwardShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(seekForwardShortcut, &QShortcut::activated, this, [this](){
        int newPos = m_controlBar->position() + 5000;
        m_mediaManager->setPosition(newPos);
    });
    QShortcut *seekBackwardShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(seekBackwardShortcut, &QShortcut::activated, this, [this](){
        int newPos = m_controlBar->position() - 5000;
        m_mediaManager->setPosition(newPos);
    });
    for (int i = 0; i < 10; ++i) {
        // i=0はKey_1, i=8はKey_9, i=9はKey_0 に対応
        Qt::Key key = (i < 9) ? Qt::Key(Qt::Key_1 + i) : Qt::Key_0;
        QShortcut *shortcut = new QShortcut(QKeySequence(Qt::CTRL | key), this);
        connect(shortcut, &QShortcut::activated, this, [this, i](){
            if (i < m_recentFiles.size()) {
                QString filePath = m_recentFiles.at(i);
                QFileInfo fileInfo(filePath);
                if (fileInfo.exists()) {
                    m_nextPlayTrigger = PlayTrigger::UserAction;

                    m_playlistManager->addFilesToPlaylist({filePath}, currentPlaylistIndex);
                } else {
                    QMessageBox::warning(this, "ファイルが見つかりません",
                                         QString("ファイルが見つかりませんでした:\n%1").arg(filePath));
                    addFileToRecentList(filePath); // 不正なパスをリストから削除する
                }
            }
        });
    }
    for (int i = 0; i < 10; ++i) {
        // キーの数値を計算 (Index 0 -> 1, ..., Index 9 -> 0)
        int keyNum = (i + 1) % 10;

        // キーコード生成
        Qt::Key key = static_cast<Qt::Key>(Qt::Key_0 + keyNum);

        QShortcut *shortcut = new QShortcut(QKeySequence(key), this);

        // ラムダ式で接続
        connect(shortcut, &QShortcut::activated, this, [this, i](){
            // プレイリストの範囲チェック
            if (m_musicPlaylistWidget && i < m_musicPlaylistWidget->count()) {

                // ★修正: 単なるタブ切り替え(setCurrentIndex)ではなく、
                // 「プレイリスト再生ボタン」を押したのと同じ関数を呼び出す
                // これにより、タブ切り替え + 先頭からの再生開始 が行われます。
                startPlaybackOnPlaylist(i);
            }
        });
    }


    QShortcut *slideshowShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(slideshowShortcut, &QShortcut::activated, m_imageViewController, &ImageViewController::toggleSlideshow);

    QShortcut *fullScreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullScreenShortcut, &QShortcut::activated, this, [this](){

        // もしビデオフルスクリーン中なら、そちらの解除処理を呼ぶ
        if (m_isVideoFullScreen) {
            toggleFullScreenVideo();
            return;
        }

        isMediaViewFullScreen = !isMediaViewFullScreen;

        // 隠すべきウィジェットリスト
        QList<QWidget*> panels = {
            m_folderTreeDock, m_playlistDock, m_slideshowDock, m_bookshelfDock,
            ui->leftToolBar, ui->rightToolBar, ui->menubar, ui->controlToolBar,
            // ★ 追加: ここにもウィジェットを追加
            ui->widget_3, ui->widget_7, ui->widget_8
        };

        if (isMediaViewFullScreen) {
            for (QWidget* panel : panels) {
                if (panel->isVisible()) panel->hide();
            }
            ui->slideshowProgressBar->hide();
            this->showFullScreen();

            // ★ メディアページでもコントロールバーをフロートさせるならここで同様の処理が必要
            // 今回は「ビデオ再生領域のダブルクリック」の要望メインでしたが、
            // F11でも全画面ならバーを出したい場合はここに toggleFullScreenVideo と同様の
            // setParentロジックを追加してください。
            // (要望には「メディアページでも...非表示にしたい」とあるので、隠す処理は必須です)

        } else {
            for (QWidget* panel : panels) {
                // Dockはアクションの状態を見て復元判断が必要ですが、
                // widget_3, 7, 8 は基本的に常時表示なのでshowしてOK
                if (panel == ui->widget_3 || panel == ui->widget_7 || panel == ui->widget_8 ||
                    panel == ui->leftToolBar || panel == ui->rightToolBar || panel == ui->menubar || panel == ui->controlToolBar) {
                    panel->show();
                }
            }
            // Dockの復元
            if(ui->actionShowFolderTree->isChecked()) m_folderTreeDock->show();
            if(ui->actionShowPlaylist->isChecked()) m_playlistDock->show();
            if(ui->actionShowSlideshow->isChecked()) m_slideshowDock->show();
            if(ui->actionShowBookshelf->isChecked()) m_bookshelfDock->show();

            if (m_imageViewController->isSlideshowActive()) {
                ui->slideshowProgressBar->show();
            }
            this->showNormal();
        }
    });
    QShortcut *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShortcut, &QShortcut::activated, this, [this](){
        if (m_isVideoFullScreen) {
            toggleFullScreenVideo();
        } else if (isMediaViewFullScreen) {
            // 既存のF11フルスクリーンの解除ロジックもここに統合すると親切です
            // (F11の処理と同じコードを呼ぶなど)
        }
    });
    QShortcut *compactModeShortcut = new QShortcut(QKeySequence(Qt::Key_F10), this);
    compactModeShortcut->setContext(Qt::WindowShortcut);
    connect(compactModeShortcut, &QShortcut::activated, this, &MainWindow::toggleCompactMode);

    QShortcut *aotShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), this);
    connect(aotShortcut, &QShortcut::activated, this, [this](){
        // 現在の状態を反転させてトグルする
        toggleAlwaysOnTop(!m_isAlwaysOnTop);
    });
}

void MainWindow::setupCustomTitleBar()
{
    // m_titleBarWidget = new QWidget(this);
    // m_titleBarWidget->setObjectName("CustomTitleBar");

    // // ★修正1: 高さを縮める
    // int barHeight = 32;
    // m_titleBarWidget->setFixedHeight(barHeight);

    // // レイアウト設定
    // QHBoxLayout *layout = new QHBoxLayout(m_titleBarWidget);
    // layout->setContentsMargins(10, 0, 0, 0);
    // layout->setSpacing(0);

    // // ★修正2: 垂直方向のアライメント指定(AlignVCenter)を【削除】
    // // これを消さないとメニューバーとの間に隙間が生まれます
    // // layout->setAlignment(Qt::AlignVCenter);  <-- 削除！

    // // アイコン (少し小さく表示)
    // QLabel *iconLabel = new QLabel(m_titleBarWidget);
    // iconLabel->setPixmap(this->windowIcon().pixmap(16, 16));
    // iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // // タイトル
    // QLabel *titleLabel = new QLabel("QSupportViewer", m_titleBarWidget);
    // titleLabel->setStyleSheet("font-weight: bold; margin-left: 8px; margin-right: 15px;");
    // titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    // // メニューバー
    // ui->menubar->setParent(m_titleBarWidget);
    // // ★修正3: 縦方向に引き伸ばす設定 (Expanding)
    // ui->menubar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    // ui->menubar->setStyleSheet("background: transparent;");

    // // ボタン作成
    // m_minBtn = new QToolButton(m_titleBarWidget);
    // m_maxBtn = new QToolButton(m_titleBarWidget);
    // m_closeBtn = new QToolButton(m_titleBarWidget);

    // m_minBtn->setObjectName("TitleBarMinBtn");
    // m_maxBtn->setObjectName("TitleBarMaxBtn");
    // m_closeBtn->setObjectName("TitleBarCloseBtn");

    // // ★修正4: アイコンサイズを明示的に小さく設定 (16x16 または 12x12)
    // // これによりボタン内でアイコンが小さく表示され、余白が生まれます
    // QSize iconSize(16, 16);
    // m_minBtn->setIconSize(iconSize);
    // m_maxBtn->setIconSize(iconSize);
    // m_closeBtn->setIconSize(iconSize);

    // // ★修正5: ボタン自体のサイズ設定
    // // 横幅 46px (Windows標準幅), 高さ barHeight (24px)
    // QSize btnSize(46, barHeight);
    // m_minBtn->setFixedSize(btnSize);
    // m_maxBtn->setFixedSize(btnSize);
    // m_closeBtn->setFixedSize(btnSize);

    // // レイアウト配置
    // // アイコンとタイトルは中央寄せ
    // layout->addWidget(iconLabel, 0, Qt::AlignVCenter);
    // layout->addWidget(titleLabel, 0, Qt::AlignVCenter);

    // // メニューバーは引き伸ばし (Alignment指定なし)
    // layout->addWidget(ui->menubar);

    // layout->addStretch();

    // // ボタン配置
    // layout->addWidget(m_minBtn);
    // layout->addWidget(m_maxBtn);
    // layout->addWidget(m_closeBtn);

    // // シグナル接続
    // connect(m_minBtn, &QToolButton::clicked, this, &MainWindow::showMinimized);
    // connect(m_closeBtn, &QToolButton::clicked, this, &MainWindow::close);
    // connect(m_maxBtn, &QToolButton::clicked, this, &MainWindow::toggleMaximizeWithFade);

    // this->setMenuWidget(m_titleBarWidget);
}

void MainWindow::performInitialUiUpdate()
{
    m_imageViewController->handleResize();
    updateAllWidgetIcons();
    updateRecentFilesMenu();
    const AppSettings& s = m_settingsManager->settings();

    // 1. ビュー状態の復元
    m_imageViewController->setViewStates(s.isPanoramaMode, s.fitMode, s.layoutDirection);

    updateMediaViewStates();
    updateControlBarStates();
    onMediaViewStatesChanged();
    updatePlaylistButtonStates();

    // 2. スライドショー効果の復元
    if (m_imageViewController) {
        int initialEffectId = ui->slideshowEffectComboBox->currentData().toInt();
        m_imageViewController->setSlideshowEffect(initialEffectId);
    }
    ui->slideshowIntervalSpinBox->setValue(m_settingsManager->settings().slideshowInterval);

    // ディレクトリとファイルの復元ロジック
    QString initialPath = s.lastBookshelfPath;
    if (!s.lastViewedFile.isEmpty()) {
        QFileInfo fi(s.lastViewedFile);
        if (fi.dir().absolutePath() == initialPath && fi.exists()) {
            m_settingsManager->setDirectoryHistory(initialPath, s.lastViewedFile);
        }
    }

    qDebug() << "[MainWindow] performInitialUiUpdate: Navigating Bookshelf...";
    m_bookshelfWidget->navigateToPath(initialPath);

    if (ui->mediaStackedWidget->currentWidget() == ui->mediaPage) {
        ui->actionShowMediaPage->setChecked(true);
    } else if (ui->mediaStackedWidget->currentWidget() == ui->videoPage) {
        ui->actionShowVideoPage->setChecked(true);
    }
}

void MainWindow::onTrackReadyToPlay(const QString& filePath, int playlistIndex, int trackIndex)
{
    qDebug() << "\n=== [Debug] onTrackReadyToPlay Start ===";
    qDebug() << "  File:" << filePath;
    qDebug() << "  IsFullScreen Mode:" << m_isVideoFullScreen;
    qDebug() << "  Window State:" << this->windowState(); // 4=FullScreen, 0=NoState

    // 隠したはずのパーツが見えているかチェック
    qDebug() << "  [Visibility Check]";
    qDebug() << "  - MenuBar:" << ui->menubar->isVisible();
    qDebug() << "  - ToolBar:" << ui->controlToolBar->isVisible();
    qDebug() << "  - LeftBar:" << ui->leftToolBar->isVisible();

    // StackedWidgetの状態
    QWidget* current = ui->mediaStackedWidget->currentWidget();
    qDebug() << "  Current Page:" << (current == ui->videoPage ? "VideoPage" : (current == ui->mediaPage ? "MediaPage" : "Unknown"));
    addFileToRecentList(filePath);
    QFileInfo fileInfo(filePath);
    m_currentTrackTitle = fileInfo.fileName();

    QString suffix = fileInfo.suffix().toLower();
    bool shouldSwitchPage = false;

    if (m_videoExtensions.contains(suffix)) {
        // 動画ファイルの場合
        qDebug() << "  Type: Video Detected";
        ui->videoFilePathLineEdit->setText(filePath);

        // ★ 設定に基づいた切り替え判定ロジック
        VideoSwitchPolicy policy = VideoSwitchPolicy::Default;
        const AppSettings& settings = m_settingsManager->settings();

        switch (m_nextPlayTrigger) {
        case PlayTrigger::OpenFile:
            policy = settings.switchOnOpenFile;
            qDebug() << "  Applying Setting: SwitchOnOpenFile";
            break;
        case PlayTrigger::UserAction:
            policy = settings.switchOnItemClick;
            qDebug() << "  Applying Setting: SwitchOnItemClick";
            break;
        case PlayTrigger::Auto:
            policy = settings.switchOnAutoPlay;
            qDebug() << "  Applying Setting: SwitchOnAutoPlay";
            break;
        }

        shouldSwitchPage = shouldSwitchToVideoPage(policy);

    } else if (m_audioExtensions.contains(suffix)) {
        // 音声ファイルの場合は常にMediaPage
        qDebug() << "  Type: Audio Detected -> Force MediaPage";
        shouldSwitchPage = true;
    } else {
        qDebug() << "  Type: Other/Image -> No Switch";
    }

    // 判定結果に基づいてページ切り替え
    if (shouldSwitchPage) {
        if (m_videoExtensions.contains(suffix)) {
            qDebug() << "  Action: Switching to VideoPage";
            if (ui->mediaStackedWidget->currentWidget() != ui->videoPage) {
                ui->mediaStackedWidget->setCurrentWidget(ui->videoPage);
            }
        } else {
            qDebug() << "  Action: Switching to MediaPage";
            // ビデオ以外ならフルスクリーン解除
            if (m_isVideoFullScreen) {
                qDebug() << "  -> Force exit FullScreen for non-video media.";
                toggleFullScreenVideo();
            }
        }
    } else {
        qDebug() << "  Action: Stay on current page";
    }

    // ★ 次回の再生のためにトリガーをデフォルト(Auto)に戻す
    m_nextPlayTrigger = PlayTrigger::Auto;
    m_playingPlaylistIndex = playlistIndex;

    // 現在のトラックインデックスをUI側でハイライト
    if (m_currentlyPlayingMusicItem) {
        // m_currentlyPlayingMusicItem が指すアイテムが、
        // まだリストウィジェットに属しているか（削除されていないか）を確認
        if (m_currentlyPlayingMusicItem->listWidget()) {
            // 有効な場合はハイライトを解除
            setItemHighlighted(m_currentlyPlayingMusicItem, false);
        } else {
            // 無効な場合（ダングリングポインタの場合）、
            // ポインタを nullptr にリセットして安全を確保する
            m_currentlyPlayingMusicItem = nullptr;
        }
    }
    QListWidget* targetList = m_musicPlaylistWidget->listWidget(playlistIndex);
    if (targetList && trackIndex < targetList->count()) {
        m_currentlyPlayingMusicItem = targetList->item(trackIndex);
        setItemHighlighted(m_currentlyPlayingMusicItem, true);
        targetList->setCurrentRow(trackIndex);
    }

    m_controlBar->setTrackInfo(fileInfo.fileName());
    m_compactWindow->controlBar()->setTrackInfo(fileInfo.fileName());

    qDebug() << "Current track:" << trackIndex;
    // 1. フルスクリーン時のみオーバーレイを表示する (幻影対策)
    //    ウィンドウモード時はチラついても目立たないため、UI操作性を優先して表示しない
    if (m_isVideoFullScreen && m_videoBlackOverlay) {
        m_videoBlackOverlay->setGeometry(ui->videoPage->rect());
        m_videoBlackOverlay->show();
        m_videoBlackOverlay->raise();
        m_videoBlackOverlay->repaint(); // 即座に黒くする
    }

    // 2. 再生開始イベントを監視してオーバーレイを消す (タイマー廃止)
    //    MediaManagerのシグナルに一度だけ反応する接続を作成
    QMetaObject::Connection* connection = new QMetaObject::Connection;
    *connection = connect(m_mediaManager, &MediaManager::playbackStateChanged, this,
                          [this, connection](bool isPlaying){
                              if (isPlaying) {
                                  // 再生状態になったら即座に隠す
                                  if (m_videoBlackOverlay) {
                                      m_videoBlackOverlay->hide();
                                  }
                                  // 接続を解除してメモリ解放
                                  disconnect(*connection);
                                  delete connection;
                              }
                          });

    // 3. 動画読み込み開始
    m_mediaManager->play(filePath);

    updateControlBarStates();
    updatePlaylistButtonStates();
}

void MainWindow::playSelected()
{
    int playlistIndex = m_musicPlaylistWidget->currentIndex();
    QListWidget* list = m_musicPlaylistWidget->currentListWidget();
    if (list) {
        int trackIndex = list->currentRow();
        m_playlistManager->playTrackAtIndex(trackIndex, playlistIndex);
    }
}

void MainWindow::stopMusic()
{
    qDebug() << "\n=== [Debug] stopMusic Called ===";

    // --- (既存のオーバーレイ表示処理) ---
    if (m_videoBlackOverlay) {
        m_videoBlackOverlay->setGeometry(ui->videoPage->rect()); // ※videoContainer->geometry() から変更しておくとより安全ですが、元のままでも動作はします
        m_videoBlackOverlay->show();
        m_videoBlackOverlay->raise();
        m_videoBlackOverlay->repaint();
    }

    m_playingPlaylistIndex = -1;
    m_mediaManager->stop();
    m_playlistManager->stopPlayback();
    m_currentTrackTitle.clear();

    // ★追加: 動画パスの表示をクリア
    ui->videoFilePathLineEdit->clear();

    // ★追加: コントロールバー（シークバー・時間）をリセット
    m_controlBar->setProgressPosition(0);
    m_controlBar->setProgressDuration(0);

    // コンパクトウィンドウ側のコントロールバーもリセット
    if (m_compactWindow && m_compactWindow->controlBar()) {
        m_compactWindow->controlBar()->setProgressPosition(0);
        m_compactWindow->controlBar()->setProgressDuration(0);
        m_compactWindow->controlBar()->clearPlaylistSelection();
    }

    if (m_currentlyPlayingMusicItem) {
        setItemHighlighted(m_currentlyPlayingMusicItem, false);
        m_currentlyPlayingMusicItem = nullptr;
    }

    updateControlBarStates();
    updatePlaylistButtonStates();
}

void MainWindow::startPlaybackOnPlaylist(int playlistIndex)
{
    QListWidget* targetList = m_musicPlaylistWidget->listWidget(playlistIndex);

    // 空の場合は再生できないのでリターン
    if (!targetList || targetList->count() == 0) {
        return;
    }
    // 再生インデックスを更新
    m_playingPlaylistIndex = playlistIndex;

    // UI上のタブ表示を切り替え
    m_musicPlaylistWidget->setCurrentIndex(playlistIndex);

    // 強制的にインデックス0（先頭）から再生を開始する
    // MediaManagerは新しいファイルパスを受け取ると、現在の再生を停止して新しいファイルを読み込みます
    m_playlistManager->playTrackAtIndex(0, playlistIndex);

    // ボタンの状態（ピンク色の下線など）を更新
    updatePlaylistButtonStates();
}

void MainWindow::switchToPlaylist(int index)
{
    if (index < 0 || index >= m_musicPlaylistWidget->count()) return;
    currentPlaylistIndex = index;
    m_musicPlaylistWidget->setCurrentIndex(index);
    m_playlistManager->playTrackAtIndex(0, index);
}

void MainWindow::togglePlayback()
{
    if (!m_mediaManager->isPlaying() && !m_mediaManager->isPaused()) {
        // --- 停止状態からの再生ロジック ---
        int playlistIndex = m_musicPlaylistWidget->currentIndex();
        QListWidget* currentList = m_musicPlaylistWidget->currentListWidget();
        if (currentList && currentList->count() == 0) {
            // プレイリストが空の場合、ユーザーに選択を促す
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("プレイリストが空です");
            msgBox.setText("メディアを追加しますか？");
            msgBox.setIcon(QMessageBox::Question);
            QPushButton *fileButton = msgBox.addButton("ファイルを開く(&F)...", QMessageBox::ActionRole);
            QPushButton *folderButton = msgBox.addButton("フォルダを開く(&D)...", QMessageBox::ActionRole);
            msgBox.addButton("キャンセル(&C)", QMessageBox::RejectRole);
            msgBox.setDefaultButton(fileButton);
            msgBox.exec();
            if (msgBox.clickedButton() == fileButton) openFile();
            else if (msgBox.clickedButton() == folderButton) openFolder();
            return;
        }
        int trackIndex = currentList ? currentList->currentRow() : 0;
        if (trackIndex < 0) trackIndex = 0;
        m_nextPlayTrigger = PlayTrigger::UserAction;

        m_playlistManager->playTrackAtIndex(trackIndex, playlistIndex);

    } else {
        // --- 再生/一時停止のトグル ---
        m_mediaManager->togglePlayback();
    }
}

void MainWindow::handleNextClicked()
{
    m_nextPlayTrigger = PlayTrigger::UserAction; // ★ユーザー操作であることを記録
    m_playlistManager->requestNextTrack();
}

void MainWindow::handlePrevClicked()
{
    m_nextPlayTrigger = PlayTrigger::UserAction; // ★ユーザー操作であることを記録
    m_playlistManager->requestPreviousTrack();
}

void MainWindow::handleRepeatClicked()
{
    // 1. PMから現在のモードを取得
    LoopMode currentMode = m_playlistManager->getLoopMode();
    // 2. 次のモードを決定
    LoopMode newMode;
    switch (currentMode) {
    case NoLoop:    newMode = RepeatOne; break;
    case RepeatOne: newMode = RepeatAll; break;
    case RepeatAll: newMode = NoLoop;    break;
    }
    // 3. PMに新しいモードを設定
    m_playlistManager->setLoopMode(newMode);
    // 4. UIを更新
    updateControlBarStates();
}

void MainWindow::handleShuffleClicked()
{
    // 1. PMから現在のモードを取得
    ShuffleMode currentMode = m_playlistManager->getShuffleMode();
    // 2. 次のモードを決定
    ShuffleMode newMode;
    switch (currentMode) {
    case ShuffleOff:      newMode = ShuffleNoRepeat; break;
    case ShuffleNoRepeat: newMode = ShuffleRepeat;   break;
    case ShuffleRepeat:   newMode = ShuffleOff;      break;
    }
    // 3. PMに新しいモードを設定
    m_playlistManager->setShuffleMode(newMode);
    // 4. UIを更新
    updateControlBarStates();
}


void MainWindow::handleSlideshowItemsDeletion(QListWidget* listWidget)
{
    const QList<QListWidgetItem*> selectedItems = listWidget->selectedItems();
    if (selectedItems.isEmpty()) return;

    // 1. 効率化のため、モデルのシグナルをブロック
    bool wasBlocked = listWidget->model()->blockSignals(true);

    bool displayedItemRemoved = false;
    // 後ろから順番に削除
    for (int i = listWidget->count() - 1; i >= 0; --i) {
        QListWidgetItem* item = listWidget->item(i);
        if (item->isSelected()) {
            if (item == m_currentlyDisplayedSlideItem) {
                displayedItemRemoved = true;
            }
            delete listWidget->takeItem(i); // アイテムを削除
        }
    }

    // 2. シグナルを元に戻す
    listWidget->model()->blockSignals(wasBlocked);

    // 3. 表示中のアイテムが削除された場合は画面をクリア
    if (displayedItemRemoved) {
        m_imageViewController->displayMedia(QString());
        m_currentlyDisplayedSlideItem = nullptr; // 安全のためクリア
    }

    // 4. IVCに通知し、スライダーの「範囲」を更新させる
    // (これによりIVCのバグがトリガーされ、スライダーが不正な値に設定される)
    m_imageViewController->onSlideshowPlaylistChanged(); // IVCに通知


    int correctIndex = 0; // デフォルトは0
    if (m_currentlyDisplayedSlideItem) {
        // (displayedItemRemoved が false の場合、このポインタは有効)
        correctIndex = listWidget->row(m_currentlyDisplayedSlideItem);

        if (correctIndex == -1) {
            // (ポインタは有効だが、listWidget->row() が -1 を返した)
            // (安全のため 0 にフォールバックし、0番目を表示し直す)
            correctIndex = 0;
            if (listWidget->count() > 0) {
                m_imageViewController->displayMedia(listWidget->item(0)->data(Qt::UserRole).toString());
            } else {
                m_imageViewController->displayMedia(QString());
            }
        }
    } else if (listWidget->count() > 0) {
        // (表示中のアイテムがない、または削除されたが、リストにはまだアイテムが残っている)
        // -> 0番目を表示し、スライダーも 0 に設定
        correctIndex = 0;
        m_imageViewController->displayMedia(listWidget->item(0)->data(Qt::UserRole).toString());
    } else {
        // (リストが空になった)
        correctIndex = 0;
        m_imageViewController->displayMedia(QString());
    }

    // 6. 算出した正しいインデックスでスライダーの値を上書き設定する
    ui->viewControlSlider->setValue(correctIndex);
}

void MainWindow::onBookshelfOptionsContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);
    // setupUiComponents で作成したアクションを追加
    contextMenu.addAction(m_bookshelfSyncDateFontAction);

    // メニューを実行
    contextMenu.exec(m_bookshelfOptions->mapToGlobal(pos));

    // アクションの ON/OFF は自動で切り替わります。
    // toggled シグナルは setupConnections で接続済みのため、
    // フォントの再適用も自動で行われます。
}

void MainWindow::onBreadcrumbClicked()
{
    QObject* senderObj = sender();
    if (!senderObj) return;

    // ボタンに保存しておいたフルパスを取得
    QString path = senderObj->property("path").toString();

    if (!path.isEmpty()) {
        m_bookshelfWidget->navigateToPath(path);
    }
}

void MainWindow::onFilePathEntered()
{
    // どのQLineEditから信号が来たかを取得
    QLineEdit* senderLineEdit = qobject_cast<QLineEdit*>(sender());
    if (!senderLineEdit) return;

    QString path = senderLineEdit->text();
    QFileInfo fileInfo(path);

    if (fileInfo.isDir()) {
        if (m_folderTreeWidget) {
            m_folderTreeWidget->navigateTo(path);
        }
        return;
    }

    // ファイルが存在しない場合はエラー表示
    if (!fileInfo.isFile() || !fileInfo.isReadable()) {
        senderLineEdit->setStyleSheet("background-color: #ffcccc;");
        QTimer::singleShot(1000, this, [=](){ senderLineEdit->setStyleSheet(""); });
        return;
    }

    QString suffix = fileInfo.suffix().toLower();

    if (senderLineEdit == ui->videoFilePathLineEdit) {
        // --- 動画用LineEditからの入力 ---
        if (m_audioExtensions.contains(suffix) || m_videoExtensions.contains(suffix)) {
            // PMにファイル追加を依頼
            m_playlistManager->addFilesToPlaylist({path}, currentPlaylistIndex);
        } else {
            // エラー表示
            senderLineEdit->setStyleSheet("background-color: #ffcccc;");
            QTimer::singleShot(1000, this, [=](){ senderLineEdit->setStyleSheet(""); });
        }
    }
}

void MainWindow::onMainTabTextUpdated(int index, const QString &text)
{
    if (index < 0 || index >= previewLabels.size()) return;

    // 1. プレビューのタイトルを更新 (この部分は変更なし)
    previewLabels[index]->setText(text);

    // 2. ControlBarのボタンテキスト更新スロットを呼び出す
    m_controlBar->updatePlaylistButtonText(index, text);
}

void MainWindow::onMediaLoadingStateChanged(bool isLoading)
{
    if (isLoading) {
        m_videoLoadingLabel->show();
        QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
        m_videoLoadingLabel->hide();
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::onMediaViewStatesChanged()
{
    // IVCから状態が変わったと通知があった
    updateMediaViewStates();
    updateSlideshowPlayPauseButton();
}

void MainWindow::onMediaVolumeChanged(int percent, bool isMuted)
{
    // スライダーなどのUIパーツ用には 192 で頭打ちにした値を使う
    int displayVolume = qMin(percent, 192);

    if (isMuted) {
        m_controlBar->setMuted(true, displayVolume, m_themeManager->getIcon("volume_mute"), QIcon(), QIcon(), QIcon());
        if (m_compactWindow && m_compactWindow->controlBar()) {
            m_compactWindow->controlBar()->setMuted(true, displayVolume, m_themeManager->getIcon("volume_mute"), QIcon(), QIcon(), QIcon());
        }
    } else {
        m_controlBar->setVolume(displayVolume,
                                m_themeManager->getIcon("volume_off1"),
                                m_themeManager->getIcon("volume_down"),
                                m_themeManager->getIcon("volume_up"));
        if (m_compactWindow && m_compactWindow->controlBar()) {
            m_compactWindow->controlBar()->setVolume(displayVolume,
                                                     m_themeManager->getIcon("volume_off1"),
                                                     m_themeManager->getIcon("volume_down"),
                                                     m_themeManager->getIcon("volume_up"));
        }
    }

    // ★ 追加: スピンボックスだけは「真の値 (percent)」を表示するように上書きする
    // ControlBarのソースを変更せずに外部から注入するヘルパー関数
    auto updateSpinBox = [&](ControlBar* bar) {
        if (!bar) return;

        // バーの中にあるQSpinBoxを探す
        QList<QSpinBox*> spinBoxes = bar->findChildren<QSpinBox*>();
        for (QSpinBox* sb : spinBoxes) {
            // ヒューリスティック: 直前にセットした displayVolume と同じ値になっているものが音量ボックスとみなす
            // (または objectName が "volumeSpinBox" などであれば判定精度が上がります)
            if (sb->value() == displayVolume) {
                QSignalBlocker blocker(sb); // 値を変更してもシグナルを出さないようにする(必須)

                // 上限が足りなければ拡張する (200 -> 500)
                if (sb->maximum() < 500) {
                    sb->setMaximum(500);
                }

                // 真の値をセット
                sb->setValue(percent);
            }
        }
    };

    // メインとコンパクト両方のコントロールバーに適用
    updateSpinBox(m_controlBar);
    if (m_compactWindow) {
        updateSpinBox(m_compactWindow->controlBar());
    }
}

void MainWindow::onNavFileActivated(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    qDebug() << "\n=== [Debug] onNavFileActivated ===";
    qDebug() << "  File:" << filePath;
    qDebug() << "  Current Playlist Index:" << currentPlaylistIndex;

    if (m_audioExtensions.contains(suffix) || m_videoExtensions.contains(suffix)) {
        m_nextPlayTrigger = PlayTrigger::UserAction;

        // 追加前の確認
        qDebug() << "  Requesting add to playlist...";

        // 1. マネージャに追加
        m_playlistManager->addFilesToPlaylist({filePath}, currentPlaylistIndex);

        // ログ出力のみにとどめ、スクロールなどの余計な操作は一旦排除して純粋な挙動を確認します
        qDebug() << "  Add request finished.";
    }
    else if (m_imageExtensions.contains(suffix)) {
        m_imageViewController->loadDirectory(fileInfo.dir(), filePath);
    }
}

void MainWindow::onNavFileSelected(const QString& filePath)
{
    Q_UNUSED(filePath);
}

void MainWindow::onNavPathChanged(const QString& path)
{
    Q_UNUSED(path);
}

void MainWindow::onNavStateChanged(bool canGoBack, bool canGoForward, bool canGoUp)
{
    qDebug() << "[NavStateChanged] Back:" << canGoBack
             << "| Forward:" << canGoForward
             << "| Up:" << canGoUp;

    ui->backButton->setEnabled(canGoBack);
    ui->backButton->setIcon(m_themeManager->getIcon(canGoBack ? "arrow_back" : "arrow_back_false"));
    ui->forwardButton->setEnabled(canGoForward);
    ui->forwardButton->setIcon(m_themeManager->getIcon(canGoForward ? "arrow_forward" : "arrow_forward_false"));
    ui->upButton->setEnabled(canGoUp);
    ui->upButton->setIcon(m_themeManager->getIcon(canGoUp ? "arrow_upward" : "arrow_upward_false"));
}

void MainWindow::onPlaybackShouldStop()
{
    stopMusic(); // PMから停止するよう指示があった
}

void MainWindow::onPlaylistDataChanged(int playlistIndex)
{
    qDebug() << "\n=== [Debug] onPlaylistDataChanged ===";
    qDebug() << "  Target Playlist Index:" << playlistIndex;

    // PMからデータ変更の通知があった
    updatePreviews(); // プレビューUIを更新

    if (playlistIndex < 0 || playlistIndex >= m_musicPlaylistWidget->count()) {
        qDebug() << "  [Error] Index out of range. Widget Count:" << m_musicPlaylistWidget->count();
        return;
    }

    QListWidget* listWidget = m_musicPlaylistWidget->listWidget(playlistIndex);

    if (!listWidget) {
        qDebug() << "  [Error] listWidget is NULL for index:" << playlistIndex;
        return;
    }

    qDebug() << "  ListWidget Ptr:" << listWidget;
    qDebug() << "  ListWidget Visible:" << listWidget->isVisible();
    qDebug() << "  Item Count Before Clear:" << listWidget->count();

    if (playlistIndex == m_playingPlaylistIndex && m_currentlyPlayingMusicItem && m_currentlyPlayingMusicItem->listWidget() == listWidget) {
        // clear() される前にポインタを安全に nullptr にしておく
        m_currentlyPlayingMusicItem = nullptr;
    }

    QStringList files = m_playlistManager->getPlaylist(playlistIndex); // PMから最新データを取得
    qDebug() << "  Files in Manager:" << files.count();
    if (!files.isEmpty()) {
        qDebug() << "  Last File in Manager:" << files.last();
    }

    listWidget->clear(); // UIをクリア

    // アイテム再追加
    for (const QString& filePath : files) {
        QListWidgetItem* item = new QListWidgetItem(QFileInfo(filePath).fileName());
        item->setToolTip(filePath); // ToolTipはフルパス
        listWidget->addItem(item);
    }

    qDebug() << "  Item Count After Refill:" << listWidget->count();
    if (listWidget->count() > 0) {
        qDebug() << "  Last Item Text:" << listWidget->item(listWidget->count()-1)->text();
        qDebug() << "  Last Item Rect:" << listWidget->visualItemRect(listWidget->item(listWidget->count()-1));
    }

    updatePreviews(); // プレビューも更新
    updateControlBarStates();
    updatePlaylistButtonStates();

    qDebug() << "=== [Debug] End onPlaylistDataChanged ===\n";
}

void MainWindow::onPlaylistTabChanged(int index)
{
    currentPlaylistIndex = index; // UI側のインデックスを更新
    m_playlistManager->setCurrentPlaylistIndex(index); // PMにも通知

    qDebug() << "Switched view to playlist" << index;

    updatePlaylistButtonStates();

    // --- プレビュー自動更新ロジック ---
    m_tabActivationHistory.removeAll(index);
    m_tabActivationHistory.prepend(index);

    // 2. プレビューが手動でピン留めされている場合は、自動更新を行わない
    if (!m_previewsArePinned && m_settingsManager->settings().autoUpdatePreviews) {
        // --- 自動モードのロジック ---
        // 3. 表示すべきプレイリストを決定する
        QList<int> playlistsToShow;
        for (int playlistIndex : m_tabActivationHistory) {
            // 現在表示中のタブ以外を候補とする
            if (playlistIndex != currentPlaylistIndex) {
                playlistsToShow.append(playlistIndex);
            }
            // 候補が2つ見つかったらループを抜ける
            if (playlistsToShow.size() >= 2) {
                break;
            }
        }

        // 4. プレビュースロットの割り当てを更新する
        m_previewSlotMapping[0] = playlistsToShow.value(0, -1); // 1番目の候補、なければ-1
        m_previewSlotMapping[1] = playlistsToShow.value(1, -1); // 2番目の候補、なければ-1
    }

    // 5. プレビューエリア全体を再描画する
    updatePreviews();

    if (!m_settingsManager->settings().syncUiStateAcrossLists) {
        // 個別モードの場合のみ、タブの状態をUIに反映させる
        QListWidget* currentList = m_musicPlaylistWidget->listWidget(index);
        if(currentList) {
            m_playlistOptions->setFontSize(currentList->font().pointSize());
            bool isReorderEnabled = currentList->dragDropMode() == QAbstractItemView::InternalMove;
            m_playlistOptions->setReorderEnabled(isReorderEnabled);
        }
    }
}

void MainWindow::onRecentFileTriggered()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) return;

    QString filePath = action->data().toString();
    QFileInfo fileInfo(filePath);

    if (fileInfo.exists()) {
        m_nextPlayTrigger = PlayTrigger::UserAction;
        m_playlistManager->addFilesToPlaylist({filePath}, currentPlaylistIndex); // <-- 修正
    } else {
        QMessageBox::warning(this, "ファイルが見つかりません",
                             QString("ファイルが見つかりませんでした:\n%1").arg(filePath));
        addFileToRecentList(filePath); // 不正なパスをリストから削除する
    }
}

void MainWindow::onSetItemHighlighted(QListWidgetItem* item, bool highlighted)
{
    if (highlighted) {
        setItemHighlighted(m_currentlyDisplayedSlideItem, false); // 古いものを解除
        m_currentlyDisplayedSlideItem = item; // 新しいものを記憶
    }
    setItemHighlighted(item, highlighted); // ヘルパーを呼ぶ
}

void MainWindow::onSlideshowListItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;

    QString path = item->data(Qt::UserRole).toString(); //
    bool isDir = item->data(Qt::UserRole + 1).toBool(); //

    if (isDir) {
        // 1. フォルダ（".." や "SubFolder"）の場合 (変更なし)
        //    (このリスト内でのナビゲーション)
        loadDirectoryIntoSlideshowList(QDir(path)); //
    } else {
        m_imageViewController->switchToSlideshowListMode(item);
        QString listName = "Slideshow";
        if (m_slideshowWidget) {
            listName = m_slideshowWidget->tabText(m_slideshowWidget->currentIndex());
        }
        updateBreadcrumbs(listName);
    }
}

void MainWindow::onSortComboChanged(int index, bool fromMediaView)
{
    SortMode mode = SortName;
    bool ascending = true;

    // インデックスからモードと方向を決定
    switch (index) {
    case 0: mode = SortName; ascending = true; break;
    case 1: mode = SortName; ascending = false; break;
    case 2: mode = SortDate; ascending = true; break;
    case 3: mode = SortDate; ascending = false; break;
    case 4: mode = SortSize; ascending = true; break;
    case 5: mode = SortSize; ascending = false; break;
    case 6: mode = SortShuffle; ascending = true; break; // シャッフルに方向は無関係
    default: break;
    }

    // 1. 操作元のウィジェットに適用
    if (fromMediaView) {
        m_imageViewController->setSortOrder(mode, ascending);
    } else {
        m_bookshelfWidget->setSortOrder(mode, ascending);
    }

    // 2. 同期処理
    if (m_syncSortOrder) {
        if (fromMediaView) {
            // 本棚側を同期 (シグナルをブロックしてループ防止)
            if (m_bookshelfSortComboBox) {
                m_bookshelfSortComboBox->blockSignals(true);
                m_bookshelfSortComboBox->setCurrentIndex(index);
                m_bookshelfWidget->setSortOrder(mode, ascending);
                m_bookshelfSortComboBox->blockSignals(false);
            }
        } else {
            // MediaView側を同期
            if (ui->imageSortComboBox) {
                ui->imageSortComboBox->blockSignals(true);
                ui->imageSortComboBox->setCurrentIndex(index);
                m_imageViewController->setSortOrder(mode, ascending);
                ui->imageSortComboBox->blockSignals(false);
            }
        }
    }
}

void MainWindow::onSortSyncActionToggled(bool checked)
{
    m_syncSortOrder = checked;

    // 同期をONにした瞬間、現在の状態をもう一方に適用する
    // ここではMediaView側の設定を正として本棚に適用する例
    if (m_syncSortOrder) {
        int currentIndex = ui->imageSortComboBox->currentIndex();
        onSortComboChanged(currentIndex, true);
    }
}

void MainWindow::onTabRenameRequested(int tabIndex)
{
    if (tabIndex == -1) return;

    QString currentName = m_musicPlaylistWidget->tabText(tabIndex);
    bool ok;
    QString newName = QInputDialog::getText(this,
                                            tr("プレイリスト名の変更"),
                                            tr("新しい名前:"),
                                            QLineEdit::Normal,
                                            currentName,
                                            &ok);
    if (ok && !newName.isEmpty()) {
        m_musicPlaylistWidget->setTabText(tabIndex, newName);

        // ControlBarやプレビューのラベルも更新
        onMainTabTextUpdated(tabIndex, newName);
    }
}

void MainWindow::onToggleTheme()
{
    if (m_themeManager->currentTheme() == "light") {
        applyTheme("dark");
    } else {
        applyTheme("light");
    }
}

void MainWindow::updateAllWidgetIcons()
{
    // この関数は、テーマが切り替わった際にUIを即時更新するために使う
    updateMediaViewStates();
    updateSlideshowPlayPauseButton();

    // メニューやツールバーのアクションアイコンも更新
    ui->actionPlay->setIcon(m_themeManager->getIcon("play_arrow"));
    ui->actionStop->setIcon(m_themeManager->getIcon("stop"));
    ui->actionPrev->setIcon(m_themeManager->getIcon("skip_previous"));
    ui->actionNext->setIcon(m_themeManager->getIcon("skip_next"));
    ui->actionShowFolderTree->setIcon(m_themeManager->getIcon("folder_data"));
    ui->actionShowPlaylist->setIcon(m_themeManager->getIcon("music_library"));
    ui->actionShowSlideshow->setIcon(m_themeManager->getIcon("photo_library"));
    ui->actionShowBookshelf->setIcon(m_themeManager->getIcon("bookshelf"));
    ui->backButton->setIcon(m_themeManager->getIcon("arrow_back"));
    ui->forwardButton->setIcon(m_themeManager->getIcon("arrow_forward"));
    ui->upButton->setIcon(m_themeManager->getIcon("arrow_upward"));

    ui->backButton->setIcon(m_themeManager->getIcon(
        ui->backButton->isEnabled() ? "arrow_back" : "arrow_back_false"
        ));
    ui->forwardButton->setIcon(m_themeManager->getIcon(
        ui->forwardButton->isEnabled() ? "arrow_forward" : "arrow_forward_false"
        ));
    ui->upButton->setIcon(m_themeManager->getIcon(
        ui->upButton->isEnabled() ? "arrow_upward" : "arrow_upward_false"
        ));

    QIcon textSizeIcon = m_themeManager->getIcon("text_size");
    m_playlistOptions->setIcon(textSizeIcon);
    m_slideshowOptions->setIcon(textSizeIcon);
    m_folderTreeOptions->setIcon(textSizeIcon);
    m_bookshelfOptions->setIcon(textSizeIcon);
    m_bookshelfToggleThumbnailButton->setIcon(m_themeManager->getIcon(m_bookshelfThumbnailsVisible ? "image" : "no_image"));

    if (m_bookshelfWidget) {
        QMap<QString, QIcon> icons;
        // BookshelfWidget内で "getIcon" の引数として使うキー文字列と一致させる
        icons.insert("no_image", m_themeManager->getIcon("no_image"));       // フォルダ用
        icons.insert("image", m_themeManager->getIcon("image"));             // ファイル用
        icons.insert("arrow_upward", m_themeManager->getIcon("arrow_upward")); // 親ディレクトリ用

        m_bookshelfWidget->setIcons(icons);
    }
}

void MainWindow::updateBreadcrumbs(const QString &path)
{
    if (!m_breadcrumbLayout) return;

    QLayoutItem *child;
    while ((child = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    // 2. パスを分割
    QString cleanPath = QDir::cleanPath(path);

#ifdef Q_OS_WIN
    // Windowsの場合のドライブレター対応など
    // 例: "C:/Users/Name" -> ["C:", "Users", "Name"]
    // QDir::toNativeSeparators を使うとバックスラッシュになるため注意
#endif

    QDir dir(cleanPath);
    QStringList parts = cleanPath.split('/', Qt::SkipEmptyParts);

#ifdef Q_OS_WIN
    // Windowsで "C:/" のようなルートの場合、splitの結果からドライブが抜ける場合や
    // 逆に空文字扱いになる場合への対処。
    // ここではシンプルに QDir::cleanPath したものを "/" で分割し、
    // 先頭がドライブレターでない場合はルート扱いする簡易ロジックです。
    // 必要に応じて調整してください。
#elif defined(Q_OS_UNIX)
    // Linux/Macの場合、ルート "/" が split で消えるため、先頭に追加
    if (cleanPath.startsWith('/')) {
        parts.prepend("/");
    }
#endif

    QString currentBuildPath = "";

    // 3. パスごとのボタン生成ループ
    for (int i = 0; i < parts.size(); ++i) {
        QString partName = parts[i];

        // パスの構築 (ルートの処理)
#ifdef Q_OS_WIN
        if (i == 0 && partName.contains(':')) {
            currentBuildPath = partName + "/"; // C:/
        } else {
            // 直前が "/" で終わっているか確認して結合
            if (!currentBuildPath.endsWith('/')) currentBuildPath += "/";
            currentBuildPath += partName;
        }
#else
        if (partName == "/") {
            currentBuildPath = "/";
        } else {
            if (!currentBuildPath.endsWith('/')) currentBuildPath += "/";
            currentBuildPath += partName;
        }
#endif

        // A. ディレクトリ名ボタンの追加
        QPushButton *pathBtn = createPathButton(partName, currentBuildPath);
        m_breadcrumbLayout->addWidget(pathBtn);

        // B. セパレータ「>」ボタンの追加
        // この「>」を押すと、currentBuildPath (つまり今ボタンを作ったフォルダ) の中身を表示する
        QToolButton *separatorBtn = createSeparatorButton(currentBuildPath);
        m_breadcrumbLayout->addWidget(separatorBtn);
    }

    // レイアウトの右側にスペーサーを入れて左寄せにする
    m_breadcrumbLayout->addStretch(1);
}

void MainWindow::updateControlBarStates()
{
    // --- 再生状態の更新 ---
    bool isPlaying = m_mediaManager->isPlaying();
    bool isPaused = m_mediaManager->isPaused(); // 一時停止状態も取得
    bool hasActiveMedia = isPlaying || isPaused;

    m_controlBar->updatePlayPauseIcon(isPlaying, m_themeManager->getIcon("play_arrow"), m_themeManager->getIcon("pause"));
    m_compactWindow->controlBar()->updatePlayPauseIcon(isPlaying, m_themeManager->getIcon("play_arrow"), m_themeManager->getIcon("pause"));
    ui->actionPlay->setText(isPlaying ? "一時停止(&P)" : "再生(&P)");
    ui->actionPlay->setIcon(m_themeManager->getIcon(isPlaying ? "pause" : "play_arrow"));
    m_controlBar->setSeekEnabled(hasActiveMedia);
    m_compactWindow->controlBar()->setSeekEnabled(hasActiveMedia);

    // --- 曲情報の更新 ---
    QString trackInfoText = "Track Info";
    if (isPlaying || m_mediaManager->isPaused()) {
        if (!m_currentTrackTitle.isEmpty()) {
            trackInfoText = m_currentTrackTitle;
        }
    }
    // 両方のControlBarを更新
    m_controlBar->setTrackInfo(trackInfoText);
    m_compactWindow->controlBar()->setTrackInfo(trackInfoText);

    bool canGoNext = m_playlistManager->canGoNext();
    bool canGoPrev = m_playlistManager->canGoPrevious();
    m_controlBar->updateButtonStates(canGoNext, canGoPrev, m_themeManager->getIcon("skip_next"), m_themeManager->getIcon("skip_previous"), m_themeManager->getIcon("stop"));
    m_compactWindow->controlBar()->updateButtonStates(canGoNext, canGoPrev, m_themeManager->getIcon("skip_next"), m_themeManager->getIcon("skip_previous"), m_themeManager->getIcon("stop"));
    // --- リピート・シャッフルアイコンの更新 ---
    LoopMode loopMode = m_playlistManager->getLoopMode();
    ShuffleMode shuffleMode = m_playlistManager->getShuffleMode();
    m_controlBar->updateRepeatIcon((ControlBar::LoopMode)loopMode, m_themeManager->getIcon("repeat"), m_themeManager->getIcon("repeat_one"), m_themeManager->getIcon("repeat_on"));
    m_compactWindow->controlBar()->updateRepeatIcon((ControlBar::LoopMode)loopMode, m_themeManager->getIcon("repeat"), m_themeManager->getIcon("repeat_one"), m_themeManager->getIcon("repeat_on"));
    m_controlBar->updateShuffleIcon((ControlBar::ShuffleMode)shuffleMode, m_themeManager->getIcon("shuffle"), m_themeManager->getIcon("shuffle_on"), m_themeManager->getIcon("random"));
    m_compactWindow->controlBar()->updateShuffleIcon((ControlBar::ShuffleMode)shuffleMode, m_themeManager->getIcon("shuffle"), m_themeManager->getIcon("shuffle_on"), m_themeManager->getIcon("random"));
    ui->actionAlwaysOnTop->setIcon(m_themeManager->getIcon(m_isAlwaysOnTop ? "pin" : "pin_off"));
    ui->actionAlwaysOnTop->setToolTip(m_isAlwaysOnTop ? "常に最前面で表示(有効)" : "常に最前面で表示(無効)");

    ui->actionAlwaysOnTop->setChecked(m_isAlwaysOnTop);

    int currentVolume = m_controlBar->volume();
    bool isMuted = false;
    if (m_mediaManager) {
        isMuted = m_mediaManager->isMuted();
        onMediaVolumeChanged(currentVolume, isMuted);
    }
}

void MainWindow::updateDockWidgetBehavior()
{
    // Dockウィジェットのリスト
    QList<QDockWidget*> docks = { m_folderTreeDock, m_playlistDock, m_slideshowDock, m_bookshelfDock};

    if (m_settingsManager->settings().docksAreLocked) {
        // --- ロックが有効な場合 ---
        // 各Dockが左側のエリアにのみドッキングできるように制限する
        for (auto* dock : docks) {
            dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

            dock->setTitleBarWidget(new QWidget());
        }
    } else {
        // --- ロックが無効な場合 ---
        // 全てのエリア（上下左右）にドッキングできるように許可する
        for (auto* dock : docks) {
            dock->setAllowedAreas(Qt::AllDockWidgetAreas);
            dock->setTitleBarWidget(nullptr);
        }
    }
}

void MainWindow::updateFullScreenUi()
{
    syncFloatingControlBarPosition();
}

void MainWindow::updateMediaViewStates()
{
    // --- IVCから状態を取得 ---
    SlideshowMode slideshowMode = m_imageViewController->getSlideshowMode();
    FitMode fitMode = m_imageViewController->getFitMode();
    LayoutDirection layoutDirection = m_imageViewController->getLayoutDirection();
    SlideDirection slideDirection = m_imageViewController->getSlideDirection();

    bool isPanorama = (slideshowMode == ModePictureScroll);

    QString currentFirstItem = ui->slideshowEffectComboBox->itemText(0);
    QString expectedFirstItem = isPanorama ? "スライド" : "なし";

    if (currentFirstItem != expectedFirstItem) {
        ui->slideshowEffectComboBox->blockSignals(true);
        ui->slideshowEffectComboBox->clear();

        if (isPanorama) {
            // パノラマモード: [スライド(ID=2), なし(ID=0)]
            // ★ 第2引数に ID を追加
            ui->slideshowEffectComboBox->addItem("スライド", 2);
            ui->slideshowEffectComboBox->addItem("なし", 0);
        } else {
            // 通常モード: [なし(ID=0), フェード(ID=1)]
            // ★ 第2引数に ID を追加
            ui->slideshowEffectComboBox->addItem("なし", 0);
            ui->slideshowEffectComboBox->addItem("フェード", 1);
        }

        // デフォルト値(0番目)を選択
        ui->slideshowEffectComboBox->setCurrentIndex(0);
        ui->slideshowEffectComboBox->blockSignals(false);

        // 新しいデフォルト効果を適用 (ここも currentData を使うように修正)
        int defaultEffectId = ui->slideshowEffectComboBox->currentData().toInt();
        m_imageViewController->setSlideshowEffect(defaultEffectId);
    }

    ui->togglePanoramaButton->setIcon(m_themeManager->getIcon(slideshowMode == ModePictureScroll ? "panorama" : "panorama_off"));

    // --- フィットモード切替ボタン (toggleFitModeButton) ---
    if (slideshowMode == ModePictureScroll) {
        if (fitMode == FitToHeight) {
            ui->toggleFitModeButton->setToolTip("レイアウト: 水平");
            ui->toggleFitModeButton->setIcon(m_themeManager->getIcon("panorama_horizontal"));
        } else { // FitToWidth
            ui->toggleFitModeButton->setToolTip("レイアウト: 垂直");
            ui->toggleFitModeButton->setIcon(m_themeManager->getIcon("panorama_vertical"));
        }
    } else {
        // 標準モードの時は、フィットモードに応じたツールチップに
        switch (fitMode) {
        case FitInside:
            ui->toggleFitModeButton->setToolTip("全体表示");
            ui->toggleFitModeButton->setIcon(m_themeManager->getIcon("image_inset"));
            break;
        case FitToWidth:
            ui->toggleFitModeButton->setToolTip("幅合わせ");
            ui->toggleFitModeButton->setIcon(m_themeManager->getIcon("panorama_vertical"));
            break;
        case FitToHeight:
            ui->toggleFitModeButton->setToolTip("高さ合わせ");
            ui->toggleFitModeButton->setIcon(m_themeManager->getIcon("panorama_horizontal"));
            break;
        }
    }

    // --- 配置方向ボタン (toggleLayoutDirectionButton) ---
    if (slideshowMode == ModePictureScroll) {
        ui->toggleLayoutDirectionButton->setVisible(true);
        if (slideDirection == DirectionHorizontal) {
            if (layoutDirection == LayoutDirection::Forward){
                ui->toggleLayoutDirectionButton->setIcon(m_themeManager->getIcon("arrow_right"));
            } else { // Backward
                ui->toggleLayoutDirectionButton->setIcon(m_themeManager->getIcon("arrow_left"));
            }
        } else { // 縦並び
            if (layoutDirection == LayoutDirection::Forward) {
                ui->toggleLayoutDirectionButton->setIcon(m_themeManager->getIcon("arrow_downward"));
            } else { // Backward
                ui->toggleLayoutDirectionButton->setIcon(m_themeManager->getIcon("arrow_upward"));
            }
        }
    } else {
        ui->toggleLayoutDirectionButton->setVisible(false);
    }

    bool isInverted = (layoutDirection == LayoutDirection::Backward);

    // 1. レイアウト方向の切り替え（これでハンドルの位置が 0=右端 になります）
    Qt::LayoutDirection dir = isInverted ? Qt::RightToLeft : Qt::LeftToRight;
    ui->viewControlSlider->setLayoutDirection(dir);
    ui->slideshowProgressBar->setLayoutDirection(dir);

    // 2. ★復活: スタイルシート切り替え用のプロパティを設定（これで色が反転します）
    ui->viewControlSlider->setProperty("inverted", isInverted);

    // 3. スタイルの再適用
    ui->viewControlSlider->style()->unpolish(ui->viewControlSlider);
    ui->viewControlSlider->style()->polish(ui->viewControlSlider);
}

void MainWindow::updatePlaylistButtonStates()
{
    int count = m_musicPlaylistWidget->count();

    for (int i = 0; i < count; ++i) {
        QListWidget* list = m_musicPlaylistWidget->listWidget(i);
        if (!list) continue;

        int itemCount = list->count();
        bool isEmpty = (itemCount == 0);
        bool isPlayingThisPlaylist = (m_playingPlaylistIndex == i);

        // ツールチップのテキスト生成
        QString tooltip;
        if (isPlayingThisPlaylist) {
            // 再生中: "再生中: 3 / 10" のような形式
            // 現在の行を取得。もし選択行がなければ0とするが、再生中なら通常はあるはず
            int currentIndex = list->currentRow() + 1;
            tooltip = QString("再生中: %1 / %2").arg(currentIndex).arg(itemCount);
        } else {
            // 非再生中: "10曲" または "空"
            if (isEmpty) {
                tooltip = "空のプレイリスト";
            } else {
                tooltip = QString("待機中: %1曲").arg(itemCount);
            }
        }

        // ControlBarに反映
        m_controlBar->setPlaylistButtonState(i, isEmpty, isPlayingThisPlaylist, tooltip);

        // CompactWindowがある場合も反映
        if (m_compactWindow && m_compactWindow->controlBar()) {
            m_compactWindow->controlBar()->setPlaylistButtonState(i, isEmpty, isPlayingThisPlaylist, tooltip);
        }
    }
}

void MainWindow::updatePreviews()
{
    // まず、全てのプレビューグループを非表示にする
    for (QWidget* group : previewGroups) {
        group->hide();
    }

    int maxCount = m_musicPlaylistWidget->count();
    if (m_previewSlotMapping[0] >= maxCount) m_previewSlotMapping[0] = -1;
    if (m_previewSlotMapping[1] >= maxCount) m_previewSlotMapping[1] = -1;

    // プレビューが1つでも表示されたかを追跡するフラグ
    bool anyPreviewVisible = false;

    // --- 1番目のプレビュースロットの更新 ---
    int playlistIndex1 = m_previewSlotMapping[0];
    if (playlistIndex1 != -1) { // PMはサイズチェック不要
        previewLabels[0]->setText(m_musicPlaylistWidget->tabText(playlistIndex1));
        QListWidget* previewList = previewListWidgets[0];
        previewList->clear();
        // --- PMからデータを取得 ---
        QStringList playlist = m_playlistManager->getPlaylist(playlistIndex1);
        for (const QString& filePath : playlist) {
            previewList->addItem(QFileInfo(filePath).fileName());
        }
        previewGroups[0]->show();

        anyPreviewVisible = true;
    }

    int playlistIndex2 = m_previewSlotMapping[1];
    if (playlistIndex2 != -1) {
        previewLabels[1]->setText(m_musicPlaylistWidget->tabText(playlistIndex2));
        QListWidget* previewList = previewListWidgets[1];
        previewList->clear();
        // --- PMからデータを取得 ---
        QStringList playlist = m_playlistManager->getPlaylist(playlistIndex2);
        for (const QString& filePath : playlist) {
            previewList->addItem(QFileInfo(filePath).fileName());
        }
        previewGroups[1]->show();

        anyPreviewVisible = true;
    }

    // 3番目のプレビュースロットは永久に非表示にする
    previewGroups[2]->hide();

    // プレビューが1つでも表示されていればコンテナを表示し、
    // 0件ならコンテナごと非表示にする（スプリッターが最大化される）
    if (anyPreviewVisible) {
        previewContainer->show();
    } else {
        previewContainer->hide();
    }
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear(); // 既存の項目をクリア
    m_recentFileActions.clear();

    // 1. 設定マネージャから最新の履歴を取得
    QStringList recentFiles = m_settingsManager->recentFiles();

    // ★ 修正 1: ショートカットキー用にメンバ変数も更新しておく
    m_recentFiles = recentFiles;

    if (recentFiles.isEmpty()) {
        m_recentFilesMenu->setEnabled(false);
        return;
    }

    m_recentFilesMenu->setEnabled(true);

    // ★ 修正 2: ループの対象を m_recentFiles ではなく、取得した recentFiles (または更新後の m_recentFiles) にする
    for (int i = 0; i < recentFiles.size(); ++i) {
        const QString& filePath = recentFiles.at(i);
        QFileInfo fileInfo(filePath);

        // ショートカットキー用の番号 (1-9, 0)
        int num = (i < 9) ? (i + 1) : 0;
        QString text = QString("&%1. %2").arg(num).arg(fileInfo.fileName());

        QAction *action = m_recentFilesMenu->addAction(text);
        action->setData(filePath); // アクションにフルパスを保存
        connect(action, &QAction::triggered, this, &MainWindow::onRecentFileTriggered);
        m_recentFileActions.append(action);
    }

    m_recentFilesMenu->addSeparator();
    QAction *clearAction = m_recentFilesMenu->addAction("クリア(&C)");
    connect(clearAction, &QAction::triggered, this, &MainWindow::clearRecentFiles);
}

void MainWindow::updateSlideshowPlayPauseButton()
{
    if (m_imageViewController->isSlideshowActive()) {
        ui->slideshowPlayPauseButton->setIcon(m_themeManager->getIcon("slideshow_pause"));
        ui->slideshowPlayPauseButton->setToolTip("スライドショー停止");
    } else {
        ui->slideshowPlayPauseButton->setIcon(m_themeManager->getIcon("slideshow_play"));
        ui->slideshowPlayPauseButton->setToolTip("スライドショー再生");
    }
}

void MainWindow::updateTitleBarStyle()
{
    // if (!m_titleBarWidget) return;

    // QString theme = m_themeManager->currentTheme();
    // bool isDark = (theme == "dark");


    // // 2. ウィンドウ操作ボタンのアイコン色変更
    // QColor iconColor = isDark ? Qt::white : Qt::black;

    // m_minBtn->setIcon(getColorizedIcon(QStyle::SP_TitleBarMinButton, iconColor));

    // if (isMaximized()) {
    //     m_maxBtn->setIcon(getColorizedIcon(QStyle::SP_TitleBarNormalButton, iconColor));
    // } else {
    //     m_maxBtn->setIcon(getColorizedIcon(QStyle::SP_TitleBarMaxButton, iconColor));
    // }

    // m_closeBtn->setIcon(getColorizedIcon(QStyle::SP_TitleBarCloseButton, iconColor));
}

void MainWindow::addFilesFromExplorer(const QStringList &filePaths, bool isColdStart)
{
    QList<QUrl> urls;
    for (const QString &path : filePaths) {
        urls.append(QUrl::fromLocalFile(path));
    }

    if (urls.isEmpty()) return;

    // ファイルの種類を判定 (先頭のファイルで判断)
    QString firstPath = filePaths.first();
    QFileInfo fi(firstPath);
    QString suffix = fi.suffix().toLower();

    // --- A. 画像ファイルの場合 ---
    if (m_imageExtensions.contains(suffix)) {
        // 1. 本棚を参照ディレクトリに移動
        m_bookshelfWidget->navigateToPath(fi.absolutePath());

        // 2. そのファイルのインデックスへ移動 (Bookshelf移動シグナルとは別に明示的にロード)
        m_imageViewController->loadDirectory(fi.dir(), firstPath);

        // 3. ページをメディアページへ
        ui->mediaStackedWidget->setCurrentWidget(ui->mediaPage);

        this->activateWindow();
        this->raise();
        return; // 画像の場合はここで終了（プレイリスト追加はしない）
    }

    // --- B. 音声・動画ファイルの場合 ---

    // フラグ設定
    m_autoPlayNextScan = true;
    m_nextPlayTrigger = PlayTrigger::OpenFile;

    // 動画かつ起動時(Cold Start)なら、ビデオページへ切り替える
    if (m_videoExtensions.contains(suffix)) {
        if (isColdStart) {
            ui->mediaStackedWidget->setCurrentWidget(ui->videoPage);
        }
        // 起動済み(Warm Start)ならページ切り替えしない
    } else {
        // 音声ならメディアページが基本
        // (ユーザーの好みによるが、一旦既存の挙動またはMediaPageへ)
        if (isColdStart) {
            ui->mediaStackedWidget->setCurrentWidget(ui->mediaPage);
        }
    }

    // スキャン開始 (scanFinished で m_autoPlayNextScan を見て再生される)
    startFileScan(urls, -1);

    this->activateWindow();
    this->raise();
}

void MainWindow::addFilesToSlideshowPlaylist(const QStringList &files)
{
    if (m_slideshowWidget) {
        m_slideshowWidget->addFilesToCurrentList(files);
    }
}

void MainWindow::addFileToRecentList(const QString& filePath)
{
    // マネージャから現在のリストを取得
    QStringList recentFiles = m_settingsManager->recentFiles();

    recentFiles.removeAll(filePath);
    recentFiles.prepend(filePath);
    while (recentFiles.size() > 10) {
        recentFiles.removeLast();
    }

    // マネージャ経由で保存
    m_settingsManager->saveRecentFiles(recentFiles);

    // UI更新 (必要であれば)
    updateRecentFilesMenu();
}

void MainWindow::appendPreviewMenuActions(QMenu* menu)
{
    // --- サブメニューとして「プレビュー設定」を追加 ---
    QMenu* previewMenu = menu->addMenu("プレビュー設定");

    // 1. ピン留めアクション
    QAction *pinAction = previewMenu->addAction("ピン留めして表示を固定");
    pinAction->setCheckable(true);
    pinAction->setChecked(m_previewsArePinned);

    connect(pinAction, &QAction::toggled, this, [this](bool checked){
        m_previewsArePinned = checked;
        if (!m_previewsArePinned) {
            // 自動モードに戻った瞬間、表示を更新
            onPlaylistTabChanged(currentPlaylistIndex);
        }
    });

    previewMenu->addSeparator();

    // 2. スロット1設定
    QMenu *slot1Menu = previewMenu->addMenu("スロット1に表示するリスト");
    QActionGroup *slot1Group = new QActionGroup(this);

    QAction *slot1None = slot1Menu->addAction("なし (非表示)");
    slot1None->setCheckable(true);
    slot1None->setData(-1);
    if (m_previewSlotMapping[0] == -1) slot1None->setChecked(true);
    slot1Group->addAction(slot1None);

    for (int i = 0; i < m_musicPlaylistWidget->count(); ++i) {
        QAction *a = slot1Menu->addAction(m_musicPlaylistWidget->tabText(i));
        a->setCheckable(true);
        a->setData(i);
        if (m_previewSlotMapping[0] == i) a->setChecked(true);
        slot1Group->addAction(a);
    }

    connect(slot1Group, &QActionGroup::triggered, this, [this](QAction* action){
        int index = action->data().toInt();
        if (index != -1 && index == m_previewSlotMapping[1]) m_previewSlotMapping[1] = -1;
        m_previewSlotMapping[0] = index;
        m_previewsArePinned = true; // 手動変更したらピン留め有効化
        updatePreviews();
    });

    // 3. スロット2設定
    QMenu *slot2Menu = previewMenu->addMenu("スロット2に表示するリスト");
    QActionGroup *slot2Group = new QActionGroup(this);

    QAction *slot2None = slot2Menu->addAction("なし (非表示)");
    slot2None->setCheckable(true);
    slot2None->setData(-1);
    if (m_previewSlotMapping[1] == -1) slot2None->setChecked(true);
    slot2Group->addAction(slot2None);

    for (int i = 0; i < m_musicPlaylistWidget->count(); ++i) {
        QAction *a = slot2Menu->addAction(m_musicPlaylistWidget->tabText(i));
        a->setCheckable(true);
        a->setData(i);
        if (m_previewSlotMapping[1] == i) a->setChecked(true);
        slot2Group->addAction(a);
    }

    connect(slot2Group, &QActionGroup::triggered, this, [this](QAction* action){
        int index = action->data().toInt();
        if (index != -1 && index == m_previewSlotMapping[0]) m_previewSlotMapping[0] = -1;
        m_previewSlotMapping[1] = index;
        m_previewsArePinned = true;
        updatePreviews();
    });
}

void MainWindow::applySettings(const AppSettings& newSettings)
{
    // テーマ変更チェック
    if (newSettings.theme != m_themeManager->currentTheme()) {
        applyTheme(newSettings.theme);
    }

    // レジストリ設定変更チェック
    // マネージャ経由で現在の値と比較
    if (newSettings.contextMenuEnabled != m_settingsManager->settings().contextMenuEnabled) {
        m_settingsManager->updateRegistrySettings(newSettings.contextMenuEnabled);
    }

    bool oldSyncState = m_settingsManager->settings().syncUiStateAcrossLists;

    // マネージャの設定を更新
    m_settingsManager->settings() = newSettings;

    updateDockWidgetBehavior();
    if (m_settingsManager->settings().syncUiStateAcrossLists && !oldSyncState) {
        syncAllListsUiState(m_playlistOptions->value(), m_playlistOptions->isChecked());
    }

    // マネージャに保存させる
    m_settingsManager->saveSettings();

    qDebug() << "Settings applied and saved.";
}

void MainWindow::applySortMode(int index, bool fromMediaView)
{
    // index を SortMode に変換 (0:Name, 1:Date, 2:Size, 3:Shuffle)
    SortMode mode = static_cast<SortMode>(index);

    // 昇順・降順のデフォルトルール
    // 名前は昇順、日付/サイズは降順(新しい/大きい順)が一般的
    bool ascending = true;
    if (mode == SortDate || mode == SortSize) {
        ascending = false;
    }

    // 1. トリガー元の反映
    if (fromMediaView) {
        m_imageViewController->setSortOrder(mode, ascending);
    } else {
        m_bookshelfWidget->setSortOrder(mode, ascending);
    }

    // 2. 同期設定が有効なら、もう片方も更新
    if (m_syncSortOrder) {
        // シグナルループを防ぐため、blockSignalsを使用
        if (fromMediaView) {
            m_bookshelfSortComboBox->blockSignals(true);
            m_bookshelfSortComboBox->setCurrentIndex(index);
            m_bookshelfWidget->setSortOrder(mode, ascending);
            m_bookshelfSortComboBox->blockSignals(false);
        } else {
            ui->imageSortComboBox->blockSignals(true);
            ui->imageSortComboBox->setCurrentIndex(index);
            m_imageViewController->setSortOrder(mode, ascending);
            ui->imageSortComboBox->blockSignals(false);
        }
    }
}

void MainWindow::applyTheme(const QString& themeName)
{
    m_themeManager->applyTheme(themeName);

    // タイトルバーの更新
    updateTitleBarStyle();

    // アイコン更新など既存処理...
    updateAllWidgetIcons();
    updateControlBarStates();
}

void MainWindow::applyUiStateToList(QListWidget* list, int fontSize, bool reorderEnabled)
{
    if (!list) return;

    // 一般的なリスト（プレイリスト等）向けの共通設定のみを残します
    list->setAcceptDrops(true);

    QFont font = list->font();
    font.setPointSize(fontSize);
    list->setFont(font);

    // 並び替えが有効なら InternalMove、無効なら NoDragDrop
    // (外部からのドロップは setAcceptDrops(true) により許可されます)
    list->setDragDropMode(reorderEnabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
}

void MainWindow::clearRecentFiles()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("履歴のクリア");
    msgBox.setText("最近アクセスしたメディアの履歴をすべて削除しますか？");
    msgBox.setIcon(QMessageBox::Question);

    // 2. ボタンを追加し、テキストを変更してショートカットキー (&Y, &N) を割り当てる
    QPushButton *yesButton = msgBox.addButton("はい(&Y)", QMessageBox::YesRole);
    QPushButton *noButton = msgBox.addButton("いいえ(&N)", QMessageBox::NoRole);

    // 3. デフォルトで選択されるボタンを「いいえ」に設定
    msgBox.setDefaultButton(noButton);

    // 4. ボタンの最小幅を指定してサイズを大きくする
    msgBox.setStyleSheet("QPushButton { min-width: 90px; }");

    // 5. ダイアログを表示し、押されたボタンを判断する
    msgBox.exec();

    if (msgBox.clickedButton() == yesButton) {
        m_settingsManager->saveRecentFiles(QStringList());
        updateRecentFilesMenu();
    }
}

QPushButton* MainWindow::createPathButton(const QString &name, const QString &targetPath)
{
    QPushButton *btn = new QPushButton(name, this);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);

    // ★修正: スタイルシートでホバー時の色（青紫色）を指定
    btn->setStyleSheet(
        "QPushButton { "
        "  border: none; "
        "  padding: 0px 4px; "
        "  text-align: left; "
        "  font-weight: bold; "
        "  max-height: 24px; " // ツールバーの高さに合わせる
        "}"
        "QPushButton:hover { "
        "  text-decoration: underline; "
        "  color: #8A2BE2; " // BlueViolet (青紫色)
        "}"
        );

    // 左クリック: ディレクトリ移動
    connect(btn, &QPushButton::clicked, this, [this, targetPath](){
        // 本棚の移動関数などを呼び出す
        m_bookshelfWidget->navigateToPath(targetPath);
        // もし ImageViewController 側も連動させるなら:
        // m_imageViewController->browseTo(targetPath, QString(), true);
    });

    // ★追加: 右クリック（コンテキストメニュー）でパスをコピー
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QWidget::customContextMenuRequested, this, [targetPath](const QPoint &pos){
        QMenu menu;
        QAction *copyAction = menu.addAction("パスをコピー");

        // メニューを実行し、選択されたらコピー処理を行う
        // execはブロッキング呼び出しなので、結果をActionポインタで受け取れます
        QAction *selected = menu.exec(QCursor::pos());
        if (selected == copyAction) {
            QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(targetPath));
        }
    });

    return btn;
}

QToolButton* MainWindow::createSeparatorButton(const QString &targetPath)
{
    QToolButton *btn = new QToolButton(this);
    btn->setText(">");
    btn->setPopupMode(QToolButton::InstantPopup); // 左クリックですぐメニュー表示
    btn->setAutoRaise(true);
    btn->setCursor(Qt::PointingHandCursor);

    // ★修正: スタイルシートでホバー時の背景色（うっすら青紫）と文字色を指定
    btn->setStyleSheet(
        "QToolButton::menu-indicator { image: none; }" // ▼マークを消す
        "QToolButton { "
        "  border: none; "
        "  padding: 0px; "
        "  font-weight: bold; "
        "  color: #888; "
        "  max-height: 24px; "
        "}"
        "QToolButton:hover { "
        "  color: #8A2BE2; " // BlueViolet (青紫色)
        "  background-color: rgba(138, 43, 226, 30); " // うっすらハイライト (Alpha 30)
        "  border-radius: 3px; "
        "}"
        );

    // 左クリック用メニュー (サブフォルダ一覧) の作成
    QMenu *menu = new QMenu(btn);
    btn->setMenu(menu);

    // 遅延読み込みでサブフォルダを追加
    connect(menu, &QMenu::aboutToShow, this, [this, menu, targetPath](){
        menu->clear();
        QDir dir(targetPath);
        QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

        if (subDirs.isEmpty()) {
            QAction *emptyAction = menu->addAction("(空のフォルダ)");
            emptyAction->setEnabled(false);
            return;
        }

        for (const QFileInfo &info : subDirs) {
            QAction *action = menu->addAction(info.fileName());
            QString subPath = info.absoluteFilePath();
            connect(action, &QAction::triggered, this, [this, subPath](){
                m_bookshelfWidget->navigateToPath(subPath);
            });
        }
    });

    // ★追加: 右クリック（コンテキストメニュー）でパスをコピー
    // QToolButtonの右クリックは InstantPopup モードでも customContextMenuRequested で拾えます
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QWidget::customContextMenuRequested, this, [targetPath](const QPoint &pos){
        QMenu contextMenu;
        QAction *copyAction = contextMenu.addAction("パスをコピー");

        QAction *selected = contextMenu.exec(QCursor::pos());
        if (selected == copyAction) {
            QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(targetPath));
        }
    });

    return btn;
}

void MainWindow::deleteSelectedSlideshowItems(QListWidget* listWidget)
{
    handleSlideshowItemsDeletion(listWidget);
}

Qt::Edges MainWindow::edgesFromPos(const QPoint &pos, int margin) const
{
    // Qt::Edges edges = Qt::Edges();

    // // ウィンドウ領域
    // QRect rect = this->rect();

    // if (pos.x() < rect.left() + margin)   edges |= Qt::LeftEdge;
    // if (pos.x() > rect.right() - margin)  edges |= Qt::RightEdge;
    // if (pos.y() < rect.top() + margin)    edges |= Qt::TopEdge;
    // if (pos.y() > rect.bottom() - margin) edges |= Qt::BottomEdge;

    // return edges;
}

QWidget* MainWindow::getActiveWindow() const
{
    if (m_compactWindow && m_compactWindow->isVisible()) {
        return m_compactWindow;
    }
    return const_cast<MainWindow*>(this);
}


QList<QListWidget*> MainWindow::getAllListWidgets() const
{
    QList<QListWidget*> allLists;
    // 音楽プレイリストを追加
    if (m_musicPlaylistWidget) {
        allLists.append(m_musicPlaylistWidget->allListWidgets());
    }

    // m_slideshowWidget からリストを取得
    if (m_slideshowWidget) {
        allLists.append(m_slideshowWidget->allListWidgets());
    }

    return allLists;
}

QString MainWindow::getListWidgetStyle() const
{
    return R"(
        /* QListWidgetのアイテムが選択された時のスタイル */
        QListWidget::item:selected {
            background-color: #ff69b4;
            color: white;
        }
        QListWidget::item:selected:!active {
            background-color: #ffb6c1;
        }
        QListWidget::item:hover {
            background-color: #ffc0cb;
        }
    )";
}

QString MainWindow::getButtonStyles() const
{
    return R"(
        /* ツールバー内のボタンスタイル */
        QToolBar QToolButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 3px;
            margin: 0px;
            padding: 2px; /* ★ ここでパディングを固定 */
        }

        /* チェック状態（ONの状態） */
        QToolBar QToolButton:checked {
            background-color: transparent; /* 背景を変えないならtransparent、変えるなら色指定 */
            border: 1px solid transparent; /* 枠線なし */

            /* ★ 重要: 通常時と同じパディングを指定して、アイコンのズレ（凹み動作）を無効化 */
            padding: 2px;
        }

        /* マウスで押している最中 */
        QToolBar QToolButton:pressed {
            background-color: transparent;
            border: 1px solid transparent;

            /* ★ 重要: 押下時もズレを防止 */
            padding: 2px;
        }

        /* ホバー時 (オプション) */
        QToolBar QToolButton:hover {
            background-color: rgba(128, 128, 128, 0.2);
            border: 1px solid transparent;
            padding: 2px; /* 念のためここも固定 */
        }

        /* 一般的なプッシュボタン */
        QPushButton:checked {
            background-color: #ffb6c1;
            border: 1px solid #adadad;
        }
    )";
}

QIcon MainWindow::getColorizedIcon(QStyle::StandardPixmap sp, const QColor &color)
{
    // // 標準アイコンを取得
    // QIcon icon = style()->standardIcon(sp);

    // // ★修正: サイズを 24x24 -> 16x16 (または12x12) に変更
    // // これによりアイコンの線が太くなりすぎず、Windows標準に近くなります
    // QPixmap pix = icon.pixmap(16, 16);

    // // 色を塗り替える
    // QPainter p(&pix);
    // p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    // p.fillRect(pix.rect(), color);
    // p.end();

    // return QIcon(pix);
}

void MainWindow::hideFullScreenControls()
{
    if (!m_isVideoFullScreen || !m_fullScreenContainer) return;

    if (m_fullScreenContainer->underMouse()) { // コンテナ上のマウス判定
        m_mouseInactivityTimer->start(1000);
        return;
    }

    this->setCursor(Qt::BlankCursor);

    m_controlBarFadeAnimation->stop();
    m_controlBarFadeAnimation->setDuration(500);
    m_controlBarFadeAnimation->setStartValue(m_fullScreenContainer->windowOpacity());
    m_controlBarFadeAnimation->setEndValue(0.0);
    m_controlBarFadeAnimation->start();
}

bool MainWindow::isMusicPlaylist(QListWidget* listWidget) const
{
    if (!listWidget) return false;

    // メンバ変数 playlistWidgets (音楽プレイリストのリスト) に含まれているか
    return m_musicPlaylistWidget->allListWidgets().contains(listWidget);
}

bool MainWindow::isSlideshowList(QListWidget* listWidget) const
{
    if (!listWidget || !m_slideshowWidget) return false;

    // SlideshowWidgetが管理しているリストの中に含まれているか確認
    return m_slideshowWidget->allListWidgets().contains(listWidget);
}

void MainWindow::loadDirectoryIntoSlideshowList(const QDir& dir, const QString& fileToSelectPath)
{
    // 1. 現在アクティブな QListWidget を取得
    if (!m_slideshowWidget) return;

    QListWidget* currentList = m_slideshowWidget->currentListWidget();
    if (!currentList) return;

    // 2. リストをクリア
    currentList->clear();

    // 3. 現在のディレクトリパスをタブのツールチップに保存（NeeView風）
    if (m_slideshowWidget->currentWidget()) {
        m_slideshowWidget->currentWidget()->setToolTip(dir.path());
    }

    // 4. ".." (親ディレクトリ) を追加 (ルートでなければ)
    QDir parentDir = dir;
    if (parentDir.cdUp()) {
        QListWidgetItem* parentItem = new QListWidgetItem("..");
        parentItem->setData(Qt::UserRole, parentDir.path()); // フルパス
        parentItem->setData(Qt::UserRole + 1, true);         // ディレクトリフラグ
        // TODO: getThemedIcon("folder") などでアイコンを設定
        currentList->addItem(parentItem);
    }

    // 5. サブディレクトリをリストに追加
    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::LocaleAware);
    for (const QString& dirName : subDirs) {
        QListWidgetItem* item = new QListWidgetItem(dirName);
        item->setData(Qt::UserRole, dir.filePath(dirName)); // フルパス
        item->setData(Qt::UserRole + 1, true);            // ディレクトリフラグ
        // TODO: getThemedIcon("folder") などでアイコンを設定
        currentList->addItem(item);
    }

    // 6. 画像ファイルをリストに追加
    QStringList nameFilters;
    for (const QString& ext : m_imageExtensions) {
        nameFilters << "*." + ext;
    }
    QStringList imageFiles = dir.entryList(nameFilters, QDir::Files, QDir::Name | QDir::LocaleAware);

    int fileToSelectIndex = -1;
    QString firstImagePath;
    for (int i = 0; i < imageFiles.size(); ++i) {
        const QString& fileName = imageFiles.at(i);
        QString fullPath = dir.filePath(fileName);

        if (firstImagePath.isEmpty()) {
            firstImagePath = fullPath;
        }

        QListWidgetItem* item = new QListWidgetItem(fileName);
        item->setData(Qt::UserRole, fullPath);
        item->setData(Qt::UserRole + 1, false);
        currentList->addItem(item);

        if (fullPath == fileToSelectPath) {
            fileToSelectIndex = currentList->count() - 1;
        }
    }

    m_imageViewController->onSlideshowPlaylistChanged();

    if (!fileToSelectPath.isEmpty() && fileToSelectIndex != -1) {
        m_imageViewController->displayMedia(fileToSelectPath);
        currentList->setCurrentRow(fileToSelectIndex);
    } else if (fileToSelectPath.isEmpty() && !firstImagePath.isEmpty()) {
        m_imageViewController->displayMedia(firstImagePath);
        int firstImageIndex = (currentList->count() - imageFiles.size());
        currentList->setCurrentRow(firstImageIndex);
    }
}

void MainWindow::loadSlideshowListFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    if (in.readLine() != "#QSLIDESHOWLIST") { // ヘッダーチェック
        file.close();
        return;
    }

    QStringList loadedFiles;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty()) loadedFiles.append(line);
    }
    file.close();

    if (loadedFiles.isEmpty()) return;

    // D&Dの場合は常に「追加」モードで動作させる
    addFilesToSlideshowPlaylist(loadedFiles);
}

void MainWindow::openFile()
{
    // ファイル選択ダイアログを表示し、音声と動画の両方の拡張子をフィルタに設定
    QStringList files = QFileDialog::getOpenFileNames(getActiveWindow(),
                                                      tr("メディアファイルを開く"),
                                                      QDir::homePath(),
                                                      tr("全てのメディアファイル (*.mp3 *.wav *.ogg *.flac *.mp4 *.mkv *.avi *.mov *.wmv);;"
                                                         "音声ファイル (*.mp3 *.wav *.ogg *.flac);;"
                                                         "動画ファイル (*.mp4 *.mkv *.avi *.mov *.wmv)"));

    // ファイルが選択された場合のみ処理を続行
    if (!files.isEmpty()) {
        m_playlistManager->addFilesToPlaylist(files, currentPlaylistIndex);
    }
}

void MainWindow::openFolder()
{
    QString dirPath = QFileDialog::getExistingDirectory(getActiveWindow(), tr("Open Folder"), QDir::homePath());
    if (!dirPath.isEmpty()) {
        // D&Dと同じファイルスキャン処理を呼び出す
        // デフォルトのドロップ先は音楽プレイリストとして指定
        startFileScan({QUrl::fromLocalFile(dirPath)});
    }
}

void MainWindow::openOptionsDialog()
{
    // テーマ情報を最新にする
    m_settingsManager->settings().theme = m_themeManager->currentTheme();

    // マネージャの設定を渡す
    OptionsDialog dialog(m_settingsManager->settings(), this);

    if (dialog.exec() == QDialog::Accepted) {
        AppSettings newSettings = dialog.getSettings();
        applySettings(newSettings);
    }
}

void MainWindow::populateSortComboBox(QComboBox* combo)
{
    combo->clear();
    // アイコンがあれば見栄えが良いですが、まずはテキストで矢印を表現します
    combo->addItem("名前 ↑");       // Index 0: Name Asc
    combo->addItem("名前 ↓");       // Index 1: Name Desc
    combo->addItem("日付 ↑");       // Index 2: Date Asc (古い順)
    combo->addItem("日付 ↓");       // Index 3: Date Desc (新しい順)
    combo->addItem("サイズ ↑");     // Index 4: Size Asc (小さい順)
    combo->addItem("サイズ ↓");     // Index 5: Size Desc (大きい順)
    combo->addItem("シャッフル");   // Index 6: Shuffle
}

void MainWindow::setItemHighlighted(QListWidgetItem* item, bool highlighted)
{
    if (!item) return;
    item->setData(MediaItemDelegate::IsActiveRole, highlighted);
}

void MainWindow::setupSortControls()
{
    // 1. MediaView用 (UIファイルに定義済みの ui->imageSortComboBox を使用)
    ui->imageSortComboBox->setToolTip("並び順 (右クリックで同期設定)");
    ui->imageSortComboBox->setFocusPolicy(Qt::NoFocus);
    populateSortComboBox(ui->imageSortComboBox);

    // 2. Bookshelf用 (コードで生成して ListOptionsWidget に追加)
    m_bookshelfSortComboBox = new QComboBox(this);
    m_bookshelfSortComboBox->setToolTip("並び順 (右クリックで同期設定)");
    m_bookshelfSortComboBox->setFocusPolicy(Qt::NoFocus);
    // 必要なら固定幅を設定
    m_bookshelfSortComboBox->setMinimumWidth(64);
    populateSortComboBox(m_bookshelfSortComboBox);

    // ListOptionsWidget に追加 ("並び順:" ラベルが欲しい場合はここでQLabelも作成して追加可能)
    // シンプルにコンボボックスだけ追加する場合:
    m_bookshelfOptions->addOptionWidget(m_bookshelfSortComboBox);

    // もし "並び順:" ラベルも一緒に本棚オプションに入れたい場合は以下のようにコンテナ化します
    /*
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(4);
    layout->addWidget(new QLabel("並び順:", container));
    layout->addWidget(m_bookshelfSortComboBox);
    m_bookshelfOptions->addOptionWidget(container);
    */

    // 3. コンテキストメニュー (同期設定用)
    ui->imageSortComboBox->setContextMenuPolicy(Qt::CustomContextMenu);
    m_bookshelfSortComboBox->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->imageSortComboBox, &QWidget::customContextMenuRequested,
            this, &MainWindow::showSortContextMenu);
    connect(m_bookshelfSortComboBox, &QWidget::customContextMenuRequested,
            this, &MainWindow::showSortContextMenu);

    // 4. 同期アクション作成
    m_syncSortOrderAction = new QAction("並び順設定を同期する", this);
    m_syncSortOrderAction->setCheckable(true);
    m_syncSortOrderAction->setChecked(m_syncSortOrder);
    connect(m_syncSortOrderAction, &QAction::toggled, this, &MainWindow::onSortSyncActionToggled);

    // 5. シグナル接続 (ラムダで呼び出し元を区別)
    connect(ui->imageSortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index){ onSortComboChanged(index, true); });

    connect(m_bookshelfSortComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index){ onSortComboChanged(index, false); });

    // 初期状態: 名前↑ (Index 0)
    ui->imageSortComboBox->setCurrentIndex(0);
    m_bookshelfSortComboBox->setCurrentIndex(0);
}

void MainWindow::setupBreadcrumbs()
{
    // コンテナがまだ作成されていなければ作成
    if (!m_breadcrumbContainer) {
        m_breadcrumbContainer = new QWidget(this);
        m_breadcrumbContainer->setObjectName("breadcrumbContainer");

        // ツールバー内に配置するため背景を透明化
        m_breadcrumbContainer->setStyleSheet("background: transparent;");

        // ★追加: コンテナ全体に対してコンテキストメニューを設定
        m_breadcrumbContainer->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_breadcrumbContainer, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos){
            QMenu menu;
            QAction *copyAction = menu.addAction("現在のパスをコピー");

            // メニューを表示
            QAction *selected = menu.exec(QCursor::pos());

            if (selected == copyAction) {
                // 現在表示中のパスを取得
                // (m_bookshelfWidget または m_imageViewController から現在のパスを取得します)
                QString currentPath = m_bookshelfWidget->currentPath();

                if (!currentPath.isEmpty()) {
                    QClipboard *clipboard = QGuiApplication::clipboard();
                    clipboard->setText(QDir::toNativeSeparators(currentPath));
                }
            }
        });

        // レイアウト作成
        m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbContainer);
        m_breadcrumbLayout->setContentsMargins(0, 0, 0, 0);
        m_breadcrumbLayout->setSpacing(2);

        // ツールバー (widget_3) に挿入
        if (auto layout = qobject_cast<QHBoxLayout*>(ui->widget_3->layout())) {
            layout->insertWidget(3, m_breadcrumbContainer);
        }
    }
}

bool MainWindow::shouldSwitchToVideoPage(VideoSwitchPolicy policy) const
{
    QString policyStr;
    switch(policy) {
    case VideoSwitchPolicy::Default: policyStr = "Default (Auto)"; break;
    case VideoSwitchPolicy::Always:  policyStr = "Always"; break;
    case VideoSwitchPolicy::Never:   policyStr = "Never"; break;
    }

    QString triggerStr;
    switch(m_nextPlayTrigger) {
    case PlayTrigger::Auto:       triggerStr = "Auto (Playlist/BG)"; break;
    case PlayTrigger::OpenFile:   triggerStr = "OpenFile (D&D/FileOpen)"; break;
    case PlayTrigger::UserAction: triggerStr = "UserAction (Click/Key)"; break;
    }

    qDebug() << "--- [PageSwitch Debug] ---";
    qDebug() << "  Input Policy:   " << policyStr;
    qDebug() << "  Current Trigger:" << triggerStr;
    // --- デバッグ出力終了 ---

    if (policy == VideoSwitchPolicy::Always) {
        qDebug() << "  Result: TRUE (Policy is Always)";
        return true;
    }
    if (policy == VideoSwitchPolicy::Never) {
        qDebug() << "  Result: FALSE (Policy is Never)";
        return false;
    }

    // --- policy == VideoSwitchPolicy::Default の場合のロジック ---

    // ユーザーが手動で再生を開始した場合
    if (m_nextPlayTrigger == PlayTrigger::OpenFile ||
        m_nextPlayTrigger == PlayTrigger::UserAction) {
        qDebug() << "  Result: TRUE (Manual Trigger in Default Mode)";
        return true;
    }

    // プレイリストの自動送り
    qDebug() << "  Result: FALSE (Auto Trigger in Default Mode)";
    return false;
}

void MainWindow::showFullScreenControls()
{
    if (!m_isVideoFullScreen || !m_fullScreenContainer) return;

    this->setCursor(Qt::ArrowCursor);
    updateFullScreenUi();
    
    if (m_fullScreenContainer->isHidden() || m_fullScreenContainer->windowOpacity() == 0.0) {
        m_fullScreenContainer->setWindowOpacity(0.0); // 念のため0に
        m_fullScreenContainer->show();
        m_fullScreenContainer->raise();

        // 0.0 から 0.9 へアニメーション
        m_controlBarFadeAnimation->setStartValue(0.0);
    } else {
        // 既に表示されている場合は、現在の不透明度から開始
        m_controlBarFadeAnimation->setStartValue(m_fullScreenContainer->windowOpacity());
    }

    m_controlBarFadeAnimation->setDuration(100);
    m_controlBarFadeAnimation->setStartValue(m_fullScreenContainer->windowOpacity());
    m_controlBarFadeAnimation->setEndValue(0.94); // 94%表示
    m_controlBarFadeAnimation->start();

    m_mouseInactivityTimer->start(1500);
}

void MainWindow::showSortContextMenu(const QPoint &pos)
{
    QWidget* senderWidget = qobject_cast<QWidget*>(sender());
    if (!senderWidget) return;

    QMenu menu(this);
    menu.addAction(m_syncSortOrderAction); // 「並び順設定を同期する」チェックボックス
    menu.exec(senderWidget->mapToGlobal(pos));
}

void MainWindow::showPreviewContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);

    // クリックされた位置にあるプレビューリストを特定する
    int targetPreviewIndex = -1;
    int targetRealIndex = -1;

    // グローバル座標に変換して判定
    QPoint globalPos = previewContainer->mapToGlobal(pos);

    for (int i = 0; i < previewListWidgets.size(); ++i) {
        if (previewListWidgets[i]->isVisible() &&
            previewListWidgets[i]->rect().contains(previewListWidgets[i]->mapFromGlobal(globalPos))) {

            targetPreviewIndex = i;
            targetRealIndex = m_previewSlotMapping[i];
            break;
        }
        // ラベル上のクリックも考慮
        if (previewLabels[i]->isVisible() &&
            previewLabels[i]->rect().contains(previewLabels[i]->mapFromGlobal(globalPos))) {

            targetPreviewIndex = i;
            targetRealIndex = m_previewSlotMapping[i];
            break;
        }
    }

    // もし有効なプレイリストの上でクリックされた場合、操作メニューを追加
    if (targetRealIndex != -1 && targetRealIndex < m_musicPlaylistWidget->count()) {
        QString name = m_musicPlaylistWidget->tabText(targetRealIndex);

        contextMenu.addAction(QString("「%1」を再生").arg(name), [this, targetRealIndex](){
            m_playlistManager->playTrackAtIndex(0, targetRealIndex);
        });

        contextMenu.addSeparator();

        contextMenu.addAction(QString("「%1」にファイルを追加...").arg(name), [this, targetRealIndex](){
            QStringList files = QFileDialog::getOpenFileNames(this, "メディアファイルを開く", QDir::homePath(),
                                                              "メディア (*.mp3 *.wav *.ogg *.flac *.mp4 *.mkv *.avi *.mov *.wmv)");
            if (!files.isEmpty()) {
                m_playlistManager->addFilesToPlaylist(files, targetRealIndex);
            }
        });

        contextMenu.addAction(QString("「%1」にフォルダを追加...").arg(name), [this, targetRealIndex](){
            QString dirPath = QFileDialog::getExistingDirectory(this, "フォルダを開く", QDir::homePath());
            if (!dirPath.isEmpty()) {
                startFileScan({QUrl::fromLocalFile(dirPath)}, targetRealIndex);
            }
        });

        contextMenu.addSeparator();
    }

    // --- 以下、既存の表示設定メニュー ---
    // ヘルパーを使ってメニュー項目を追加
    appendPreviewMenuActions(&contextMenu);

    contextMenu.exec(globalPos);
}

void MainWindow::showAboutDialog()
{
    AboutDialog dialog(this);
    dialog.exec(); // モーダルで表示
}

void MainWindow::startFileScan(const QList<QUrl> &urls, int targetPlaylist)
{
    // 設定構造体を作成して渡す
    ScanSettings settings;

    const AppSettings& appSettings = m_settingsManager->settings();

    settings.scanSubdirectories = appSettings.scanSubdirectories;
    settings.fileScanLimit = appSettings.fileScanLimit;
    settings.chunkSize = appSettings.chunkSize;

    // 以下の拡張子リストは MainWindow のメンバ変数のままなので変更なし
    settings.audioVideoExtensions = m_audioExtensions + m_videoExtensions;
    settings.imageExtensions = m_imageExtensions;

    // スキャン開始を依頼
    m_fileScanner->startScan(urls, targetPlaylist, settings);
}

void MainWindow::switchToCompactMode()
{
    this->hide();
    m_compactWindow->show();
    m_compactWindow->activateWindow();
}

void MainWindow::switchToFullMode()
{
    m_compactWindow->hide();
    this->show();
}

void MainWindow::syncAllListsUiState(int sourceFontSize, bool sourceReorderEnabled)
{
    // 1. 全てのリストに設定を適用
    for (QListWidget* list : getAllListWidgets()) {
        applyUiStateToList(list, sourceFontSize, sourceReorderEnabled);
    }
    
    if (m_folderTreeWidget) {
        QFont treeFont = m_folderTreeWidget->font();
        treeFont.setPointSize(sourceFontSize);
        m_folderTreeWidget->setFont(treeFont);
    }

    // 2. UIコントロール自体も同期させる
    m_playlistOptions->setFontSize(sourceFontSize);
    m_playlistOptions->setReorderEnabled(sourceReorderEnabled);

    m_slideshowOptions->setFontSize(sourceFontSize);
    m_slideshowOptions->setReorderEnabled(sourceReorderEnabled);

    m_folderTreeOptions->setFontSize(sourceFontSize);
    m_bookshelfOptions->setFontSize(sourceFontSize);
    m_bookshelfOptions->setReorderEnabled(false);
}

void MainWindow::syncControlBarButtons()
{
    // 1. コントロールバーのボタンを一旦全てクリア
    m_controlBar->clearPlaylistButtons();

    // 2. 現在のタブ情報を元に、ボタンを再作成
    int count = m_musicPlaylistWidget->count();
    for (int i = 0; i < count; ++i) {
        QString name = m_musicPlaylistWidget->tabText(i);
        m_controlBar->addPlaylistButton(i, name);
        m_compactWindow->controlBar()->addPlaylistButton(i, name);
    }

    // 3. 現在選択されているタブに対応するボタンをチェック状態にするようControlBarに指示
    updatePlaylistButtonStates();
}

void MainWindow::syncFloatingControlBarPosition()
{
    // コンテナがなければ何もしない
    if (!m_isVideoFullScreen || !m_fullScreenContainer) return;

    QPoint globalPos = ui->videoPage->mapToGlobal(QPoint(0, 0));
    int videoW = ui->videoPage->width();
    int videoH = ui->videoPage->height();

    int barW = qMin(static_cast<int>(videoW * 0.8), 800);
    // 高さは中身(ControlBar)に合わせて自動調整されるが、念のためHintを見る
    int barH = m_controlBar->sizeHint().height();

    // コンテナのサイズを設定
    m_fullScreenContainer->resize(barW, barH);

    int x = globalPos.x() + (videoW - barW) / 2;
    int y = globalPos.y() + videoH - barH - 40;

    m_fullScreenContainer->move(x, y);
    m_fullScreenContainer->raise();
}

void MainWindow::toggleAlwaysOnTop(bool checked)
{
    if (checked == m_isAlwaysOnTop) {
        return;
    }
    m_isAlwaysOnTop = checked;

    bool wasMainVisible = this->isVisible();
    bool wasCompactVisible = m_compactWindow->isVisible();

    // --- 両方のウィンドウのフラグを更新 ---
    Qt::WindowFlags mainFlags = this->windowFlags();
    Qt::WindowFlags compactFlags = m_compactWindow->windowFlags();

    if (m_isAlwaysOnTop) {
        this->setWindowFlags(mainFlags | Qt::WindowStaysOnTopHint);
        m_compactWindow->setWindowFlags(compactFlags | Qt::WindowStaysOnTopHint);
    } else {
        this->setWindowFlags(mainFlags & ~Qt::WindowStaysOnTopHint);
        m_compactWindow->setWindowFlags(compactFlags & ~Qt::WindowStaysOnTopHint);
    }

    if (wasMainVisible) {
        this->show();
    } else if (wasCompactVisible) {
        m_compactWindow->show();
    }

    // --- 全てのUIの状態を更新 ---
    updateControlBarStates();
}

void MainWindow::toggleCompactMode()
{
    // 現在フルウィンドウが表示されているかチェック
    if (this->isVisible()) {
        // 表示されていれば、コンパクトモードに切り替え
        switchToCompactMode();
    } else {
        // 表示されていなければ（＝コンパクトモード中なら）、フルモードに戻す
        switchToFullMode();
    }
}

void MainWindow::toggleFullScreenVideo()
{
    qDebug() << "\n### toggleFullScreenVideo CALLED ###";

    m_isVideoFullScreen = !m_isVideoFullScreen;

    QList<QWidget*> widgetsToHide = {
        ui->menubar, ui->leftToolBar, ui->rightToolBar, ui->controlToolBar,
        m_folderTreeDock, m_playlistDock, m_slideshowDock, m_bookshelfDock,
        ui->widget_3, ui->widget_8, ui->widget_7
    };

    if (m_isVideoFullScreen) {
        // === フルスクリーン化 ===
        ui->videoPage->setStyleSheet("background-color: black;");
        ui->videoFilePathLineEdit->setFocusPolicy(Qt::NoFocus);
        ui->videoFilePathLineEdit->clearFocus();

        this->showFullScreen();
        ui->videoContainer->show();
        for (QWidget* w : widgetsToHide) w->hide();

        // コンテナウィンドウ作成 (既存コード)
        m_fullScreenContainer = new QWidget(this, Qt::FramelessWindowHint | Qt::Tool);

        m_fullScreenContainer->setAttribute(Qt::WA_TranslucentBackground, true);
        m_fullScreenContainer->setAttribute(Qt::WA_NoSystemBackground, true);
        m_fullScreenContainer->installEventFilter(this);

        QVBoxLayout* layout = new QVBoxLayout(m_fullScreenContainer);
        layout->setContentsMargins(0, 0, 0, 0);

        m_controlBar->setParent(m_fullScreenContainer);
        m_controlBar->setAutoFillBackground(false);
        m_controlBar->setStyleSheet("background: transparent;");

        layout->addWidget(m_controlBar);
        m_controlBar->show();

        if (m_controlBarFadeAnimation) {
            m_controlBarFadeAnimation->setTargetObject(m_fullScreenContainer);
        }

        m_fullScreenContainer->setWindowOpacity(0.0);

        // レイアウト確定待ちタイマー
        QTimer::singleShot(100, this, [this](){
            if (m_isVideoFullScreen && m_fullScreenContainer) {
                m_fullScreenContainer->adjustSize();
                syncFloatingControlBarPosition();

                // アニメーション付きで表示開始
                showFullScreenControls();
            }
        });

    } else {
        // === 通常モード復帰 ===
        ui->videoPage->setStyleSheet("");
        ui->videoFilePathLineEdit->setFocusPolicy(Qt::StrongFocus);
        m_mouseInactivityTimer->stop();
        if(m_controlBarFadeAnimation) m_controlBarFadeAnimation->stop();
        this->setCursor(Qt::ArrowCursor);

        // ControlBarを戻す
        m_controlBar->setParent(this);
        m_controlBar->setStyleSheet("");
        ui->controlToolBar->addWidget(m_controlBar);
        m_controlBar->show();
        ui->controlToolBar->show();

        if (m_fullScreenContainer) {
            m_fullScreenContainer->removeEventFilter(this);
            m_fullScreenContainer->close();
            delete m_fullScreenContainer;
            m_fullScreenContainer = nullptr;
        }

        this->showNormal();

        ui->menubar->show();
        ui->leftToolBar->show();
        ui->rightToolBar->show();
        ui->widget_3->show();
        ui->widget_8->show();
        ui->widget_7->show();

        if(ui->actionShowFolderTree->isChecked()) m_folderTreeDock->show();
        if(ui->actionShowPlaylist->isChecked()) m_playlistDock->show();
        if(ui->actionShowSlideshow->isChecked()) m_slideshowDock->show();
        if(ui->actionShowBookshelf->isChecked()) m_bookshelfDock->show();
        if (m_videoBlackOverlay && m_videoBlackOverlay->isVisible()) {
            QTimer::singleShot(0, this, [this](){
                m_videoBlackOverlay->setGeometry(ui->videoContainer->geometry());
                m_videoBlackOverlay->lower(); // 念のため背面に下げる（videoContainerの上、widget_7の下）
                ui->widget_7->raise();        // widget_7を最前面へ
            });
        }
    }
}

void MainWindow::toggleMaximizeWithFade()
{
    qDebug() << "[Debug] toggleMaximize triggered.";
    qDebug() << "  Current State:" << (isMaximized() ? "Maximized" : "Normal");
    qDebug() << "  Window Geometry Before:" << this->geometry();

    // オーバーレイやアニメーションは一切行わず、素の挙動を確認する
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }

    // タイトルバーのアイコン更新のみ行う
    updateTitleBarStyle();

    qDebug() << "  Command sent. Waiting for resizeEvent...";
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_settingsManager->saveLastVolume(m_controlBar->volume());

    if (m_musicPlaylistWidget) {
        m_settingsManager->savePlaylistCount(m_musicPlaylistWidget->count());
    }

    // 1. 本棚のパス保存
    QString path = m_bookshelfWidget->currentPath();
    if (path.isEmpty()) path = QDir::homePath();
    m_settingsManager->saveBookshelfPath(path);

    QString currentFile = m_imageViewController->getCurrentFilePath();

    // ★ デバッグ出力
    qDebug() << "=== CLOSING APP ===";
    qDebug() << "Saving Last Viewed File:" << currentFile;

    m_settingsManager->saveLastViewedFile(currentFile);

    // 2. ビューの状態保存
    bool isPanorama = (m_imageViewController->getSlideshowMode() == ModePictureScroll);

    m_settingsManager->saveViewStates(
        isPanorama,
        m_imageViewController->getFitMode(),      // FitMode 型のまま
        m_imageViewController->getLayoutDirection() // LayoutDirection 型のまま
        );
    m_settingsManager->saveSlideshowInterval(ui->slideshowIntervalSpinBox->value());

    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) return;

    // ドロップされた場所がフォルダツリーの上かどうかだけをチェック
    QWidget* dropTarget = childAt(event->position().toPoint());
    QWidget* p = dropTarget;
    bool isFolderTreeDrop = false;
    while (p) {
        if (p == m_folderTreeWidget) {
            isFolderTreeDrop = true;
            break;
        }
        p = p->parentWidget();
    }

    if (isFolderTreeDrop) {
        // フォルダツリーの上なら、イベントを無視して終了（FolderTree側で処理させるため）
        event->ignore();
        return;
    }

    // プレビューリストへのドロップ判定
    for (int i = 0; i < previewListWidgets.size(); ++i) {
        QListWidget* previewList = previewListWidgets.at(i);
        if (previewList == dropTarget || previewList->isAncestorOf(dropTarget)) {
            qDebug() << "Dropped on preview list" << i;

            int realPlaylistIndex = m_previewSlotMapping[i];

            if (realPlaylistIndex != -1) {
                // 特定のプレイリストへの追加時は、自動再生・画面遷移は行わないのが一般的
                startFileScan(event->mimeData()->urls(), realPlaylistIndex);
            }

            event->acceptProposedAction();
            return;
        }
    }

    // プレイリストファイルのドロップか判定 (.qpl / .qsl)
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.size() == 1) {
        QString filePath = urls.first().toLocalFile();
        QFileInfo fileInfo(filePath);
        QString suffix = fileInfo.suffix().toLower();

        if (suffix == "qpl") {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                event->ignore();
                return;
            }

            QTextStream in(&file);
            QStringList loadedFiles;
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty()) continue;
                QFileInfo fi(line);
                if (fi.exists() && fi.isFile()) {
                    loadedFiles.append(line);
                }
            }
            file.close();

            if (!loadedFiles.isEmpty()) {
                // ★ 修正: プレイリスト読み込み時もユーザー操作としてマーク（必要に応じて再生ロジックも追加可能）
                m_nextPlayTrigger = PlayTrigger::OpenFile;
                m_playlistManager->addFilesToPlaylist(loadedFiles, currentPlaylistIndex);
            }
            event->acceptProposedAction();
            return;
        }
        else if (suffix == "qsl") {
            loadSlideshowListFromFile(filePath);
            event->acceptProposedAction();
            return;
        }
    }

    // --- 汎用のメディアファイルドロップ処理 ---
    // ここでフラグを立てることで、scanFinishedシグナル受信時に「再生開始」と「ページ切り替え」が行われます。

    m_autoPlayNextScan = true;
    m_nextPlayTrigger = PlayTrigger::OpenFile;

    qDebug() << "[Debug] dropEvent: Trigger set to OpenFile. AutoPlay=true."; // ★追加

    startFileScan(event->mimeData()->urls());
    event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // コンテナの背景描画処理 (枠線なし・テーマカラー)
    if (m_fullScreenContainer && obj == m_fullScreenContainer && event->type() == QEvent::Paint) {
        QPainter p(m_fullScreenContainer);
        QColor bgColor = this->palette().color(QPalette::Window);
        bgColor.setAlpha(255);

        p.setPen(Qt::NoPen);
        p.setBrush(bgColor);
        p.drawRect(m_fullScreenContainer->rect());

        return true;
    }

    if (obj == ui->slideshowIntervalSpinBox && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        int delta = wheelEvent->angleDelta().y();

        if (delta != 0) {
            double currentVal = ui->slideshowIntervalSpinBox->value();
            // 上回転(正)なら +0.5、下回転(負)なら -0.5
            double nextVal = (delta > 0) ? (currentVal + 0.5) : (currentVal - 0.5);

            ui->slideshowIntervalSpinBox->setValue(nextVal);
            return true; // イベントを消費してデフォルトの動作(0.1変化)を防ぐ
        }
    }

    // --- 1. 判定対象の領域を特定 ---

    // 動画領域かどうか
    bool isVideoArea = (obj == ui->videoContainer || obj == ui->videoPage || obj == m_videoBlackOverlay);

    // コントロールバー領域（およびその子要素）かどうか
    bool isControlBarObj = (obj == m_controlBar || obj == m_compactWindow->controlBar());
    if (m_fullScreenContainer && obj == m_fullScreenContainer) {
        isControlBarObj = true;
    }
    // オブジェクトがウィジェットなら親を遡って判定
    if (!isControlBarObj && obj->isWidgetType()) {
        QWidget* widget = static_cast<QWidget*>(obj);
        while (widget) {
            if (widget == m_controlBar || widget == m_compactWindow->controlBar()) {
                isControlBarObj = true;
                break;
            }
            widget = widget->parentWidget();
        }
    }


    // --- 2. 共通処理: クリック時のフォーカス解除 ---
    // 動画領域 または コントロールバー領域 をクリックした場合
    if ((isVideoArea || isControlBarObj) && event->type() == QEvent::MouseButtonPress) {

        // クリックされたオブジェクト自体が「入力可能なウィジェット」かどうか判定
        bool isTargetInput = false;
        if (qobject_cast<QLineEdit*>(obj) || qobject_cast<QAbstractSpinBox*>(obj)) {
            isTargetInput = true;
        }

        // 入力ウィジェット「以外」をクリックした場合のみ、フォーカス解除を実行する
        if (!isTargetInput) {
            // 現在フォーカスを持っているウィジェットを確認
            QWidget* focusedWidget = QApplication::focusWidget();

            if (focusedWidget) {
                // LineEdit, SpinBox, DoubleSpinBox ならフォーカスを強制解除
                if (qobject_cast<QLineEdit*>(focusedWidget) ||
                    qobject_cast<QAbstractSpinBox*>(focusedWidget)) {

                    focusedWidget->clearFocus();
                }
            }

            // ★修正: フルスクリーン時のコントロールバー操作中は、フォーカスを奪わないようにする
            // 理由: コントロールバーは別ウィンドウ(Qt::Tool)のため、this->setFocus()すると
            //      ウィンドウが非アクティブ化し、ボタンのクリック処理が中断されてしまうため。
            bool isFloatingControlBar = (m_isVideoFullScreen && isControlBarObj);

            if (!isFloatingControlBar) {
                // 通常時、または動画エリア背景をクリックした場合はメインウィンドウにフォーカスを戻す
                this->setFocus();
            }
        }

        // イベントはそのまま流す (return true しない)
    }


    // --- 3. 動画領域固有の処理 ---
    if (isVideoArea) {
        // ダブルクリックでフルスクリーン切り替え
        if (event->type() == QEvent::MouseButtonDblClick) {
            toggleFullScreenVideo();
            return true;
        }

        if (m_isVideoFullScreen && event->type() == QEvent::MouseMove) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if ((me->globalPosition().toPoint() - m_lastMousePos).manhattanLength() > 2) {
                m_lastMousePos = me->globalPosition().toPoint();
                showFullScreenControls();
            }
        }

        // Resize処理
        if (event->type() == QEvent::Resize && obj == ui->videoContainer) {
            m_videoLoadingLabel->move(ui->videoContainer->rect().center() - m_videoLoadingLabel->rect().center());
            if (m_isVideoFullScreen) {
                updateFullScreenUi();
            }
            return false;
        }

        // ホイールイベント
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            int delta = wheelEvent->angleDelta().y();

            if (delta > 0) {
                // ★修正: 上限を100に制限
                int newVol = qMin(500, m_controlBar->volume() + 5);
                m_mediaManager->setVolume(newVol);
            } else if (delta < 0) {
                // 0未満にならないように制限
                int newVol = qMax(0, m_controlBar->volume() - 5);
                m_mediaManager->setVolume(newVol);
            }
            if (m_isVideoFullScreen) showFullScreenControls();
            return true;
        }
    }


    // --- 4. コントロールバー領域固有の処理 ---
    if (isControlBarObj) {
        if (m_isVideoFullScreen && event->type() == QEvent::MouseMove) {
            showFullScreenControls();
        }

        if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            int delta = wheelEvent->angleDelta().y();
            int currentVol = m_mediaManager->getCurrentVolume();

            if (delta > 0) {
                // ★修正: 上限を100に制限
                int newVol = qMin(500, currentVol + 5);
                m_mediaManager->setVolume(newVol);
            } else if (delta < 0) {
                // 0未満にならないように制限
                int newVol = qMax(0, currentVol - 5);
                m_mediaManager->setVolume(newVol);
            }
            return true;
        }
    }

    // --- 5. その他の処理 ---
    if (event->type() == QEvent::MouseButtonDblClick) {
        int index = previewLabels.indexOf(qobject_cast<QLabel*>(obj));
        if (index != -1) {
            onTabRenameRequested(index);
            return true;
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    m_imageViewController->handleKeyPressEvent(event);

    if (event->isAccepted()) {
        return; // IVCが処理した
    }

    if (event->key() == Qt::Key_Delete) {
        // 現在フォーカスを持っているウィジェットを取得
        QWidget* focusedWidget = QApplication::focusWidget();

        // QListWidgetにキャスト試行
        QListWidget* focusedList = qobject_cast<QListWidget*>(focusedWidget);

        if (focusedList) {
            // 3a. フォーカスがスライドショーリストにあるかチェック
            if (isSlideshowList(focusedList)) {
                deleteSelectedSlideshowItems(focusedList);
                event->accept(); // イベントを処理済みにする
                return;
            }

            // 3b. フォーカスが音楽プレイリストにあるかチェック
            if (isMusicPlaylist(focusedList)) {
                m_musicPlaylistWidget->deleteSelectedItems();

                event->accept(); // イベントを処理済みにする
                return;
            }
        }
    }

    // IVCが処理しなかった場合、MainWindowのデフォルト処理 (ただし現在は空)
    QMainWindow::keyPressEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    // if (event->button() == Qt::LeftButton && m_titleBarWidget) {
    //     QPoint localPos = m_titleBarWidget->mapFrom(this, event->position().toPoint());
    //     if (m_titleBarWidget->rect().contains(localPos)) {
    //         QWidget *child = m_titleBarWidget->childAt(localPos);
    //         if (!child || child == m_titleBarWidget || qobject_cast<QLabel*>(child)) {
    //             toggleMaximizeWithFade();
    //             event->accept();
    //             return;
    //         }
    //     }
    // }
    QMainWindow::mouseDoubleClickEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    // if (event->button() == Qt::LeftButton) {

    //     // 1. まずリサイズ判定 (最大化時はスキップ)
    //     if (!isMaximized()) {
    //         Qt::Edges edges = edgesFromPos(event->position().toPoint(), 8);
    //         if (edges != Qt::Edges()) {
    //             if (windowHandle()) {
    //                 // ★Qt標準機能でOSのリサイズ動作を開始
    //                 windowHandle()->startSystemResize(edges);
    //                 return;
    //             }
    //         }
    //     }

    //     // 2. 次にタイトルバー移動判定 (リサイズ中でなければ)
    //     if (m_titleBarWidget) {
    //         QPoint localPos = m_titleBarWidget->mapFrom(this, event->position().toPoint());

    //         if (m_titleBarWidget->rect().contains(localPos)) {
    //             // ボタン類除外
    //             QWidget *child = m_titleBarWidget->childAt(localPos);
    //             if (child == m_minBtn || child == m_maxBtn || child == m_closeBtn) {
    //                 QMainWindow::mousePressEvent(event);
    //                 return;
    //             }
    //             // メニューバー除外
    //             if (ui->menubar && ui->menubar->geometry().contains(localPos)) {
    //                 QPoint menuPos = ui->menubar->mapFrom(m_titleBarWidget, localPos);
    //                 if (ui->menubar->actionAt(menuPos)) {
    //                     QMainWindow::mousePressEvent(event);
    //                     return;
    //                 }
    //             }

    //             // 移動開始
    //             if (windowHandle()) {
    //                 windowHandle()->startSystemMove();
    //                 event->accept();
    //                 return;
    //             }
    //         }
    //     }
    // }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // // 最大化時はリサイズカーソルを出さない
    // if (isMaximized()) {
    //     if (cursor() != Qt::ArrowCursor) setCursor(Qt::ArrowCursor);
    //     QMainWindow::mouseMoveEvent(event);
    //     return;
    // }

    // // 端の判定 (8px以内)
    // Qt::Edges edges = edgesFromPos(event->position().toPoint(), 8);

    // // カーソル形状の設定
    // if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) {
    //     setCursor(Qt::SizeFDiagCursor); // ⤡
    // } else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) {
    //     setCursor(Qt::SizeBDiagCursor); // ⤢
    // } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
    //     setCursor(Qt::SizeHorCursor);   // ⇔
    // } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
    //     setCursor(Qt::SizeVerCursor);   // ⇕
    // } else {
    //     // 端でなければ通常カーソルに戻す
    //     if (cursor() != Qt::ArrowCursor) setCursor(Qt::ArrowCursor);
    // }

    QMainWindow::mouseMoveEvent(event);
}

// void MainWindow::mouseReleaseEvent(QMouseEvent *event)
// {
//     m_isDragging = false;
//     QMainWindow::mouseReleaseEvent(event);
// }

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);

    // ★追加: ウィンドウ移動時も同期 (マルチモニタ環境などで重要)
    if (m_isVideoFullScreen) {
        syncFloatingControlBarPosition();
    }
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
// #ifdef Q_OS_WIN
//     if (eventType == "windows_generic_MSG") {
//         MSG *msg = static_cast<MSG *>(message);

//         // WM_NCCALCSIZE: 標準フレームを消す処理のみ残す
//         if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
//             if (::IsZoomed(msg->hwnd)) {
//                 int frameX = ::GetSystemMetrics(SM_CXFRAME);
//                 int frameY = ::GetSystemMetrics(SM_CYFRAME);
//                 int padding = ::GetSystemMetrics(SM_CXPADDEDBORDER);
//                 int border = frameX + padding;

//                 NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
//                 params->rgrc[0].left   += border;
//                 params->rgrc[0].top    += border;
//                 params->rgrc[0].right  -= border;
//                 params->rgrc[0].bottom -= border;
//             }
//             *result = 0;
//             return true;
//         }

//         // ★ WM_NCHITTEST の処理ブロックは全て削除します
//         // Qtの mouseMoveEvent 等に任せるためです
//     }
// #endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    QSize oldSize = event->oldSize();
    QSize newSize = event->size();
    QSize mediaViewSize = ui->mediaView->size();
    QSize viewportSize = ui->mediaView->viewport()->size();

    qDebug() << "\n[Debug] resizeEvent Caught!";
    qDebug() << "  Window Size Change:" << oldSize << "->" << newSize;
    qDebug() << "  MediaView Size:    " << mediaViewSize;
    qDebug() << "  Viewport Size:     " << viewportSize;


    // 画像位置の再計算
    if (m_imageViewController) {
        m_imageViewController->handleResize(true);
    }

    // コントロールバー同期
    if (m_isVideoFullScreen) {
        syncFloatingControlBarPosition();
    }
    if (m_videoBlackOverlay && m_videoBlackOverlay->isVisible()) {
        m_videoBlackOverlay->setGeometry(ui->videoContainer->geometry());
    }
}
