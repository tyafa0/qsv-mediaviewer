#include "imageviewcontroller.h"
#include "panoramaview.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGraphicsView>
#include <QGraphicsOpacityEffect>
#include <QImageReader>
#include <QMimeData>
#include <QLabel>
#include <QLineEdit>
#include <QParallelAnimationGroup>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QRandomGenerator> // ★ 追加: シャッフル用
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolTip>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>

ImageViewController::ImageViewController(
    QGraphicsView *view,
    QLineEdit *filenameEdit,
    QSlider *viewControlSlider,
    QSpinBox *zoomSpinBox,
    QProgressBar *progressBar,
    QDoubleSpinBox *intervalSpinBox,
    QStackedWidget *stackedWidget,
    QWidget *videoPage,
    QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_filenameEdit(filenameEdit)
    , m_viewControlSlider(viewControlSlider)
    , m_zoomSpinBox(zoomSpinBox)
    , m_progressBar(progressBar)
    , m_intervalSpinBox(intervalSpinBox)
    , m_stackedWidget(stackedWidget)
    , m_videoPage(videoPage)
    , mediaScene(new QGraphicsScene(this))
    , m_currentSlideshowList(nullptr)
    , slideshowCurrentIndex(-1)
    , m_slideshowMode(ModeStandard)
    , m_slideshowEffect(EffectSlide)
    , m_slideDirection(DirectionHorizontal)
    , m_currentlyDisplayedSlideItem(nullptr)
    , m_fitScale(1.0)
    , m_userZoomFactor(1.0)
    , m_fitMode(FitInside)
    , m_standardLastFitMode(FitInside)
    , m_panoramaLastFitMode(FitToHeight)
    , m_layoutDirection(LayoutDirection::Forward)
    , m_isUpdatingView(false)
    , m_isProgrammaticScroll(false)
    , m_viewMode(ModeDirectoryBrowse)
    , m_historyIndex(-1)
    , m_currentSortMode(SortName) // ★ 初期化: 名前順
    , m_sortAscending(true)
    , m_currentMovie(nullptr)
    , m_logicalTargetIndex(-1)
    , m_currentScrollAnim(nullptr)
{
    // --- mediaView 関連の初期化 ---
    currentImageItem = new PixmapObject();
    mediaScene->addItem(currentImageItem);

    // m_loadingLabel の作成 (parent は m_view)
    m_loadingLabel = new QLabel("Loading...", m_view);
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_loadingLabel->font();
    font.setPointSize(24);
    font.setBold(true);
    m_loadingLabel->setFont(font);
    m_loadingLabel->setStyleSheet("background-color: rgba(0, 0, 0, 128); color: white; border-radius: 10px;");
    m_loadingLabel->adjustSize();
    m_loadingLabel->setFixedSize(m_loadingLabel->width() + 40, m_loadingLabel->height() + 20);
    m_loadingLabel->hide();

    m_emptyDirectoryLabel = new QLabel("表示可能なファイルがありません", m_view);
    m_emptyDirectoryLabel->setAlignment(Qt::AlignCenter);
    m_emptyDirectoryLabel->setFont(font);
    m_emptyDirectoryLabel->setStyleSheet("background-color: rgba(0, 0, 0, 128); color: white; border-radius: 10px;");
    m_emptyDirectoryLabel->adjustSize();
    m_emptyDirectoryLabel->setFixedSize(m_emptyDirectoryLabel->width() + 40, m_emptyDirectoryLabel->height() + 20);
    m_emptyDirectoryLabel->hide();

    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);

    // --- タイマーの初期化 ---
    m_slideshowInterval = m_intervalSpinBox->value(); // ui->slideshowIntervalSpinBox -> m_intervalSpinBox
    slideshowTimer = new QTimer(this);
    connect(slideshowTimer, &QTimer::timeout, this, [this](){ this->showNextSlide(); });

    slideshowProgressTimer = new QTimer(this);
    connect(slideshowProgressTimer, &QTimer::timeout, this, &ImageViewController::updateSlideshowProgress);

    m_scrollIndexUpdateTimer = new QTimer(this);
    m_scrollIndexUpdateTimer->setInterval(SCROLL_UPDATE_INTERVAL);
    m_scrollIndexUpdateTimer->setSingleShot(true);
    connect(m_scrollIndexUpdateTimer, &QTimer::timeout, this, &ImageViewController::updateSlideshowIndexFromScroll);

    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setInterval(RESIZE_DEBOUNCE_INTERVAL);
    m_resizeTimer->setSingleShot(true);

    connect(m_resizeTimer, &QTimer::timeout, this, &ImageViewController::onResizeTimeout);

    // Enterが押されたら onFilenameEntered スロットを呼ぶ
    connect(m_filenameEdit, &QLineEdit::returnPressed, this, &ImageViewController::onFilenameEntered);

    // 編集が終わったら（フォーカスが外れたら）読み取り専用に戻す
    connect(m_filenameEdit, &QLineEdit::editingFinished, this, [this]() {
        this->m_filenameEdit->setReadOnly(true);
    });

    m_view->installEventFilter(this);
    m_view->viewport()->installEventFilter(this);
    m_filenameEdit->installEventFilter(this);
    m_viewControlSlider->installEventFilter(this);
    m_intervalSpinBox->installEventFilter(this);
}

ImageViewController::~ImageViewController()
{
    stopCurrentMovie();

    for (auto movie : m_panoramaMovies) {
        if (movie) {
            movie->stop();
            delete movie;
        }
    }
    m_panoramaMovies.clear();

    if (m_view && m_view->viewport()) {
        m_view->viewport()->removeEventFilter(this);
    }
    if (m_filenameEdit) {
        m_filenameEdit->removeEventFilter(this);
    }
    if (m_viewControlSlider) {
        m_viewControlSlider->removeEventFilter(this);
    }
    if (m_intervalSpinBox) {
        m_intervalSpinBox->removeEventFilter(this);
    }

    // m_slides 内の QGraphicsPixmapItem* は手動で削除
    for(const auto& slide : m_slides) {
        if(slide.item) delete slide.item;
    }
}

void ImageViewController::displayMedia(const QString &filePath, bool updateLineEdit)
{
    if (slideshowTimer->isActive() || m_slideshowMode == ModePictureScroll) {
        return;
    }

    // 1. 古いムービーを停止・破棄
    stopCurrentMovie();

    if (updateLineEdit) {
        m_filenameEdit->setText(QFileInfo(filePath).fileName());
    }

    // 2. 画像フォーマットの判定
    bool isMovie = isAnimatedImage(filePath);

    qDebug() << "[IVC] File:" << QFileInfo(filePath).fileName() << "IsMovie:" << isMovie;

    if (isMovie) {
        // --- A. アニメーション画像 (GIF) の場合 ---
        qDebug() << "[IVC] Starting QMovie...";
        m_currentMovie = new QMovie(filePath);

        if (!m_currentMovie->isValid()) {
            qDebug() << "[IVC] QMovie is invalid. Fallback to static.";
            delete m_currentMovie;
            m_currentMovie = nullptr;
            isMovie = false;
        } else {
            // キャッシュ設定
            m_currentMovie->setCacheMode(QMovie::CacheAll);

            // フレーム更新シグナルを接続
            connect(m_currentMovie, &QMovie::frameChanged, this, [this](int frameNumber) {
                Q_UNUSED(frameNumber);
                if (m_currentMovie && currentImageItem) {
                    currentImageItem->setPixmap(m_currentMovie->currentPixmap());
                    // ★重要: GIFアニメーションのために強制的にシーンを更新
                    mediaScene->update();
                }
            });

            // エラー監視
            connect(m_currentMovie, &QMovie::error, this, [](QImageReader::ImageReaderError error){
                qDebug() << "[IVC] QMovie Error:" << error;
            });

            m_currentMovie->start();

            // ラベル操作
            m_emptyDirectoryLabel->hide();

            emit setItemHighlighted(m_currentlyDisplayedSlideItem, false);
            m_currentlyDisplayedSlideItem = nullptr;

            // (リスト内検索ハイライト処理があればここに記述)

            currentImageItem->show();

            // サイズ合わせ
            connect(m_currentMovie, &QMovie::started, this, [this](){
                QTimer::singleShot(0, this, &ImageViewController::applyFitMode);
            });

            if (!filePath.isEmpty()) {
                emit currentImageChanged(filePath);
            }
        }
    }

    if (!isMovie) {
        // --- B. 通常の静止画の場合 ---
        qDebug() << "[IVC] Loading as static image.";
        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        QPixmap pixmap = QPixmap::fromImageReader(&reader);

        if (pixmap.isNull()) {
            currentImageItem->setPixmap(QPixmap());
            if (getActiveImageList().isEmpty()) {
                m_emptyDirectoryLabel->show();
                updateOverlayLayout();
            } else {
                m_emptyDirectoryLabel->hide();
            }
        } else {
            m_emptyDirectoryLabel->hide();

            emit setItemHighlighted(m_currentlyDisplayedSlideItem, false);
            m_currentlyDisplayedSlideItem = nullptr;

            const QStringList& allFiles = getActiveImageList();
            int itemIndex = allFiles.indexOf(filePath);

            // リスト内アイテムの特定
            if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
                for (int i = 0; i < m_currentSlideshowList->count(); ++i) {
                    QListWidgetItem* item = m_currentSlideshowList->item(i);
                    if (item && item->data(Qt::UserRole).toString() == filePath) {
                        m_currentlyDisplayedSlideItem = item;
                        break;
                    }
                }
            }

            emit setItemHighlighted(m_currentlyDisplayedSlideItem, true);
            currentImageItem->setPixmap(pixmap);
            currentImageItem->show();

            QTimer::singleShot(0, this, &ImageViewController::applyFitMode);

            if (itemIndex != -1) {
                updateViewControlSliderState(itemIndex, allFiles.count());
            }

            if (!filePath.isEmpty()) {
                emit currentImageChanged(filePath);
            }
        }
    }

    updateZoomState();
}

