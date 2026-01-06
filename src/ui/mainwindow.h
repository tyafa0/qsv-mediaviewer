#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "mediamanager.h"
#include "settingsmanager.h"
#include "controlbar.h"
#include "compactwindow.h"
#include "listoptionswidget.h"
#include "slideshowwidget.h"
#include "aboutdialog.h"

#include <QMainWindow>

#include <QButtonGroup>
#include <QCache>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QFileSystemModel>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QMutex>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QStringList>
#include <QToolButton>

//============ 前方宣言 ============
class QTimer;
class QButtonGroup;
class QLabel;
class QShortcut;
class MediaManager;
class PlaylistManager;
class ThemeManager;
class SettingsManager;
class ImageViewController;
class FolderTreeWidget;
class ViewUpdateGuard;
class BookshelfWidget;
class FileScanner;
class MusicPlaylistWidget;
namespace Ui { class MainWindow; }
class ListOptionsWidget;

enum class PlayTrigger {
    Auto,       // 自動送り、連続再生（デフォルト）
    OpenFile,   // ファイルを開く、D&D
    UserAction  // リストのダブルクリック、次へ/前へボタンなど
};

//==============================================================================
// MainWindow クラス定義
//==============================================================================
class MainWindow : public QMainWindow
{
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void addFilesFromExplorer(const QStringList &filePaths, bool isColdStart = false);

protected:
    // --- Qt標準イベントハンドラ ---
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    // void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    // --- UIからのアクションを処理するスロット ---
    // ファイル操作
    void openFile();
    void openFolder();
    void onFilePathEntered();
    void onFolderTreeContextMenu(const QPoint &pos);
    void loadSlideshowListFromFile(const QString& filePath);

    // 再生コントロール
    void togglePlayback();
    void stopMusic();
    void playSelected();

    // プレイリスト操作
    void onPlaylistTabChanged(int index);
    void onTabRenameRequested(int index);
    void onMainTabTextUpdated(int index, const QString &text);
    void startPlaybackOnPlaylist(int playlistIndex);
    void showPreviewContextMenu(const QPoint &pos);
    void updatePreviews();

     // スライドショー操作
    void onSlideshowListItemDoubleClicked(QListWidgetItem *item);

    // コントロールバー
    void handlePlayPauseClicked() { togglePlayback(); } // 既存のロジックを呼び出す
    void handleStopClicked() { stopMusic(); }
    void handleNextClicked();
    void handlePrevClicked();
    void handleRepeatClicked();
    void handleShuffleClicked();

    // ウィンドウ・メニューバー
    void toggleAlwaysOnTop(bool checked);
    void switchToCompactMode();
    void switchToFullMode();
    void toggleCompactMode();
    void updateRecentFilesMenu();
    void onRecentFileTriggered();
    void clearRecentFiles();


    // 本棚・ナビゲーション
    void onBookshelfOptionsContextMenu(const QPoint &pos);
    void onSortComboChanged(int index, bool fromMediaView);
    void onSortSyncActionToggled(bool checked);
    void showSortContextMenu(const QPoint &pos);

    // オプション
    void openOptionsDialog();
    void applySettings(const AppSettings& newSettings);
    void onToggleTheme();
    void performInitialUiUpdate();

    // --- MediaManagerからのシグナルを受けるスロット ---
    void onMediaLoadingStateChanged(bool isLoading);
    void onMediaVolumeChanged(int percent, bool isMuted);

    // --- PlaylistManagerからのシグナルを受けるスロット ---
    void onTrackReadyToPlay(const QString& filePath, int playlistIndex, int trackIndex);
    void onPlaybackShouldStop();
    void onPlaylistDataChanged(int playlistIndex);

    // --- ImageViewerControllerからのシグナルを受けるスロット ---
    void onMediaViewStatesChanged();
    void onSetItemHighlighted(QListWidgetItem* item, bool highlighted);

