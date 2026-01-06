#ifndef IMAGEVIEWCONTROLLER_H
#define IMAGEVIEWCONTROLLER_H

#include <QObject>
#include <QCollator>
#include <QFutureWatcher>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QElapsedTimer>
#include <QListWidget>
#include <QFileInfo>
#include <QMap>
#include <QMovie>
#include <QSet>
#include <QtConcurrent>

#include "pixmap_object.h"
#include "utils/common_types.h"

// ★ 必要なクラスの前方宣言
class QGraphicsView;
class QLineEdit;
class QSlider;
class QSpinBox;
class QProgressBar;
class QDoubleSpinBox;
class QStackedWidget;
class QLabel;

enum SlideshowMode { ModeStandard, ModePictureScroll };
enum SlideshowEffect { EffectNone, EffectFade, EffectSlide };
enum FitMode { FitInside, FitToWidth, FitToHeight };
enum LayoutDirection { Forward, Backward };
enum SlideDirection { DirectionHorizontal, DirectionVertical };
enum ViewMode { ModeDirectoryBrowse, ModeSlideshowList };


// --- 前方宣言 ---
class ViewUpdateGuard;

class ImageViewController : public QObject
{
    Q_OBJECT
    friend class ViewUpdateGuard;

public:
    // UIのポインタを受け取るコンストラクタ
    void setSortOrder(SortMode mode, bool ascending = true);
    explicit ImageViewController(
        QGraphicsView *view,
        QLineEdit *filenameEdit,
        QSlider *viewControlSlider,
        QSpinBox *zoomSpinBox,
        QProgressBar *progressBar,
        QDoubleSpinBox *intervalSpinBox,
        QStackedWidget *stackedWidget,
        QWidget *videoPage,
        QObject *parent = nullptr
        );
    ~ImageViewController();

    void setViewStates(bool isPanorama, FitMode fitMode, LayoutDirection layoutDirection);
    QGraphicsScene* getScene() const { return mediaScene; }

    // --- ゲッター ---
    ViewMode getViewMode() const { return m_viewMode; }
    QString getCurrentFilePath() const;
    QListWidget* getCurrentListWidget() const { return m_currentSlideshowList; }
    SlideshowMode getSlideshowMode() const { return m_slideshowMode; }
    FitMode getFitMode() const { return m_fitMode; }
    LayoutDirection getLayoutDirection() const { return m_layoutDirection; }
    SlideDirection getSlideDirection() const { return m_slideDirection; }
    bool isSlideshowActive() const { return slideshowTimer->isActive(); }
    const QStringList getActiveImageList() const;

    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    // --- 操作スロット ---
    void displayMedia(const QString &filePath, bool updateLineEdit = true);
    void setupPictureScroll(const QStringList& files);
    void positionScrollAtIndex(int index);
    void rebuildPanoramaOnResize();
    void applyFitMode();

    void toggleSlideshow();
    void onTogglePanoramaMode();
    void onToggleFitMode();
    void onLayoutDirectionToggled();
    void onViewControlSliderMoved(int value);
    void onZoomSpinBoxChanged(int value);
    void setSlideshowEffect(int index);
    void setLoading(bool loading);
    void setSlideshowInterval(double seconds);

    // --- イベントハンドラ ---
    void handleResize(bool forceImmediate = false);
    void handleWheelEvent(QWheelEvent *event, QWidget* viewport);
    void handleKeyPressEvent(QKeyEvent *event);

    // --- データ連携 ---
    void setSlideshowList(QListWidget* listWidget);
    void onSlideshowSelectionChanged();
    void onSlideshowListDoubleClicked(QListWidgetItem *item);
    void onSlideshowPlaylistChanged();