void ImageViewController::setupPictureScroll(const QStringList& files)
{
    QRect viewRect = m_view->viewport()->rect();

    qDebug() << "\n=== [DEBUG] setupPictureScroll Start ===";
    qDebug() << "Viewport Rect:" << viewRect;
    qDebug() << "Direction:" << (m_slideDirection == DirectionHorizontal ? "Horizontal" : "Vertical");
    qDebug() << "Layout:" << (m_layoutDirection == LayoutDirection::Forward ? "Forward" : "Backward");

    if (viewRect.isEmpty()) {
        qDebug() << "Viewport rect is empty. Aborting.";
        return;
    }

    // クリーンアップ
    for (int i = 0; i < m_slides.size(); ++i) {
        if (m_slides[i].item) delete m_slides[i].item;
        cleanupPanoramaMovie(i); // ★ ここでムービーも削除
    }
    m_slides.clear();
    m_panoramaMovies.clear();

    if (files.isEmpty()) {
        mediaScene->setSceneRect(viewRect);
        return;
    }

    // 1. サイズ計算フェーズ
    QList<QSize> renderedSizes;
    int totalLength = 0;

    for (int i = 0; i < files.size(); ++i) {
        const QString& filePath = files.at(i);
        QImageReader reader(filePath);
        if (!reader.canRead()) {
            qDebug() << "Size Calc [" << i << "]: Cannot read file.";
            renderedSizes.append(QSize(0,0));
            continue;
        }
        QSize originalSize = reader.size();

        QSize renderedSize;
        if (m_slideDirection == DirectionHorizontal) {
            // 倍率計算 (浮動小数点で計算)
            qreal scale = (qreal)viewRect.height() / (qreal)originalSize.height();
            // 幅を整数へ丸める
            int width = qRound(originalSize.width() * scale);
            renderedSize = QSize(width, viewRect.height());
            totalLength += width;

            // デバッグ: 個別サイズ
            // qDebug() << "Size Calc [" << i << "]: Org" << originalSize << "Scale" << scale << "-> Rendered" << renderedSize;
        } else {
            qreal scale = (qreal)viewRect.width() / (qreal)originalSize.width();
            int height = qRound(originalSize.height() * scale);
            renderedSize = QSize(viewRect.width(), height);
            totalLength += height;
        }
        renderedSizes.append(renderedSize);
    }

    qDebug() << "Total Length Calculated:" << totalLength;

    // 2. 配置計算フェーズ
    int currentPos;
    int padding;

    if (m_slideDirection == DirectionHorizontal) {
        padding = viewRect.width() / 2;
        currentPos = (m_layoutDirection == LayoutDirection::Backward) ? totalLength + padding : padding;
    } else {
        padding = viewRect.height() / 2;
        currentPos = (m_layoutDirection == LayoutDirection::Backward) ? totalLength + padding : padding;
    }

    // 隣接チェック用変数
    int previousEdgePos = -999999;
    bool isFirstItem = true;

    for (int i = 0; i < files.size(); ++i) {
        SlideInfo info;
        info.filePath = files.at(i);

        QSize renderedSize = renderedSizes.at(i);
        if (renderedSize.isEmpty()) continue;

        int itemStartPos = 0; // このアイテムの描画開始位置
        int itemEndPos = 0;   // このアイテムの描画終了位置

        if (m_slideDirection == DirectionHorizontal) {
            if (m_layoutDirection == LayoutDirection::Backward) {
                // Backward: currentPos は「右端」から「左」へ進む
                // 配置: (currentPos - width) が左端
                currentPos -= renderedSize.width();
                info.geometry = QRectF(currentPos, 0, renderedSize.width(), renderedSize.height());

                // 境界チェック用
                itemStartPos = currentPos + renderedSize.width(); // 右端
                itemEndPos = currentPos; // 左端 (次のアイテムの右端になるべき場所)

                // デバッグ: 隙間チェック (Backwardなので、前のアイテムの左端 == 今のアイテムの右端)
                if (!isFirstItem) {
                    int gap = previousEdgePos - itemStartPos;
                    if (gap != 0) qDebug() << "!!! GAP DETECTED [" << i-1 << "->" << i << "] Gap:" << gap << "px";
                }
                previousEdgePos = itemEndPos;

            } else {
                // Forward: currentPos は「左端」から「右」へ進む
                info.geometry = QRectF(currentPos, 0, renderedSize.width(), renderedSize.height());

                itemStartPos = currentPos;
                itemEndPos = currentPos + renderedSize.width();

                currentPos += renderedSize.width();

                // デバッグ: 隙間チェック (前のアイテムの右端 == 今のアイテムの左端)
                if (!isFirstItem) {
                    int gap = itemStartPos - previousEdgePos;
                    if (gap != 0) qDebug() << "!!! GAP DETECTED [" << i-1 << "->" << i << "] Gap:" << gap << "px";
                }
                previousEdgePos = itemEndPos;
            }
        } else {
            // Vertical (同様のロジック)
            if (m_layoutDirection == LayoutDirection::Backward) {
                currentPos -= renderedSize.height();
                info.geometry = QRectF(0, currentPos, renderedSize.width(), renderedSize.height());

                itemStartPos = currentPos + renderedSize.height();
                itemEndPos = currentPos;

                if (!isFirstItem) {
                    int gap = previousEdgePos - itemStartPos;
                    if (gap != 0) qDebug() << "!!! GAP DETECTED [" << i-1 << "->" << i << "] Gap:" << gap << "px";
                }
                previousEdgePos = itemEndPos;

            } else {
                info.geometry = QRectF(0, currentPos, renderedSize.width(), renderedSize.height());

                itemStartPos = currentPos;
                itemEndPos = currentPos + renderedSize.height();

                currentPos += renderedSize.height();

                if (!isFirstItem) {
                    int gap = itemStartPos - previousEdgePos;
                    if (gap != 0) qDebug() << "!!! GAP DETECTED [" << i-1 << "->" << i << "] Gap:" << gap << "px";
                }
                previousEdgePos = itemEndPos;
            }
        }

        // 詳細な座標ログが必要な場合はコメントアウトを外す
        // qDebug() << "Item" << i << "Geometry:" << info.geometry << " (Int Rect:" << info.geometry.toRect() << ")";

        m_slides.append(info);
        isFirstItem = false;
    }

    // シーン矩形の設定ログ
    QRectF sceneRect;
    if (m_slideDirection == DirectionHorizontal) {
        sceneRect = QRectF(0, 0, totalLength + padding * 2, viewRect.height());
    } else {
        sceneRect = QRectF(0, 0, viewRect.width(), totalLength + padding * 2);
    }
    mediaScene->setSceneRect(sceneRect);

    qDebug() << "Scene Rect Set To:" << sceneRect;
    qDebug() << "=== [DEBUG] setupPictureScroll End ===\n";
}

void ImageViewController::positionScrollAtIndex(int index)
{
    if (m_slides.isEmpty() || index < 0 || index >= m_slides.size()) {
        return;
    }
    loadSlidesAround(index);
    QPointF targetCenter = m_slides.at(index).geometry.center();
    m_view->centerOn(targetCenter); // ui->mediaView -> m_view
    m_view->update();
}

void ImageViewController::rebuildPanoramaOnResize()
{
    qDebug() << "[Refresh] Refreshing panorama view state.";
    if (!m_currentSlideshowList || m_currentSlideshowList->count() == 0) return;

    int currentIndex = m_currentSlideshowList->currentRow();
    if (currentIndex < 0) currentIndex = 0;

    positionScrollAtIndex(currentIndex);
}

void ImageViewController::applyFitMode()
{
    if (!currentImageItem || currentImageItem->pixmap().isNull()) return;
    QRectF viewRect = m_view->viewport()->rect(); // ui->mediaView -> m_view
    QRectF pixmapRect = currentImageItem->pixmap().rect();
    if (viewRect.isEmpty() || pixmapRect.isEmpty()) return;

    switch (m_fitMode) {
    case FitToWidth:
        m_fitScale = viewRect.width() / pixmapRect.width();
        break;
    case FitToHeight:
        m_fitScale = viewRect.height() / pixmapRect.height();
        break;
    case FitInside:
    default:
        m_fitScale = qMin(viewRect.width() / pixmapRect.width(),
                          viewRect.height() / pixmapRect.height());
        break;
    }
    m_userZoomFactor = 1.0;
    updateViewFit();
}

QString ImageViewController::getCurrentFilePath() const
{
    const QStringList allFiles = getActiveImageList();
    if (allFiles.isEmpty()) return QString();

    int index = -1;

    // パノラマモード: スライダーの値が現在のインデックス
    if (m_slideshowMode == ModePictureScroll) {
        index = m_viewControlSlider->value();
    }
    // 標準モード: 表示中のアイテムから特定、失敗したらファイル名から逆引き
    else {
        // 現在表示中のパスがあればそれを使う (displayMediaで更新されている前提)
        // ※この変数が定義されていない場合は、以下のファイル名検索にフォールバックします

        // ファイル名入力欄のテキストからリスト内を検索
        QString currentName = m_filenameEdit->text();
        for(int i=0; i<allFiles.size(); ++i) {
            if (QFileInfo(allFiles[i]).fileName() == currentName) {
                index = i;
                break;
            }
        }
    }

    if (index >= 0 && index < allFiles.size()) {
        qDebug() << "[IVC] getCurrentFilePath found index" << index << ":" << allFiles.at(index);
        return allFiles.at(index);
    }

    qDebug() << "[IVC] getCurrentFilePath failed to find current file.";
    return QString();
}