    // --- NavigationManagerからのシグナルを受けるスロット ---
    void onNavPathChanged(const QString& path);
    void onNavFileActivated(const QString& filePath);
    void onNavFileSelected(const QString& filePath);
    void onNavStateChanged(bool canGoBack, bool canGoForward, bool canGoUp);
    void updateBreadcrumbs(const QString &path);
    void onBreadcrumbClicked();

private:
    // --- 初期化ヘルパー関数 ---
    void setupManagers();
    void setupUiComponents();
    void setupConnections();
    void setupShortcuts();
    void setupSortControls();
    void setupBreadcrumbs();
    void setupCustomTitleBar();

    // ヘルパー関数
    void addFilesToSlideshowPlaylist(const QStringList &files);
    void addFileToRecentList(const QString& filePath);
    void appendPreviewMenuActions(QMenu* menu);
    void applySortMode(int index, bool fromMediaView);
    void applyTheme(const QString& themeName);
    void applyUiStateToList(QListWidget* list, int fontSize, bool reorderEnabled);
    void handleSlideshowItemsDeletion(QListWidget* listWidget);
    void loadDirectoryIntoSlideshowList(const QDir& dir, const QString& fileToSelectPath = QString());
    void populateSortComboBox(QComboBox* combo);
    void setItemHighlighted(QListWidgetItem* item, bool highlighted);
    void startFileScan(const QList<QUrl> &urls, int targetPlaylist = -1);
    void switchToPlaylist(int index);
    void syncAllListsUiState(int sourceFontSize, bool sourceReorderEnabled);
    void syncControlBarButtons();
    void updateAllWidgetIcons();
    void updateControlBarStates();
    void updateDockWidgetBehavior();
    void updateMediaViewStates();
    void updatePlaylistButtonStates();
    void updateSlideshowPlayPauseButton();
    void updateTitleBarStyle();

    QString getListWidgetStyle() const;
    QString getButtonStyles() const;
    QList<QListWidget*> getAllListWidgets() const;
    QIcon getColorizedIcon(QStyle::StandardPixmap sp, const QColor &color);

    bool isSlideshowList(QListWidget* listWidget) const;
    bool isMusicPlaylist(QListWidget* listWidget) const;
    bool shouldSwitchToVideoPage(VideoSwitchPolicy policy) const;
    void deleteSelectedSlideshowItems(QListWidget* listWidget);

    void toggleFullScreenVideo();         // フルスクリーンの切り替え
    void toggleMaximizeWithFade();
    void updateFullScreenUi();            // フルスクリーン時のUI配置更新
    void showFullScreenControls();        // コントロールを表示
    void hideFullScreenControls();        // コントロールを隠す
    void syncFloatingControlBarPosition();
    Qt::Edges edgesFromPos(const QPoint &pos, int margin) const;

    QToolButton* createSeparatorButton(const QString &targetPath);
    QPushButton* createPathButton(const QString &name, const QString &targetPath);

private:
    // --- UIポインタ ---
    Ui::MainWindow *ui;
    QActionGroup *m_viewPageActionGroup;
    ControlBar* m_controlBar;
    CompactWindow* m_compactWindow;
    MediaManager* m_mediaManager;
    PlaylistManager* m_playlistManager;
    ThemeManager *m_themeManager;
    ImageViewController* m_imageViewController;
    FolderTreeWidget* m_folderTreeWidget;
    BookshelfWidget *m_bookshelfWidget;
    MusicPlaylistWidget *m_musicPlaylistWidget;
    FileScanner *m_fileScanner;
    SlideshowWidget *m_slideshowWidget;
    QWidget* m_titleBarWidget = nullptr;
    QToolButton *m_minBtn = nullptr; // メンバ変数に昇格
    QToolButton *m_maxBtn = nullptr; // メンバ変数に昇格
    QToolButton *m_closeBtn = nullptr; // メンバ変数に昇格

    ListOptionsWidget *m_playlistOptions;
    ListOptionsWidget *m_slideshowOptions;
    ListOptionsWidget *m_folderTreeOptions;
    ListOptionsWidget* m_bookshelfOptions;
    QAction* m_bookshelfSyncDateFontAction;
    QCheckBox* m_bookshelfShowImagesCheckbox;