    void loadDirectory(const QDir& dir, const QString& fileToSelectPath = QString());
    void switchToSlideshowListMode(QListWidgetItem *item);
    void setImageExtensions(const QStringList& extensions);
    void goBack();
    void goForward();
    void goUp();

signals:
    // --- 通知シグナル ---
    void mediaViewStatesChanged();
    void setItemHighlighted(QListWidgetItem* item, bool highlighted);
    void currentImageChanged(const QString& filePath);
    void currentDirectoryChanged(const QString& path);
    void navigationStateChanged(bool canGoBack, bool canGoForward, bool canGoUp);
    void filesDropped(const QList<QUrl>& urls);

private slots:
    void showNextSlide(bool isFirstSlide = false);
    void updateSlideshowProgress();
    void updateSlideshowIndexFromScroll();
    void handlePanoramaScrollChanged();
    void onResizeTimeout();
    void onFilenameEntered();

private:
    // --- ヘルパー関数 ---
    void updateOverlayLayout();
    void updateViewFit();
    void updateViewControlSliderState(int currentIndex = -1, int count = -1);
    void updateZoomState();
    void loadSlidesAround(int index);
    void scrollToImage(int index);
    QString getParentPath(const QString& path) const;
    void addPathToHistory(const QString& path);
    void browseTo(const QString& path, const QString& fileToSelectPath, bool addToHistory);
    void stopCurrentMovie();
    void cleanupPanoramaMovie(int index);
    bool isAnimatedImage(const QString &path);
    void stepByImage(int step);

    // --- UIポインタ (Dependency Injection) ---
    QGraphicsView *m_view;
    QLineEdit *m_filenameEdit;
    QSlider *m_viewControlSlider;
    QSpinBox *m_zoomSpinBox;
    QProgressBar *m_progressBar;
    QDoubleSpinBox *m_intervalSpinBox;
    QStackedWidget *m_stackedWidget;
    QWidget *m_videoPage;

    // --- 内部データ ---
    QGraphicsScene *mediaScene;
    PixmapObject *currentImageItem;
    QMovie *m_currentMovie = nullptr;
    QMap<int, QMovie*> m_panoramaMovies;
    QLabel *m_loadingLabel;
    QLabel *m_emptyDirectoryLabel; // 追加: 宣言が漏れていた場合のために念のため
    QString m_currentDisplayedFilePath;

    QList<QString> m_history;
    int m_historyIndex;
    QString m_currentBrowsePath;
    double m_slideshowInterval;
    int m_logicalTargetIndex = -1;

    QListWidget* m_currentSlideshowList;
    QTimer *slideshowTimer;
    int slideshowCurrentIndex;
    SlideshowMode m_slideshowMode;
    SlideshowEffect m_slideshowEffect;
    SlideDirection m_slideDirection;
    ViewMode m_viewMode;
    QStringList m_directoryFiles;
    QStringList m_imageExtensions;

    struct SlideInfo {
        QString filePath;
        QRectF geometry;
        QGraphicsPixmapItem* item = nullptr;
    };
    QList<SlideInfo> m_slides;

    QTimer *slideshowProgressTimer;
    QTimer *m_scrollIndexUpdateTimer;
    QElapsedTimer slideshowElapsedTimer;
    QTimer* m_resizeTimer;
    QListWidgetItem* m_currentlyDisplayedSlideItem;
    QPoint m_clickStartPos;
    bool m_isClickCandidate = false;
    QPropertyAnimation* m_currentScrollAnim = nullptr;

    qreal m_fitScale;
    qreal m_userZoomFactor;
    FitMode m_fitMode;
    FitMode m_standardLastFitMode;
    FitMode m_panoramaLastFitMode;
    LayoutDirection m_layoutDirection;
    bool m_isUpdatingView;
    bool m_isProgrammaticScroll;

    QCollator m_collator;       // 数値考慮の比較用
    SortMode m_currentSortMode; // 現在のモード
    bool m_sortAscending;       // 昇順/降順

    void sortFileInfos(QFileInfoList &list);

    QSet<int> m_loadingIndices;
    struct AsyncLoadResult {
        int index;
        QImage image;
        bool success;
    };

    void onImageLoaded(const AsyncLoadResult& result);

    static const int SCROLL_UPDATE_INTERVAL = 40;
    static const int RESIZE_DEBOUNCE_INTERVAL = 80;
};

class ViewUpdateGuard {
public:
    explicit ViewUpdateGuard(ImageViewController* controller) : m_controller(controller) {
        if (m_controller) m_controller->blockSignals(true);
    }
    ~ViewUpdateGuard() {
        if (m_controller) m_controller->blockSignals(false);
    }
private:
    ImageViewController* m_controller;
};

#endif // IMAGEVIEWCONTROLLER_H