void ImageViewController::onTogglePanoramaMode()
{
    if (!m_currentSlideshowList) return;

    bool slideshowWasActive = slideshowTimer->isActive();
    if (slideshowWasActive) {
        slideshowTimer->stop();
        slideshowProgressTimer->stop();
    }

    int finalIndex = -1;

    QScrollBar* currentScrollBar = nullptr;
    if (m_slideshowMode == ModePictureScroll) {
        currentScrollBar = (m_slideDirection == DirectionHorizontal) ? m_view->horizontalScrollBar() : m_view->verticalScrollBar();
    }
    if (currentScrollBar) {
        disconnect(currentScrollBar, &QScrollBar::valueChanged, this, &ImageViewController::handlePanoramaScrollChanged);
    }

    m_scrollIndexUpdateTimer->stop();

    if (m_slideshowMode == ModeStandard) {
        m_slideshowMode = ModePictureScroll;
        if (auto* pView = qobject_cast<PanoramaView*>(m_view)) {
            pView->setPanoramaMode(true);
        }
        m_view->resetTransform();

        m_standardLastFitMode = m_fitMode;
        m_fitMode = m_panoramaLastFitMode;
        if (m_fitMode == FitInside) { m_fitMode = FitToWidth; }

        // ★修正: 方向を確定させ、新しいスクロールバーを監視対象にする
        m_slideDirection = (m_fitMode == FitToHeight) ? DirectionHorizontal : DirectionVertical;
        QScrollBar* newScrollBar = (m_slideDirection == DirectionHorizontal) ? m_view->horizontalScrollBar() : m_view->verticalScrollBar();
        connect(newScrollBar, &QScrollBar::valueChanged, this, &ImageViewController::handlePanoramaScrollChanged);

        qDebug() << "[IVC Panorama] Toggling Standard -> Panorama";

        const QStringList allFiles = getActiveImageList();

        if (!allFiles.isEmpty()) {
            int currentIndex = 0;
            if (slideshowWasActive) {
                currentIndex = slideshowCurrentIndex;
            } else {
                currentIndex = m_viewControlSlider->value(); // ui->viewControlSlider -> m_viewControlSlider
            }
            if (currentIndex < 0) currentIndex = 0;
            finalIndex = currentIndex;

            setupPictureScroll(allFiles);
            positionScrollAtIndex(currentIndex);
        }
        currentImageItem->hide();

    } else {
        qDebug() << "[IVC Panorama] Toggling Panorama -> Standard";
        m_slideshowMode = ModeStandard;
        if (auto* pView = qobject_cast<PanoramaView*>(m_view)) {
            pView->setPanoramaMode(false);
        }
        m_view->resetTransform();

        m_panoramaLastFitMode = m_fitMode;
        m_standardLastFitMode = m_fitMode;

        int currentIndex = m_viewControlSlider->value();
        finalIndex = currentIndex;

        for (const SlideInfo& slide : m_slides) {
            if (slide.item) {
                mediaScene->removeItem(slide.item);
                delete slide.item;
            }
        }
        m_slides.clear();

        const QStringList allFiles = getActiveImageList();

        if (!allFiles.isEmpty()) {
            if (finalIndex < 0 || finalIndex >= allFiles.size()) {
                finalIndex = 0;
            }
            displayMedia(allFiles.at(finalIndex));

            if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
                QString path = allFiles.at(finalIndex);
                for (int i = 0; i < m_currentSlideshowList->count(); ++i) {
                    if (m_currentSlideshowList->item(i)->data(Qt::UserRole).toString() == path) {
                        m_currentSlideshowList->setCurrentItem(m_currentSlideshowList->item(i));
                        break;
                    }
                }
            }
        } else {
            displayMedia(QString());
        }
        currentImageItem->show();
    }

    emit mediaViewStatesChanged();

    if (slideshowWasActive) {
        slideshowCurrentIndex = finalIndex;
        int interval_ms = static_cast<int>(m_slideshowInterval * 1000.0);
        if (interval_ms > 0) {
            slideshowTimer->start(interval_ms);
            slideshowElapsedTimer.restart();
            slideshowProgressTimer->start(16);
        }
        emit mediaViewStatesChanged();
    }
}

void ImageViewController::onToggleFitMode()
{
    FitMode nextMode = m_fitMode;
    switch (m_fitMode) {
    case FitInside: nextMode = FitToHeight; break;
    case FitToHeight: nextMode = FitToWidth; break;
    case FitToWidth: nextMode = FitInside; break;
    }

    if (m_slideshowMode == ModePictureScroll && nextMode == FitInside) {
        nextMode = FitToHeight;
    }
    m_fitMode = nextMode;

    if (m_slideshowMode == ModePictureScroll) {
        // FitMode変更によりスクロール方向が変わる場合、監視するスクロールバーを付け替える
        QScrollBar* oldScrollBar = (m_slideDirection == DirectionHorizontal) ? m_view->horizontalScrollBar() : m_view->verticalScrollBar();

        m_slideDirection = (m_fitMode == FitToHeight) ? DirectionHorizontal : DirectionVertical;

        QScrollBar* newScrollBar = (m_slideDirection == DirectionHorizontal) ? m_view->horizontalScrollBar() : m_view->verticalScrollBar();

        if (oldScrollBar && newScrollBar && oldScrollBar != newScrollBar) {
            disconnect(oldScrollBar, &QScrollBar::valueChanged, this, &ImageViewController::handlePanoramaScrollChanged);
            connect(newScrollBar, &QScrollBar::valueChanged, this, &ImageViewController::handlePanoramaScrollChanged);
        }
        m_scrollIndexUpdateTimer->stop();

        const QStringList allFiles = getActiveImageList();

        if (!allFiles.isEmpty()) {
            int currentIndex = m_viewControlSlider->value();
            if (currentIndex < 0 || currentIndex >= allFiles.size()) currentIndex = 0;

            setupPictureScroll(allFiles);
            positionScrollAtIndex(currentIndex);
        }
    }

    emit mediaViewStatesChanged();
    applyFitMode();
}

void ImageViewController::onLayoutDirectionToggled()
{
    m_layoutDirection = (m_layoutDirection == LayoutDirection::Forward) ? LayoutDirection::Backward : LayoutDirection::Forward;
    emit mediaViewStatesChanged();
    const QStringList allFiles = getActiveImageList();

    if (!allFiles.isEmpty()) {
        int currentIndex = m_viewControlSlider->value(); // ui->viewControlSlider -> m_viewControlSlider
        if (currentIndex < 0 || currentIndex >= allFiles.size()) currentIndex = 0;

        setupPictureScroll(allFiles);
        positionScrollAtIndex(currentIndex);
    }
}

void ImageViewController::onViewControlSliderMoved(int value)
{
    qDebug() << "[IVC Slider] Slider moved to value:" << value;
    const QStringList allFiles = getActiveImageList();
    if (allFiles.isEmpty()) {
        return;
    }

    if (m_slideshowMode == ModePictureScroll) {
        scrollToImage(value);
        if (value >= 0 && value < allFiles.size()) {
            QString path = allFiles.at(value);

            // 1. ファイル名表示 (LineEdit) を更新
            m_filenameEdit->setText(QFileInfo(path).fileName());

            // 2. 本棚や履歴のためにシグナルを発信
            emit currentImageChanged(path);
        }

        if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
            if (value >= 0 && value < allFiles.size()) {
                QString path = allFiles.at(value);
                QListWidgetItem* itemToHighlight = nullptr;
                for (int i = 0; i < m_currentSlideshowList->count(); ++i) {
                    if (m_currentSlideshowList->item(i)->data(Qt::UserRole).toString() == path) {
                        itemToHighlight = m_currentSlideshowList->item(i);
                        break;
                    }
                }
                if (itemToHighlight && m_currentSlideshowList->currentRow() != m_currentSlideshowList->row(itemToHighlight)) {
                    emit setItemHighlighted(m_currentlyDisplayedSlideItem, false);
                    m_currentlyDisplayedSlideItem = itemToHighlight;
                    emit setItemHighlighted(m_currentlyDisplayedSlideItem, true);
                }
            }
        }
        if (slideshowTimer->isActive()) {
            slideshowCurrentIndex = value;
            int interval_ms = static_cast<int>(m_slideshowInterval * 1000.0);
            if (interval_ms > 0) {
                slideshowTimer->start(interval_ms);
                slideshowElapsedTimer.restart();
                m_progressBar->setValue(0); // ui->slideshowProgressBar -> m_progressBar
            }
        }
    } else {
        int newIndex = value;
        if (newIndex >= 0 && newIndex < allFiles.size()) {
            if (slideshowTimer->isActive()) {
                slideshowCurrentIndex = newIndex;
                showNextSlide(true);
                int interval_ms = static_cast<int>(m_slideshowInterval * 1000.0);
                if (interval_ms > 0) {
                    slideshowTimer->start(interval_ms);
                    slideshowElapsedTimer.restart();
                    m_progressBar->setValue(0); // ui->slideshowProgressBar -> m_progressBar
                }
            } else {
                QString path = allFiles.at(newIndex);
                displayMedia(path);
            }
        }
    }
}

void ImageViewController::onZoomSpinBoxChanged(int value)
{
    if (m_zoomSpinBox->signalsBlocked()) return; // ui->zoomSpinBox -> m_zoomSpinBox

    if (m_slideshowMode == ModePictureScroll) {
        qreal currentScale = m_view->transform().m11(); // ui->mediaView -> m_view
        if (qFuzzyCompare(currentScale, 0.0)) return;
        qreal newAbsoluteScale = value / 100.0;
        qreal factor = newAbsoluteScale / currentScale;
        {
            ViewUpdateGuard guard(this);
            m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter); // ui->mediaView -> m_view
            m_view->scale(factor, factor);
        }
    } else {
        m_userZoomFactor = value / 100.0;
        updateViewFit();
    }
}

void ImageViewController::setSlideshowEffect(int index)
{
    if (index == 0) m_slideshowEffect = EffectNone;
    else if (index == 1) m_slideshowEffect = EffectFade;
    else if (index == 2) m_slideshowEffect = EffectSlide;
}

void ImageViewController::setSlideshowInterval(double seconds)
{
    if (seconds <= 0) return;

    m_slideshowInterval = seconds;
    qDebug() << "[IVC] Slideshow interval changed to:" << m_slideshowInterval;

    if (slideshowTimer->isActive()) {
        int interval_ms = static_cast<int>(m_slideshowInterval * 1000.0);
        slideshowTimer->start(interval_ms);
        slideshowElapsedTimer.restart();
        m_progressBar->setValue(0); // ui->slideshowProgressBar -> m_progressBar
    }
}

void ImageViewController::setSlideshowList(QListWidget* listWidget)
{
    m_currentSlideshowList = listWidget;
    onSlideshowPlaylistChanged();
}

void ImageViewController::setSortOrder(SortMode mode, bool ascending)
{
    // 変更がない場合は何もしない (シャッフル以外)
    if (m_currentSortMode == mode && m_sortAscending == ascending && mode != SortShuffle) {
        return;
    }

    m_currentSortMode = mode;
    m_sortAscending = ascending;

    // 現在表示中のファイルを維持しつつ、リストを再構築してリロード
    if (!m_currentBrowsePath.isEmpty()) {
        QString currentFile = getCurrentFilePath();
        browseTo(m_currentBrowsePath, currentFile, false);
    }
}