    // --- データメンバ ---
    // プレイリスト関連
    QListWidgetItem* m_currentlyPlayingMusicItem = nullptr;
    int currentPlaylistIndex;
    int m_playingPlaylistIndex;
    QListWidgetItem* m_currentlyDisplayedSlideItem = nullptr;
    QString m_currentTrackTitle;

    // プレビュー関連
    QList<QWidget*> previewGroups;
    QList<QLabel*> previewLabels;
    QList<QListWidget*> previewListWidgets;
    QWidget* previewContainer;
    // m_previewSlotMapping[0] -> 1番目のプレビュースロットに表示するプレイリストのインデックス
    // m_previewSlotMapping[1] -> 2番目のプレビュースロットに表示するプレイリストのインデックス
    int m_previewSlotMapping[2];
    QList<int> m_tabActivationHistory;
    bool m_previewsArePinned;

    // ファイルスキャン関連
    int m_asyncOperations = 0;
    QStringList m_audioExtensions;
    QStringList m_videoExtensions;
    QStringList m_imageExtensions;
    QStringList m_playlistExtensions;
    QStringList m_allMediaExtensions;
    bool m_autoPlayNextScan;

    // メディアビューワー関連
    QLabel *m_videoLoadingLabel;
    PlayTrigger m_nextPlayTrigger = PlayTrigger::Auto;

    // ビデオ関連
    bool m_isVideoFullScreen = false;     // ビデオフルスクリーンモードかどうか
    QTimer* m_mouseInactivityTimer;       // マウス操作がない時にUIを隠すタイマー
    QPoint m_lastMousePos;                // 最後のマウス位置（移動判定用）
    QWidget* m_originalControlBarParent;  // コントロールバーの元の親（復帰用）
    QPropertyAnimation* m_controlBarFadeAnimation = nullptr;
    QWidget* m_fullScreenContainer = nullptr;
    QLabel* m_videoBlackOverlay = nullptr;


    // 折りたたみ・ウィンドウ・メニューバー関連
    QList<int> lastSplitterSizes;
    bool isMediaViewFullScreen;
    QDockWidget *m_folderTreeDock;
    QDockWidget *m_playlistDock;
    QDockWidget *m_slideshowDock;
    QDockWidget *m_bookshelfDock;
    bool m_isAlwaysOnTop;
    // QPoint m_dragPosition;
    // bool m_isDragging = false;
    QToolButton *m_maximizeBtn = nullptr;
    QLabel* m_transitionOverlay = nullptr;

    QWidget* getActiveWindow() const;
    void showAboutDialog();

    QMenu* m_recentFilesMenu;
    QStringList m_recentFiles;
    QList<QAction*> m_recentFileActions;

    // 本棚関連
    QToolButton* m_bookshelfToggleThumbnailButton; // (サムネイル表示切替ボタン)
    bool m_bookshelfThumbnailsVisible;           // (サムネイル表示状態)
    QCache<QString, QPixmap> m_thumbnailCache;   // (サムネイルキャッシュ)
    QMutex m_thumbnailCacheMutex;                // (キャッシュ保護用ミューテックス)
    QComboBox *m_bookshelfSortComboBox = nullptr;
    QAction *m_syncSortOrderAction = nullptr;
    bool m_syncSortOrder = true; // デフォルトで同期する

    // オプション関連
    SettingsManager *m_settingsManager;
    int m_initialPlaylistCount;

    // Qt関連
    QList<QWidget*> m_breadcrumbWidgets;
    QWidget *m_breadcrumbContainer = nullptr;
    QHBoxLayout *m_breadcrumbLayout = nullptr;

    // 定数
    static const int SCROLL_UPDATE_INTERVAL = 40;
    static const int RESIZE_DEBOUNCE_INTERVAL = 80;
    static const int FILE_ADD_CHUNK_SIZE = 50;
};
#endif // MAINWINDOW_H