void ImageViewController::setLoading(bool loading)
{
    if (loading) {
        m_loadingLabel->show();
        m_emptyDirectoryLabel->hide();
        updateOverlayLayout(); // ★追加: 表示した瞬間に位置合わせ
    } else {
        m_loadingLabel->hide();

        if (currentImageItem->pixmap().isNull() && getActiveImageList().isEmpty()) {
            m_emptyDirectoryLabel->show();
            updateOverlayLayout(); // ★追加: 表示した瞬間に位置合わせ
        } else {
            m_emptyDirectoryLabel->hide();
        }
    }
}

void ImageViewController::setViewStates(bool isPanorama, FitMode fitMode, LayoutDirection layoutDirection)
{
    // 既存の接続を解除（念のため）
    if (m_slideshowMode == ModePictureScroll) {
        QScrollBar* oldBar = (m_slideDirection == DirectionHorizontal) ? m_view->horizontalScrollBar() : m_view->verticalScrollBar();
        disconnect(oldBar, &QScrollBar::valueChanged, this, &ImageViewController::handlePanoramaScrollChanged);
    }

    m_slideshowMode = isPanorama ? ModePictureScroll : ModeStandard;
    m_fitMode = fitMode;
    m_layoutDirection = layoutDirection;

    if (auto* pView = qobject_cast<PanoramaView*>(m_view)) {
        pView->setPanoramaMode(isPanorama);
    }

    if (isPanorama) {
        currentImageItem->hide();

        // ★修正: パノラマモードなら、FitModeに基づいて方向を確定し、スクロールバーを監視する
        if (m_fitMode == FitInside) m_fitMode = FitToWidth;
        m_slideDirection = (m_fitMode == FitToHeight) ? DirectionHorizontal : DirectionVertical;

        QScrollBar* newBar = (m_slideDirection == DirectionHorizontal) ? m_view->horizontalScrollBar() : m_view->verticalScrollBar();
        connect(newBar, &QScrollBar::valueChanged, this, &ImageViewController::handlePanoramaScrollChanged);

    } else {
        currentImageItem->show();
        for (const auto& slide : m_slides) {
            if (slide.item) {
                mediaScene->removeItem(slide.item);
                delete slide.item;
            }
        }
        m_slides.clear();
        m_scrollIndexUpdateTimer->stop();
    }

    if (m_slideshowMode == ModeStandard) {
        m_standardLastFitMode = m_fitMode;
    } else {
        m_panoramaLastFitMode = m_fitMode;
    }

    updateViewFit();
    emit mediaViewStatesChanged();
}

void ImageViewController::toggleSlideshow()
{
    if (slideshowTimer->isActive()) {
        // --- 停止処理 (変更なし) ---
        slideshowTimer->stop();
        slideshowProgressTimer->stop();
        m_progressBar->hide();
        m_progressBar->setValue(0);

        handleResize();

        if (m_slideshowMode == ModePictureScroll) {
            rebuildPanoramaOnResize();
        }
        emit mediaViewStatesChanged();
        return;
    }

    // --- 開始処理 ---

    const QStringList allFiles = getActiveImageList();
    if (allFiles.isEmpty()) return;

    // インデックスの決定 (変更なし)
    if (m_slideshowMode == ModePictureScroll) {
        slideshowCurrentIndex = m_viewControlSlider->value();
    } else {
        if (m_viewMode == ModeSlideshowList && m_currentSlideshowList && m_currentSlideshowList->currentItem()) {
            slideshowCurrentIndex = allFiles.indexOf(m_currentSlideshowList->currentItem()->data(Qt::UserRole).toString());
        } else if (!currentImageItem->pixmap().isNull()) {
            slideshowCurrentIndex = allFiles.indexOf(m_filenameEdit->text());
        } else {
            slideshowCurrentIndex = 0;
        }
    }

    if (slideshowCurrentIndex < 0 || slideshowCurrentIndex >= allFiles.size()) {
        slideshowCurrentIndex = 0;
    }

    // 1. まずプログレスバーを表示
    m_progressBar->show();

    // 2. ここでレイアウト変更を強制的に完了させる
    // これにより mediaView がプログレスバー分だけ縮んだ最終的なサイズになります
    QApplication::processEvents();

    if (m_slideshowMode == ModePictureScroll) {
        // パノラマモード: 正しいサイズで一度だけ構築
        setupPictureScroll(allFiles);
    } else {
        // 標準モード: 即座にフィットさせる
        applyFitMode();
    }

    showNextSlide(true);

    int interval_ms = static_cast<int>(m_slideshowInterval * 1000.0);
    if (interval_ms > 0) {
        slideshowTimer->start(interval_ms);
        slideshowElapsedTimer.restart();
        slideshowProgressTimer->start(16);
    }
    emit mediaViewStatesChanged();
}


void ImageViewController::handlePanoramaScrollChanged()
{
    m_scrollIndexUpdateTimer->start();
}

void ImageViewController::handleResize(bool forceImmediate)
{
    if (forceImmediate) {
        // タイマーが動いていれば止める
        if (m_resizeTimer->isActive()) {
            m_resizeTimer->stop();
        }
        // 直接リサイズ処理を呼ぶ (関数名は実装によりますが、タイマーの接続先と同じもの)
        // 例: onResizeTimeout() や updateView() など、タイマー満了時に呼んでいる関数
        onResizeTimeout();
    } else {
        // 既存のタイマー処理 (ドラッグリサイズ用)
        m_resizeTimer->start(); // 再スタート(リセット)
    }
}

void ImageViewController::handleWheelEvent(QWheelEvent *wheelEvent, QWidget* viewport)
{
    int delta = 0;
    if (!wheelEvent->pixelDelta().isNull()) delta = wheelEvent->pixelDelta().y();
    else if (!wheelEvent->angleDelta().isNull()) delta = wheelEvent->angleDelta().y();
    if (delta == 0) return;

    if (wheelEvent->modifiers() & Qt::ControlModifier) {
        // ズーム処理 (変更なし)
        if (m_slideshowMode == ModePictureScroll) {
            int currentPercent = m_zoomSpinBox->value();
            int nextPercent = (delta > 0) ? currentPercent + 5 : currentPercent - 5;
            if (nextPercent >= m_zoomSpinBox->minimum() && nextPercent <= m_zoomSpinBox->maximum()) {
                onZoomSpinBoxChanged(nextPercent);
                m_zoomSpinBox->setValue(nextPercent);
            }
        } else {
            if (!currentImageItem->pixmap().isNull()) {
                int currentPercent = qRound(m_userZoomFactor * 100.0);
                int nextPercent = (delta > 0) ? currentPercent + 5 : currentPercent - 5;
                if (nextPercent >= m_zoomSpinBox->minimum() && nextPercent <= m_zoomSpinBox->maximum()) {
                    m_userZoomFactor = nextPercent / 100.0;
                    updateViewFit();
                }
            }
        }
    } else {
        // スクロール処理
        int step = (delta < 0) ? 1 : -1;

        if (m_slideshowMode == ModePictureScroll) {
            if (m_layoutDirection == LayoutDirection::Backward) step *= -1;

            // ★ 修正: 共通関数を呼ぶだけにする
            stepByImage(step);

        } else {
            // 標準モード
            if (step > 0) m_viewControlSlider->triggerAction(QAbstractSlider::SliderSingleStepAdd);
            else m_viewControlSlider->triggerAction(QAbstractSlider::SliderSingleStepSub);
        }
    }
}

void ImageViewController::handleKeyPressEvent(QKeyEvent *event)
{
    if (m_slideshowMode != ModePictureScroll || m_slides.isEmpty()) {
        event->ignore();
        return;
    }

    int step = 0;
    bool keyHandled = true;

    if (m_slideDirection == DirectionHorizontal) {
        switch (event->key()) {
        case Qt::Key_Left:  step = -1; break;
        case Qt::Key_Right: step = 1; break;
        default:            keyHandled = false; break;
        }
    } else {
        switch (event->key()) {
        case Qt::Key_Up:    step = -1; break;
        case Qt::Key_Down:  step = 1; break;
        default:            keyHandled = false; break;
        }
    }

    if (keyHandled) {
        if (m_layoutDirection == LayoutDirection::Backward) step *= -1;

        // ★ 修正: 共通関数を使用
        stepByImage(step);

        event->accept();
    } else {
        event->ignore();
    }
}

void ImageViewController::onImageLoaded(const AsyncLoadResult& result)
{
    // ロード中フラグを解除
    m_loadingIndices.remove(result.index);

    // インデックスの有効性チェック
    if (result.index < 0 || result.index >= m_slides.size()) return;

    // ユーザーが高速スクロールして、既に画面外になっていたら追加しない（メモリ節約）
    // ただし、キャッシュとして保持したい場合は追加しても良い。
    // ここでは、現在の loadSlidesAround の範囲ロジックと整合性を取るため、
    // 生成してシーンに追加し、次回の loadSlidesAround 呼び出し時に範囲外なら消えるようにします。
    // (または、ここで範囲チェックをして生成をスキップするのもありです)

    // 既にアイテムが存在する場合は何もしない（二重追加防止）
    if (m_slides[result.index].item != nullptr) return;

    if (result.success) {
        // メインスレッドで QPixmap に変換 (QPixmapはメインスレッドでしか扱えない)
        QPixmap pixmap = QPixmap::fromImage(result.image);

        QGraphicsPixmapItem* item = new QGraphicsPixmapItem(pixmap);
        item->setTransformationMode(Qt::SmoothTransformation);
        item->setPos(m_slides[result.index].geometry.topLeft());

        // シーンに追加
        mediaScene->addItem(item);
        m_slides[result.index].item = item;
    }
}

void ImageViewController::onSlideshowSelectionChanged()
{
    if (!m_currentSlideshowList || !m_currentSlideshowList->currentItem()) return;

    if (m_currentSlideshowList->count() == 1) {
        QString filePath = m_currentSlideshowList->currentItem()->data(Qt::UserRole).toString();
        if (m_slideshowMode == ModePictureScroll) {
            setupPictureScroll({filePath});
            positionScrollAtIndex(0);
        } else {
            displayMedia(filePath);
        }
        return;
    }

    int newIndex = m_currentSlideshowList->currentRow();
    if (newIndex < 0) return;

    if (slideshowTimer->isActive()) {
        if (QApplication::mouseButtons() & Qt::LeftButton) {
            slideshowCurrentIndex = newIndex;
            showNextSlide(true);
            slideshowTimer->start(static_cast<int>(m_intervalSpinBox->value() * 1000.0)); // ui->slideshowIntervalSpinBox -> m_intervalSpinBox
            slideshowElapsedTimer.restart();
            m_progressBar->setValue(0); // ui->slideshowProgressBar -> m_progressBar
        }
    } else {
        QString filePath = m_currentSlideshowList->currentItem()->data(Qt::UserRole).toString();
        m_filenameEdit->setText(QFileInfo(filePath).fileName()); // ui->filenameLineEdit -> m_filenameEdit
    }
}

void ImageViewController::onSlideshowListDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    qDebug() << "[IVC DblClick] Item double-clicked:" << item->data(Qt::UserRole).toString();

    if (m_slideshowMode == ModePictureScroll) {
        qDebug() << "[IVC DblClick] Panorama mode is active.";
        const QStringList allFiles = getActiveImageList();
        int newIndex = allFiles.indexOf(item->data(Qt::UserRole).toString());
        qDebug() << "[IVC DblClick] Filtered index is:" << newIndex;

        if (newIndex == -1) {
            qDebug() << "[IVC DblClick] ERROR: Item (folder?) not found in filtered list.";
            return;
        }

        positionScrollAtIndex(newIndex);
        emit setItemHighlighted(m_currentlyDisplayedSlideItem, false);
        m_currentlyDisplayedSlideItem = item;
        emit setItemHighlighted(m_currentlyDisplayedSlideItem, true);
        m_viewControlSlider->setValue(newIndex); // ui->viewControlSlider -> m_viewControlSlider
    } else {
        qDebug() << "[IVC DblClick] Standard mode is active. Calling displayMedia.";
        displayMedia(item->data(Qt::UserRole).toString());
    }
}

void ImageViewController::onSlideshowPlaylistChanged()
{
    const QStringList allFiles = getActiveImageList();

    if (!slideshowTimer->isActive()) {
        updateViewControlSliderState(-1, allFiles.count());
        return;
    }

    qDebug() << "Slideshow playlist changed, restarting slideshow...";
    slideshowTimer->stop();
    slideshowProgressTimer->stop();


    if (allFiles.isEmpty()) {
        m_progressBar->hide(); // ui->slideshowProgressBar -> m_progressBar
        emit mediaViewStatesChanged();
        m_progressBar->setValue(0);
        if (m_slideshowMode == ModeStandard) currentImageItem->setPixmap(QPixmap());
        handleResize();
        return;
    }

    if (slideshowCurrentIndex >= allFiles.size()) slideshowCurrentIndex = allFiles.size() - 1;
    if (slideshowCurrentIndex < 0) slideshowCurrentIndex = 0;

    if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
        for (int i = 0; i < m_currentSlideshowList->count(); ++i) {
            if (m_currentSlideshowList->item(i)->data(Qt::UserRole).toString() == allFiles.at(slideshowCurrentIndex)) {
                m_currentSlideshowList->setCurrentItem(m_currentSlideshowList->item(i));
                break;
            }
        }
    }

    if (m_slideshowMode == ModePictureScroll) {
        QApplication::processEvents();
        setupPictureScroll(allFiles);
        positionScrollAtIndex(slideshowCurrentIndex);
    } else {
        showNextSlide(true);
    }

    slideshowElapsedTimer.restart();
    m_progressBar->setValue(0);
    int interval_ms = static_cast<int>(m_slideshowInterval * 1000.0);
    if (interval_ms > 0) {
        slideshowTimer->start(interval_ms);
        slideshowProgressTimer->start(16);
    }

    if (m_slideshowMode == ModePictureScroll) {
        const QStringList allFiles = getActiveImageList();
        setupPictureScroll(allFiles);
        // 現在位置を維持するか、0に戻すか
        if (slideshowCurrentIndex >= allFiles.size()) slideshowCurrentIndex = allFiles.size() - 1;
        positionScrollAtIndex(slideshowCurrentIndex);
    }
}

void ImageViewController::onResizeTimeout()
{
    qDebug() << "[IVC Resize] Resize timer timed out. Rebuilding panorama.";

    const QStringList allFiles = getActiveImageList();

    // 1. 画像がある場合の処理 (既存)
    if (!allFiles.isEmpty()) {
        int currentIndex = m_viewControlSlider->value(); // ui->viewControlSlider -> m_viewControlSlider
        setupPictureScroll(allFiles);
        positionScrollAtIndex(currentIndex);
    }

    // 2. オーバーレイの位置を更新 (画像がない場合のラベルや、ローディング表示の位置合わせ)
    // ★ 修正: 手動計算をやめ、ヘルパー関数を使用することで中央寄せを確実にします
    updateOverlayLayout();
}

void ImageViewController::onFilenameEntered()
{
    QString newName = m_filenameEdit->text(); // ui->filenameLineEdit -> m_filenameEdit
    m_filenameEdit->setReadOnly(true);

    const QStringList& allFiles = getActiveImageList();
    QString foundPath;
    int foundIndex = -1;

    for (int i = 0; i < allFiles.size(); ++i) {
        if (QFileInfo(allFiles[i]).fileName() == newName) {
            foundPath = allFiles[i];
            foundIndex = i;
            break;
        }
    }

    if (foundIndex != -1) {
        qDebug() << "[IVC FilenameSearch] Found:" << foundPath;
        if (m_slideshowMode == ModePictureScroll) {
            scrollToImage(foundIndex);
        } else {
            displayMedia(foundPath);
        }
    } else {
        qDebug() << "[IVC FilenameSearch] Not found:" << newName;

        QLabel *toolTipLabel = new QLabel("指定のファイルが見つかりません", m_filenameEdit, Qt::ToolTip | Qt::FramelessWindowHint); // parent changed

        toolTipLabel->setStyleSheet(
            "QLabel {"
            "    background-color: #333333;"
            "    color: white;"
            "    border: 1px solid #FFFFFF;"
            "    padding: 4px;"
            "    border-radius: 3px;"
            "}"
            );
        toolTipLabel->adjustSize();

        QPoint pos = m_filenameEdit->mapToGlobal(QPoint(0, m_filenameEdit->height())); // ui->filenameLineEdit -> m_filenameEdit
        toolTipLabel->move(pos);
        toolTipLabel->show();

        QTimer::singleShot(2000, toolTipLabel, &QLabel::deleteLater);

        if (m_slideshowMode == ModePictureScroll) {
            int currentIndex = m_viewControlSlider->value(); // ui->viewControlSlider -> m_viewControlSlider
            if (currentIndex >= 0 && currentIndex < m_slides.size()) {
                m_filenameEdit->setText(QFileInfo(m_slides.at(currentIndex).filePath).fileName()); // ui->filenameLineEdit -> m_filenameEdit
            }
        } else {
            if (!currentImageItem->pixmap().isNull()) {
                int currentIndex = m_viewControlSlider->value();
                if (currentIndex >= 0 && currentIndex < allFiles.size()) {
                    m_filenameEdit->setText(QFileInfo(allFiles.at(currentIndex)).fileName());
                }
            }
        }
    }
}

// --- 内部ヘルパー関数 --- (変更なし部分は省略) ---
void ImageViewController::cleanupPanoramaMovie(int index)
{
    if (m_panoramaMovies.contains(index)) {
        QMovie *movie = m_panoramaMovies.take(index); // マップから取り出して削除
        if (movie) {
            movie->stop();
            delete movie;
        }
    }
}

const QStringList ImageViewController::getActiveImageList() const
{
    if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
        QStringList files;
        int count = m_currentSlideshowList->count();
        for(int i=0; i<count; ++i) {
            QListWidgetItem* item = m_currentSlideshowList->item(i);
            if (item && !item->data(Qt::UserRole + 1).toBool()) {
                files << item->data(Qt::UserRole).toString();
            }
        }
        return files;
    } else {
        // ディレクトリ閲覧モード
        return m_directoryFiles;
    }
}

void ImageViewController::goBack()
{
    if (m_historyIndex > 0) {
        m_historyIndex--;
        QString path = m_history.at(m_historyIndex);
        browseTo(path, QString(), false); // 履歴には追加しない（戻るだけ）
    }
}

void ImageViewController::goForward()
{
    if (m_historyIndex < m_history.count() - 1) {
        m_historyIndex++;
        QString path = m_history.at(m_historyIndex);
        browseTo(path, QString(), false); // 履歴には追加しない
    }
}

void ImageViewController::goUp()
{
    QString parent = getParentPath(m_currentBrowsePath);
    if (!parent.isEmpty()) {
        browseTo(parent, QString(), true);
    }
}

bool ImageViewController::isAnimatedImage(const QString &path)
{
    QImageReader reader(path);

    // 1. そのフォーマットがアニメーションに対応しているか確認
    // (GIF, WEBP, APNG など)
    if (!reader.supportsAnimation()) {
        return false;
    }

    // 2. 実際にフレーム数が 1 より多いか確認
    // これにより、"動かないGIF" や "静止画WebP" は false になり、
    // 軽量な静止画処理 (QImage/QPixmap) に回されます。
    if (reader.imageCount() > 1) {
        return true;
    }

    return false;
}

void ImageViewController::loadDirectory(const QDir& dir, const QString& fileToSelectPath)
{
    browseTo(dir.absolutePath(), fileToSelectPath, true);
}

void ImageViewController::loadSlidesAround(int index)
{
    if (m_slides.isEmpty()) return;

    const int range = 5;
    int startIndex = qMax(0, index - range);
    int endIndex = qMin(m_slides.size() - 1, index + range);

    // 1. 範囲外のアイテムを解放
    for (int i = 0; i < m_slides.size(); ++i) {
        if ((i < startIndex || i > endIndex)) {
            if (m_slides[i].item) {
                mediaScene->removeItem(m_slides[i].item);
                delete m_slides[i].item;
                m_slides[i].item = nullptr;
            }
            // ★ 範囲外になったらムービーも停止してメモリ解放
            cleanupPanoramaMovie(i);
        }
    }

    // 2. 範囲内のアイテムをロード
    for (int i = startIndex; i <= endIndex; ++i) {
        // すでにアイテムがある、または現在ロード中なら何もしない
        if (m_slides[i].item != nullptr || m_loadingIndices.contains(i)) {
            continue;
        }

        QString path = m_slides[i].filePath;
        QSize targetSize = m_slides[i].geometry.size().toSize();
        if (targetSize.isEmpty()) continue;

        // ★ 分岐: GIFアニメーションかどうか判定
        if (isAnimatedImage(path)) {
            // --- GIFの場合: QMovieを使用 (メインスレッドで処理) ---

            // 既存のムービーがあれば削除（念のため）
            cleanupPanoramaMovie(i);

            QMovie *movie = new QMovie(path);
            if (!movie->isValid()) {
                delete movie;
                continue; // 失敗したらスキップ（または静止画処理へ）
            }

            movie->setCacheMode(QMovie::CacheAll);
            // ★重要: QMovieにターゲットサイズを指定してスケーリングさせる
            movie->setScaledSize(targetSize);

            // アイテム作成
            QGraphicsPixmapItem* item = new QGraphicsPixmapItem();
            item->setTransformationMode(Qt::SmoothTransformation);
            item->setPos(m_slides[i].geometry.topLeft());
            mediaScene->addItem(item);
            m_slides[i].item = item;

            // フレーム更新シグナル
            connect(movie, &QMovie::frameChanged, this, [this, i]() {
                // インデックスの妥当性とアイテムの存在を確認
                if (m_panoramaMovies.contains(i) && i < m_slides.size() && m_slides[i].item) {
                    QMovie* m = m_panoramaMovies[i];
                    m_slides[i].item->setPixmap(m->currentPixmap());
                    // シーン更新
                    mediaScene->update();
                }
            });

            // 管理マップに追加して再生開始
            m_panoramaMovies.insert(i, movie);
            movie->start();

            // GIFの場合はここで完了とし、非同期ロードには行かない
            continue;
        }

        // --- 静止画の場合: 既存の非同期ロード処理 (QtConcurrent) ---

        m_loadingIndices.insert(i);

        QFuture<AsyncLoadResult> future = QtConcurrent::run([i, path, targetSize]() -> AsyncLoadResult {
            AsyncLoadResult result;
            result.index = i;
            result.success = false;

            if (targetSize.isEmpty()) return result;

            QImageReader reader(path);
            reader.setAutoTransform(true);
            reader.setScaledSize(targetSize);

            QImage image = reader.read();
            if (image.isNull()) return result;

            if (image.size() != targetSize) {
                image = image.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }

            result.image = image;
            result.success = true;
            return result;
        });

        auto* watcher = new QFutureWatcher<AsyncLoadResult>();
        connect(watcher, &QFutureWatcher<AsyncLoadResult>::finished, this, [this, watcher]() {
            AsyncLoadResult result = watcher->result();
            onImageLoaded(result);
            watcher->deleteLater();
        });
        watcher->setFuture(future);
    }
}

void ImageViewController::scrollToImage(int index)
{
    if (index < 0 || index >= m_slides.size() || m_slideshowMode != ModePictureScroll) {
        return;
    }

    // デバッグ: エフェクト設定と状態の確認
    // EffectSlide (1) になっていなければアニメーションしません
    qDebug() << "[IVC] scrollToImage. Index:" << index
             << "Logical:" << m_logicalTargetIndex
             << "IsProgrammatic:" << m_isProgrammaticScroll
             << "Effect:" << m_slideshowEffect;

    // 論理ターゲットを更新
    m_logicalTargetIndex = index;

    loadSlidesAround(index);

    // フラグ管理: 常にTrue
    m_isProgrammaticScroll = true;
    m_scrollIndexUpdateTimer->stop();

    QRectF targetSlideRect = m_slides.at(index).geometry;
    QScrollBar* scrollBar = (m_slideDirection == DirectionHorizontal)
                                ? m_view->horizontalScrollBar()
                                : m_view->verticalScrollBar();

    qreal scale = m_view->transform().m11();
    if (qFuzzyCompare(scale, 0.0)) scale = 1.0;

    qreal viewportSize = (m_slideDirection == DirectionHorizontal)
                             ? m_view->viewport()->width()
                             : m_view->viewport()->height();

    qreal targetSceneCenter = (m_slideDirection == DirectionHorizontal)
                                  ? targetSlideRect.center().x()
                                  : targetSlideRect.center().y();

    qreal targetViewCenter = targetSceneCenter * scale;
    qreal targetScrollValue = targetViewCenter - (viewportSize / 2.0);

    // ★ 修正: 既存アニメーションの確実な停止と破棄
    if (m_currentScrollAnim) {
        m_currentScrollAnim->stop();
        delete m_currentScrollAnim;
        m_currentScrollAnim = nullptr;
    }

    if (m_slideshowEffect == EffectSlide) {
        // 新しいアニメーションを作成
        m_currentScrollAnim = new QPropertyAnimation(scrollBar, "value", this);
        m_currentScrollAnim->setDuration(250);
        m_currentScrollAnim->setEndValue(targetScrollValue);
        m_currentScrollAnim->setEasingCurve(QEasingCurve::OutQuad);

        // 完了時の処理
        connect(m_currentScrollAnim, &QPropertyAnimation::finished, this, [this](){
            // アニメーションが完了したらポインタをクリア
            // (注: DeleteWhenStoppedによりオブジェクト自体は消えるため、ポインタをnullにする)
            m_currentScrollAnim = nullptr;

            // 少し待ってからフラグを下ろす (連打の切れ目を判定)
            QTimer::singleShot(50, this, [this](){
                // 次のアニメーションが始まっていなければ (ポインタがnullなら) 終了
                if (m_currentScrollAnim == nullptr) {
                    m_isProgrammaticScroll = false;
                    m_logicalTargetIndex = -1; // ここで初めてリセット
                    m_scrollIndexUpdateTimer->start();
                }
            });
        });

        // 削除はQtの親システム(this)に任せるか、明示的に行うが、
        // 今回は stop() -> delete を自分で行うため DeleteWhenStopped は使わない、
        // あるいは finished で deleteLater する。
        // ここでは安全のため、次の scrollToImage で delete する運用にします。
        m_currentScrollAnim->start();

    } else {
        // アニメーションなし
        scrollBar->setValue(targetScrollValue);
        QTimer::singleShot(50, this, [this](){
            m_isProgrammaticScroll = false;
            m_logicalTargetIndex = -1;
            m_scrollIndexUpdateTimer->start();
        });
    }
}

void ImageViewController::showNextSlide(bool isFirstSlide)
{
    const QStringList allFiles = getActiveImageList();
    if (allFiles.isEmpty()) { toggleSlideshow(); return; }

    if (!isFirstSlide) {
        slideshowElapsedTimer.restart();
        m_progressBar->setValue(0); // ui->slideshowProgressBar -> m_progressBar
        slideshowCurrentIndex++;
        if (slideshowCurrentIndex >= allFiles.size()) slideshowCurrentIndex = 0;
    }

    QString nextImagePath = allFiles.at(slideshowCurrentIndex);
    if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
        for (int i = 0; i < m_currentSlideshowList->count(); ++i) {
            if (m_currentSlideshowList->item(i)->data(Qt::UserRole).toString() == nextImagePath) {
                m_currentSlideshowList->setCurrentItem(m_currentSlideshowList->item(i));
                break;
            }
        }
    }

    m_viewControlSlider->blockSignals(true); // ui->viewControlSlider -> m_viewControlSlider
    m_viewControlSlider->setValue(slideshowCurrentIndex);
    m_viewControlSlider->blockSignals(false);

    if (m_slideshowMode == ModePictureScroll) {
        if (m_slides.isEmpty() || slideshowCurrentIndex >= m_slides.size()) return;
        m_filenameEdit->setText(QFileInfo(nextImagePath).fileName()); // ui->filenameLineEdit -> m_filenameEdit
        loadSlidesAround(slideshowCurrentIndex);

        QRectF targetSlideRect = m_slides.at(slideshowCurrentIndex).geometry;
        QScrollBar* scrollBar = (m_slideDirection == DirectionHorizontal)
                                    ? m_view->horizontalScrollBar() // ui->mediaView -> m_view
                                    : m_view->verticalScrollBar();
        qreal targetScrollValue;
        qreal scale = m_view->transform().m11(); // ui->mediaView -> m_view
        if (qFuzzyCompare(scale, 0.0)) scale = 1.0;

        // ビューポートのサイズ（ピクセル）
        qreal viewportSize = (m_slideDirection == DirectionHorizontal)
                                 ? m_view->viewport()->width()
                                 : m_view->viewport()->height();

        // 目標とするスライドの中心（Scene座標）
        qreal targetSceneCenter = (m_slideDirection == DirectionHorizontal)
                                      ? targetSlideRect.center().x()
                                      : targetSlideRect.center().y();

        // Scene座標をView座標（拡大された座標系）に変換
        qreal targetViewCenter = targetSceneCenter * scale;

        // スクロール位置 = 中心位置 - 画面半分
        targetScrollValue = targetViewCenter - (viewportSize / 2.0);

        if (isFirstSlide || m_slideshowEffect != EffectSlide) {
            scrollBar->setValue(targetScrollValue);
        } else {
            QPropertyAnimation* anim = new QPropertyAnimation(scrollBar, "value", this);
            anim->setEndValue(targetScrollValue);
            anim->setDuration(200);
            anim->setEasingCurve(QEasingCurve::OutQuad);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    } else {
        // --- 標準モード ---
        m_filenameEdit->setText(QFileInfo(nextImagePath).fileName()); // ui->filenameLineEdit -> m_filenameEdit
        QPixmap newPixmap(nextImagePath);
        if (newPixmap.isNull()) return;

        if (isFirstSlide || m_slideshowEffect == EffectNone || m_slideshowEffect == EffectSlide) {
            currentImageItem->setPixmap(newPixmap);
            QTimer::singleShot(0, this, &ImageViewController::updateViewFit);
        } else if (m_slideshowEffect == EffectFade) {
            PixmapObject* oldItem = currentImageItem;
            currentImageItem = new PixmapObject(newPixmap);
            mediaScene->addItem(currentImageItem);
            currentImageItem->setOpacity(0.0);
            QTimer::singleShot(0, this, &ImageViewController::updateViewFit);

            QPropertyAnimation* fadeIn = new QPropertyAnimation(currentImageItem, "opacity", this);
            fadeIn->setEndValue(1.0); fadeIn->setDuration(500);
            QPropertyAnimation* fadeOut = new QPropertyAnimation(oldItem, "opacity", this);
            fadeOut->setEndValue(0.0); fadeOut->setDuration(500);

            connect(fadeOut, &QPropertyAnimation::finished, oldItem, &QObject::deleteLater);
            fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    updateZoomState();
}

void ImageViewController::setImageExtensions(const QStringList& extensions)
{
    m_imageExtensions = extensions;
}

void ImageViewController::stepByImage(int step)
{
    const int count = getActiveImageList().size();
    if (count <= 0) return;

    // 1. 基準となるインデックスを決定
    // アニメーション中 (m_isProgrammaticScroll == true) なら
    // 画面上の位置ではなく「予定されている目的地 (m_logicalTargetIndex)」を基準にする。
    int baseIndex;
    if (m_isProgrammaticScroll && m_logicalTargetIndex != -1) {
        baseIndex = m_logicalTargetIndex;
    } else {
        baseIndex = m_viewControlSlider->value();
    }

    // 2. 次のインデックスを計算
    int nextIndex = baseIndex + step;

    // 3. ループ処理
    if (nextIndex >= count) nextIndex = 0;
    else if (nextIndex < 0) nextIndex = count - 1;

    qDebug() << "[IVC] stepByImage. Step:" << step
             << "Base:" << baseIndex << "->" << nextIndex
             << "(Logical:" << m_logicalTargetIndex << ")";

    // 4. 移動実行
    // ★重要: スライダーのシグナルに頼らず、直接 scrollToImage を呼ぶ
    // これにより、連打時も確実に処理が走る
    if (nextIndex != baseIndex || m_isProgrammaticScroll) {

        // スライダーも同期させるが、ループ防止のためにシグナルを止める
        // (scrollToImage内から再度呼ばれるのを防ぐ、または二重呼び出し防止)
        m_viewControlSlider->blockSignals(true);
        m_viewControlSlider->setValue(nextIndex);
        m_viewControlSlider->blockSignals(false);

        scrollToImage(nextIndex);

        // ▼▼▼ 追加: シグナルをブロックしたため、ここで手動でUI更新と通知を行う ▼▼▼
        const QStringList allFiles = getActiveImageList();
        if (nextIndex >= 0 && nextIndex < allFiles.size()) {
            QString path = allFiles.at(nextIndex);

            // 1. ファイル名表示 (LineEdit) を更新
            if (m_filenameEdit) {
                m_filenameEdit->setText(QFileInfo(path).fileName());
            }

            // 2. 本棚や履歴のためにシグナルを発信
            emit currentImageChanged(path);

            // 3. スライドショーリストモードならハイライトも同期
            if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
                QListWidgetItem* itemToHighlight = nullptr;
                for (int i = 0; i < m_currentSlideshowList->count(); ++i) {
                    if (m_currentSlideshowList->item(i)->data(Qt::UserRole).toString() == path) {
                        itemToHighlight = m_currentSlideshowList->item(i);
                        break;
                    }
                }
                // ハイライトの更新
                if (itemToHighlight) {
                    emit setItemHighlighted(m_currentlyDisplayedSlideItem, false);
                    m_currentlyDisplayedSlideItem = itemToHighlight;
                    emit setItemHighlighted(m_currentlyDisplayedSlideItem, true);
                }
            }
        }
        // ▲▲▲ 追加ここまで ▲▲▲
    }
}

void ImageViewController::sortFileInfos(QFileInfoList &list)
{
    if (m_currentSortMode == SortShuffle) {
        auto *generator = QRandomGenerator::global();
        for (int i = 0; i < list.size(); ++i) {
            int j = generator->bounded(list.size());
            list.swapItemsAt(i, j);
        }
        return;
    }

    std::sort(list.begin(), list.end(), [this](const QFileInfo &a, const QFileInfo &b) -> bool {
        bool result = true;
        switch (m_currentSortMode) {
        case SortName:
            // QCollator で "1.jpg, 2.jpg, 10.jpg" の順序を実現
            result = (m_collator.compare(a.fileName(), b.fileName()) < 0);
            break;
        case SortDate:
            result = (a.lastModified() < b.lastModified());
            break;
        case SortSize:
            result = (a.size() < b.size());
            break;
        default:
            break;
        }
        return m_sortAscending ? result : !result;
    });
}

void ImageViewController::stopCurrentMovie()
{
    if (m_currentMovie) {
        m_currentMovie->stop();
        delete m_currentMovie;
        m_currentMovie = nullptr;
    }
}

void ImageViewController::switchToSlideshowListMode(QListWidgetItem *item)
{
    m_viewMode = ModeSlideshowList;
    if (item) {
        displayMedia(item->data(Qt::UserRole).toString());
    }
    // ナビゲーションボタンの状態更新（スライドショーリストモードでは戻る/進むは無効）
    emit navigationStateChanged(false, false, false);
}

void ImageViewController::updateOverlayLayout()
{
    if (!m_view || !m_view->viewport()) return;

    // 中央寄せの計算
    // m_view->viewport()->geometry() を使うことで、枠線などを考慮した正しい表示領域が取れます
    QRect vp = m_view->viewport()->geometry();

    auto centerWidget = [&](QWidget* w) {
        if (w && w->isVisible()) {
            int x = vp.x() + (vp.width() - w->width()) / 2;
            int y = vp.y() + (vp.height() - w->height()) / 2;
            w->move(x, y);
        }
    };

    centerWidget(m_emptyDirectoryLabel);
    centerWidget(m_loadingLabel);
}

void ImageViewController::updateSlideshowProgress()
{
    double duration = m_slideshowInterval * 1000.0;
    if (duration <= 0) return;
    qint64 elapsed = slideshowElapsedTimer.elapsed();
    int progress = static_cast<int>((elapsed / duration) * 1000.0);
    if (progress > 1000) progress = 1000;
    m_progressBar->setValue(progress); // ui->slideshowProgressBar -> m_progressBar
}

void ImageViewController::updateSlideshowIndexFromScroll()
{
    if (m_slideshowMode != ModePictureScroll || m_slides.isEmpty()) {
        return;
    }

    if (m_isProgrammaticScroll) {
        return;
    }

    QScrollBar* scrollBar = (m_slideDirection == DirectionHorizontal)
                                ? m_view->horizontalScrollBar()
                                : m_view->verticalScrollBar();

    QPropertyAnimation* currentAnim = scrollBar->findChild<QPropertyAnimation*>();
    if (currentAnim && currentAnim->state() == QAbstractAnimation::Running) {
        return;
    }

    qreal scale = m_view->transform().m11();
    if (qFuzzyCompare(scale, 0.0)) scale = 1.0;

    // 現在のスクロール位置（ビュー座標）
    qreal currentScroll = scrollBar->value();

    // ビューポートサイズ（ピクセル）
    qreal viewportSize = (m_slideDirection == DirectionHorizontal)
                             ? m_view->viewport()->width()
                             : m_view->viewport()->height();

    // 現在の画面中心（ビュー座標）
    // CenterView = ScrollValue + (ViewportWidth / 2)
    qreal currentCenterInView = currentScroll + (viewportSize / 2.0);

    // ★修正: ビュー座標をシーン座標に戻す
    // CenterScene = CenterView / Scale
    qreal currentCenterInScene = currentCenterInView / scale;

    // 最適なインデックスの探索
    qreal minDistanceSquared = -1.0;
    int bestIndex = 0;

    for (int i = 0; i < m_slides.size(); ++i) {
        QPointF slideCenter = m_slides.at(i).geometry.center();
        qreal itemCenter = (m_slideDirection == DirectionHorizontal) ? slideCenter.x() : slideCenter.y();

        qreal dist = itemCenter - currentCenterInScene;
        qreal distSq = dist * dist;

        if (minDistanceSquared < 0 || distSq < minDistanceSquared) {
            minDistanceSquared = distSq;
            bestIndex = i;
        }
    }

    loadSlidesAround(bestIndex);

    if (bestIndex != m_viewControlSlider->value()) {
        qDebug() << "[IVC Debug] Auto-adjusting index to:" << bestIndex
                 << "(SceneCenter:" << currentCenterInScene << " SliderVal:" << m_viewControlSlider->value() << ")";

        m_filenameEdit->setText(QFileInfo(m_slides.at(bestIndex).filePath).fileName());

        if (m_viewMode == ModeSlideshowList && m_currentSlideshowList) {
            QString path = m_slides.at(bestIndex).filePath;
            QListWidgetItem* itemToHighlight = nullptr;
            for (int i = 0; i < m_currentSlideshowList->count(); ++i) {
                if (m_currentSlideshowList->item(i)->data(Qt::UserRole).toString() == path) {
                    itemToHighlight = m_currentSlideshowList->item(i);
                    break;
                }
            }
            if (itemToHighlight) {
                emit setItemHighlighted(m_currentlyDisplayedSlideItem, false);
                m_currentlyDisplayedSlideItem = itemToHighlight;
                emit setItemHighlighted(m_currentlyDisplayedSlideItem, true);
            }
        }

        m_viewControlSlider->blockSignals(true);
        m_viewControlSlider->setValue(bestIndex);
        m_viewControlSlider->blockSignals(false);
    }
}

void ImageViewController::updateViewControlSliderState(int currentIndex, int count)
{
    m_viewControlSlider->show(); // ui->viewControlSlider -> m_viewControlSlider

    if (count == -1) {
        count = (m_slideshowMode == ModePictureScroll) ? m_slides.count() : getActiveImageList().count();
    }

    if (count > 1) {
        m_viewControlSlider->setEnabled(true);
        m_viewControlSlider->setRange(0, count - 1);


        int indexToSet = currentIndex;

        if (indexToSet < 0) {
            const QStringList& allFiles = getActiveImageList();
            if (m_viewMode == ModeSlideshowList && m_currentSlideshowList && m_currentSlideshowList->currentItem()) {
                indexToSet = allFiles.indexOf(m_currentSlideshowList->currentItem()->data(Qt::UserRole).toString());
            } else {
                indexToSet = allFiles.indexOf(m_filenameEdit->text()); // ui->filenameLineEdit -> m_filenameEdit
            }
        }
        if (indexToSet < 0) indexToSet = 0;

        m_viewControlSlider->blockSignals(true);
        m_viewControlSlider->setValue(indexToSet);
        m_viewControlSlider->blockSignals(false);

        if (m_slideshowMode == ModePictureScroll) {
            const QStringList& allFiles = getActiveImageList();
            if (indexToSet >= 0 && indexToSet < allFiles.size()) {
                emit currentImageChanged(allFiles.at(indexToSet));
            }
        }
    } else {
        m_viewControlSlider->setEnabled(false);
        m_viewControlSlider->setRange(0, 0);
    }
}

void ImageViewController::updateViewFit()
{
    if (m_slideshowMode == ModePictureScroll) return;
    m_view->resetTransform(); // ui->mediaView -> m_view

    if (!currentImageItem || currentImageItem->pixmap().isNull()) {
        mediaScene->setSceneRect(m_view->viewport()->rect()); // ui->mediaView -> m_view
        return;
    }

    QRectF viewRect = m_view->viewport()->rect();
    QRectF pixmapRect = currentImageItem->pixmap().rect();
    if (viewRect.isEmpty() || pixmapRect.isEmpty()) return;

    qreal finalScale = m_fitScale * m_userZoomFactor;

    QPointF centerPos = viewRect.center() - (pixmapRect.center() * finalScale);
    currentImageItem->setScale(finalScale);
    currentImageItem->setPos(centerPos);
    mediaScene->setSceneRect(currentImageItem->sceneBoundingRect());

    m_zoomSpinBox->blockSignals(true); // ui->zoomSpinBox -> m_zoomSpinBox
    m_zoomSpinBox->setValue(qRound(m_userZoomFactor * 100.0));
    m_zoomSpinBox->blockSignals(false);
}

void ImageViewController::updateZoomState()
{
    bool isZoomable = false;
    // m_stackedWidget で現在のページを確認 (ui->mediaStackedWidget)
    if (m_stackedWidget->currentWidget() == m_videoPage) { // ui->videoPage -> m_videoPage
        isZoomable = false;
    } else if (m_slideshowMode == ModePictureScroll) {
        isZoomable = !m_slides.isEmpty();
    } else {
        isZoomable = !currentImageItem->pixmap().isNull();
    }
    m_zoomSpinBox->setEnabled(isZoomable); // ui->zoomSpinBox -> m_zoomSpinBox
}

// --- Private Helpers ---

void ImageViewController::addPathToHistory(const QString& path)
{
    // 現在位置より後ろの履歴を削除（新しい分岐）
    if (m_historyIndex < m_history.size() - 1) {
        m_history = m_history.mid(0, m_historyIndex + 1);
    }

    // 同じパスの連続追加を防ぐ
    if (m_history.isEmpty() || m_history.last() != path) {
        m_history.append(path);
        m_historyIndex = m_history.size() - 1;
    }
}

void ImageViewController::browseTo(const QString& path, const QString& fileToSelectPath, bool addToHistory)
{
    qDebug() << "[IVC] browseTo called.";
    qDebug() << "      Path:" << path;
    qDebug() << "      TargetFile:" << (fileToSelectPath.isEmpty() ? "<EMPTY>" : fileToSelectPath);

    m_viewMode = ModeDirectoryBrowse;
    m_currentBrowsePath = path;

    if (addToHistory) {
        addPathToHistory(path);
    }

    QDir dir(path);
    QStringList filters;
    for (const QString& ext : m_imageExtensions) {
        filters << "*." + ext;
    }

    m_directoryFiles.clear();
    // 以前: QFileInfoList fileInfos = dir.entryInfoList(filters, QDir::Files, QDir::Name | QDir::LocaleAware);

    // ソート指定なしでファイルリストを取得
    QFileInfoList fileInfos = dir.entryInfoList(filters, QDir::Files);

    // 自前のソート関数を通す (ここで自然順ソートなどが適用される)
    sortFileInfos(fileInfos);

    // ソート済みの結果をリストに格納
    // ★ リスト作成時にパスを標準化 (fromNativeSeparators) しておく (既存の修正を維持)
    for (const QFileInfo& fi : fileInfos) {
        m_directoryFiles << QDir::fromNativeSeparators(fi.absoluteFilePath());
    }

    emit currentDirectoryChanged(m_currentBrowsePath);

    bool canUp = !getParentPath(m_currentBrowsePath).isEmpty();
    bool canBack = (m_historyIndex > 0);
    bool canForward = (m_historyIndex < m_history.count() - 1);
    emit navigationStateChanged(canBack, canForward, canUp);

    // ★ 検索対象のパスも標準化する
    QString normTarget = QDir::fromNativeSeparators(fileToSelectPath);

    if (m_slideshowMode == ModePictureScroll) {
        // --- パノラマモード ---
        setupPictureScroll(m_directoryFiles);

        int index = 0;
        if (!normTarget.isEmpty()) {
            // 1. 完全一致検索 (リスト側も標準化済みなのでヒット率向上)
            index = m_directoryFiles.indexOf(normTarget);

            // 2. それでもダメなら大文字小文字無視で比較
            if (index == -1) {
                for (int i = 0; i < m_directoryFiles.size(); ++i) {
                    if (QString::compare(m_directoryFiles[i], normTarget, Qt::CaseInsensitive) == 0) {
                        index = i;
                        break;
                    }
                }
            }

            // デバッグ出力
            if (index != -1) {
                qDebug() << "[IVC] Found restore file at index:" << index;
            } else {
                qDebug() << "[IVC] Restore file NOT found:" << normTarget;
            }
        }

        if (index < 0) index = 0;

        positionScrollAtIndex(index);
        updateViewControlSliderState(index, m_directoryFiles.count());

        if (index >= 0 && index < m_directoryFiles.count()) {
            m_filenameEdit->setText(QFileInfo(m_directoryFiles.at(index)).fileName());
        } else {
            m_filenameEdit->clear();
        }

        if (m_directoryFiles.isEmpty()) {
            m_emptyDirectoryLabel->show();
            updateOverlayLayout(); // ★ 追加: 表示直後に位置を中央に合わせる
        } else {
            m_emptyDirectoryLabel->hide();
        }

    } else {
        // --- 標準モード ---
        // こちらも標準化パスを使って検索・表示する
        if (!normTarget.isEmpty()) {
            // リストに含まれているか確認してから表示 (エラー回避のため)
            int index = m_directoryFiles.indexOf(normTarget);
            if (index == -1) {
                // 大文字小文字無視で再検索
                for (int i = 0; i < m_directoryFiles.size(); ++i) {
                    if (QString::compare(m_directoryFiles[i], normTarget, Qt::CaseInsensitive) == 0) {
                        normTarget = m_directoryFiles[i]; // 見つかった正しいパスで上書き
                        index = i;
                        break;
                    }
                }
            }

            if (index != -1) {
                displayMedia(normTarget);
            } else if (!m_directoryFiles.isEmpty()) {
                displayMedia(m_directoryFiles.first());
            } else {
                displayMedia(QString());
            }
        } else if (!m_directoryFiles.isEmpty()) {
            displayMedia(m_directoryFiles.first());
        } else {
            displayMedia(QString());
            // ここは displayMedia 側で処理されるため追加不要ですが、念のため
        }
    }
}

QString ImageViewController::getParentPath(const QString& path) const
{
    QDir dir(path);
    if (dir.cdUp()) {
        return dir.absolutePath();
    }
    return QString();
}

bool ImageViewController::eventFilter(QObject *obj, QEvent *event)
{
    if (!obj) return false;
    if (obj == m_view && event->type() == QEvent::Resize) {
        updateOverlayLayout();
        // ここでは return true せず、QGraphicsView 本来のリサイズ処理も行わせる
    }

    // --- ファイル名入力欄のダブルクリック ---
    if (obj == m_filenameEdit && event->type() == QEvent::MouseButtonDblClick) {
        m_filenameEdit->setReadOnly(false);
        m_filenameEdit->selectAll();
        m_filenameEdit->setFocus();
        return true;
    }

    // --- MediaView (Viewport) のイベント ---
    if (obj == m_view->viewport()) {

        // 1. マウスボタンが押されたとき (記録のみ)
        // ★修正: MouseButtonDblClick も「押された」とみなす
        if (m_slideshowMode == ModePictureScroll &&
            (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {

            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::RightButton) {
                m_clickStartPos = mouseEvent->pos();
                m_isClickCandidate = true; // クリック判定の候補とする

                // return true せずにイベントを通過させる
            }
        }

        // 2. マウスが動いたとき (ドラッグ判定)
        if (m_slideshowMode == ModePictureScroll && event->type() == QEvent::MouseMove) {
            if (m_isClickCandidate) {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if ((mouseEvent->pos() - m_clickStartPos).manhattanLength() > QApplication::startDragDistance()) {
                    m_isClickCandidate = false; // クリック移動をキャンセル
                }
            }
        }

        // 3. マウスボタンが離されたとき (移動実行判定)
        if (m_slideshowMode == ModePictureScroll && event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);

            if (m_isClickCandidate) {
                int step = 0;
                bool handled = false;

                if (mouseEvent->button() == Qt::LeftButton) {
                    step = -1;
                    handled = true;
                } else if (mouseEvent->button() == Qt::RightButton) {
                    step = 1;
                    handled = true;
                }

                if (handled) {
                    if (m_layoutDirection == LayoutDirection::Backward) step *= -1;

                    // 共通関数を使用
                    stepByImage(step);
                }
                m_isClickCandidate = false;
            }
        }

        // --- ドロップ処理 ---
        if (event->type() == QEvent::Drop) {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            const QMimeData* mimeData = dropEvent->mimeData();
            if (mimeData->hasUrls()) {
                qDebug() << "[IVC] Drop event detected on viewport!";
                emit filesDropped(mimeData->urls());
                dropEvent->acceptProposedAction();
                return true;
            }
            return false;
        }

        // --- ドラッグエンター ---
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* dragEvent = static_cast<QDragMoveEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                dragEvent->acceptProposedAction();
                return true;
            }
            return false;
        }

        // --- ホイール ---
        if (event->type() == QEvent::Wheel) {
            handleWheelEvent(static_cast<QWheelEvent*>(event), m_view->viewport());
            return true;
        }
    }

    // --- スライダー/SpinBoxのホイール操作 (変更なし) ---
    if (obj == m_viewControlSlider && event->type() == QEvent::Wheel) {
        handleWheelEvent(static_cast<QWheelEvent*>(event), m_viewControlSlider);
        return true;
    }
    if (obj == m_intervalSpinBox && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        double originalStep = m_intervalSpinBox->singleStep();
        m_intervalSpinBox->setSingleStep(0.1);
        if (wheelEvent->angleDelta().y() > 0) {
            m_intervalSpinBox->stepUp();
        } else {
            m_intervalSpinBox->stepDown();
        }
        m_intervalSpinBox->setSingleStep(originalStep);
        return true;
    }

    return QObject::eventFilter(obj, event);
}
