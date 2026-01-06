#include "controlbar.h"
#include "ui_controlbar.h" // Qtが生成するUIヘッダ
#include <QMenu>
#include <qstyle.h>
#include <QHBoxLayout> // 追加
#include <QTime>

ControlBar::ControlBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ControlBar),
    m_isPlaying(false),
    m_currentLoopMode(NoLoop),
    m_currentShuffleMode(ShuffleOff),
    m_canGoNext(false),
    m_canGoPrev(false),
    m_isCompact(false),
    m_currentPosition(0),
    m_totalDuration(0),
    m_showRemainingTime(false)
{
    ui->setupUi(this);

    // 1. 既存のレイアウトからスライダーを一時的に外す
    // (ui->verticalLayout の 0番目に slider がある前提)
    ui->verticalLayout->removeWidget(ui->progressSlider);

    // 2. 新しい水平レイアウトを作成
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->setContentsMargins(0, 0, 0, 0); // 余白調整
    sliderLayout->setSpacing(0);

    // 3. 左側のラベル (現在時間) を作成
    m_currentTimeLabel = new QLabel("00:00", this);
    m_currentTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_currentTimeLabel->setMinimumWidth(40); // 桁が変わってもガタつかないように

    // 4. 右側のボタン (合計/残り時間) を作成
    //    QPushButtonをフラットにすることで、クリック可能なラベルのように見せる
    m_totalTimeButton = new QPushButton("00:00", this);
    m_totalTimeButton->setFlat(true); // 枠線を消す
    m_totalTimeButton->setCursor(Qt::PointingHandCursor); // カーソルを指にする
    m_totalTimeButton->setStyleSheet("QPushButton { border: none; text-align: right; padding: 0px; } QPushButton:hover { color: #ff69b4; }");
    m_totalTimeButton->setToolTip("クリックで 合計時間 / 残り時間 を切り替え");
    m_totalTimeButton->setMinimumWidth(40);

    // クリックイベントを接続
    connect(m_totalTimeButton, &QPushButton::clicked, this, &ControlBar::toggleTimeDisplayMode);

    // 5. 水平レイアウトに配置: [Label] - [Slider] - [Button]
    sliderLayout->addWidget(m_currentTimeLabel);
    sliderLayout->addWidget(ui->progressSlider); // スライダーを戻す
    sliderLayout->addWidget(m_totalTimeButton);

    // 6. 全体の垂直レイアウトの先頭(元スライダーがあった場所)に、この水平レイアウトを挿入
    ui->verticalLayout->insertLayout(0, sliderLayout);

    m_playlistButtons = new QButtonGroup(this);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &ControlBar::customContextMenuRequested, this, &ControlBar::showContextMenu);
    m_visibilityMenu = new QMenu("プレイリスト切り替えボタンの表示・非表示", this);
    // グループ内のいずれかのボタンがクリックされたら、そのID付きでシグナルを発信する
    connect(m_playlistButtons, &QButtonGroup::idClicked, this, &ControlBar::playlistButtonClicked);

    // --- UI要素のシグナルを、このクラスのシグナルに中継 ---
    connect(ui->playPauseButton, &QPushButton::clicked, this, &ControlBar::playPauseClicked);
    connect(ui->stopButton, &QPushButton::clicked, this, &ControlBar::stopClicked);
    connect(ui->nextButton, &QPushButton::clicked, this, &ControlBar::nextClicked);
    connect(ui->prevButton, &QPushButton::clicked, this, &ControlBar::prevClicked);
    connect(ui->repeatButton, &QPushButton::clicked, this, &ControlBar::repeatClicked);
    connect(ui->shuffleButton, &QPushButton::clicked, this, &ControlBar::shuffleClicked);
    connect(ui->volumeMuteButton, &QPushButton::clicked, this, &ControlBar::muteClicked);

    // スライダー操作のシグナル
    connect(ui->progressSlider, &QSlider::sliderMoved, this, &ControlBar::positionSliderMoved);
    connect(ui->progressSlider, &QSlider::sliderPressed, this, &ControlBar::positionSliderPressed);
    connect(ui->progressSlider, &QSlider::sliderReleased, this, &ControlBar::positionSliderReleased);

    // --- ボリューム操作（スライダーとスピンボックスの同期）---
    // スライダーが動いたら、シグナルを発信し、スピンボックスも更新
    connect(ui->volumeSlider, &QSlider::valueChanged, this, [this](int value){
        if (isUpdatingVolume) return;
        isUpdatingVolume = true;

        // mainwindow.cppのロジックを再利用
        float volumePercent = 0.0f;
        if (value <= 160) {
            volumePercent = (value / 160.0f) * 100.0f;
        } else {
            volumePercent = 100.0f + ((value - 160) / 40.0f) * 100.0f;
        }
        int percent = qRound(volumePercent);
        ui->volumeSpinBox->setValue(percent);

        // 操作時にアイコンを即時更新する
        // setVolume 内の更新処理はガードされてしまうため、ここで直接キャッシュ済みアイコンを適用します
        if (percent == 0) {
            ui->volumeMuteButton->setIcon(m_volumeIcons[1]); // Zero Icon
        } else if (percent <= 100) {
            ui->volumeMuteButton->setIcon(m_volumeIcons[2]); // Down Icon
        } else {
            ui->volumeMuteButton->setIcon(m_volumeIcons[3]); // Up Icon
        }
        // ----------------------------------------------

        emit volumeChanged(percent);

        isUpdatingVolume = false;
    });

    // スピンボックスが動いたら、シグナルを発信し、スライダーも更新
    connect(ui->volumeSpinBox, &QSpinBox::valueChanged, this, [this](int percent){
        if (isUpdatingVolume) return;
        emit volumeChanged(percent);
        if (isUpdatingVolume) return;
        isUpdatingVolume = true;
        ui->volumeSpinBox->setValue(percent);
        float sliderValue = 0.0f;
        if (percent <= 100) {
            sliderValue = (percent / 100.0f) * 160.0f;
        } else {
            sliderValue = 160.0f + ((percent - 100) / 100.0f) * 40.0f;
        }
        ui->volumeSlider->setValue(qRound(sliderValue));
        isUpdatingVolume = false;
    });

    // --- 初期状態の設定 ---
    updateButtonStates(false, false, QIcon(), QIcon(), QIcon());
}

ControlBar::~ControlBar()
{
    delete ui;
}

// === スロットの実装 ===

int ControlBar::position() const
{
    return ui->progressSlider->value();
}

int ControlBar::volume() const
{
    return ui->volumeSpinBox->value();
}

void ControlBar::addPlaylistButton(int id, const QString &text)
{
    QPushButton* newButton = new QPushButton(text, this);
    newButton->setCheckable(false);

    newButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    // ボタンの最小幅を指定します（この値はお好みで調整してください）。
    newButton->setMinimumWidth(80);
    newButton->setMaximumHeight(56);

    if (m_isCompact) {
        // コンパクトモードならボタンの高さを低くする
        newButton->setMinimumWidth(0);
        newButton->setMaximumWidth(56);
    }

    m_playlistButtons->addButton(newButton, id);
    ui->playlistButtonLayout->addWidget(newButton);

    QAction* action = new QAction(text, this);
    action->setCheckable(true);
    action->setChecked(true); // 初期状態は表示(チェックあり)
    connect(action, &QAction::toggled, newButton, &QPushButton::setVisible);
    m_visibilityMenu->addAction(action);
    m_buttonActions.append(action); // 管理リストに追加
}

void ControlBar::clearPlaylistButtons()
{
    // 処理中にシグナルが飛ばないように一時的に接続を解除
    disconnect(m_playlistButtons, &QButtonGroup::idClicked, this, &ControlBar::playlistButtonClicked);

    // グループに所属する全てのボタンを取得
    QList<QAbstractButton*> buttons = m_playlistButtons->buttons();
    for(QAbstractButton* button : buttons) {
        m_playlistButtons->removeButton(button); // グループから削除
        ui->playlistButtonLayout->removeWidget(button); // レイアウトから削除
        delete button; // メモリを解放
    }

    m_visibilityMenu->clear();
    qDeleteAll(m_buttonActions); // QList<QAction*> の中身を全てdelete
    m_buttonActions.clear();

    // シグナルを再接続
    connect(m_playlistButtons, &QButtonGroup::idClicked, this, &ControlBar::playlistButtonClicked);
}

void ControlBar::clearPlaylistSelection()
{
    if (!m_playlistButtons) return;

    // 排他制御(Exclusive)が有効なままだと「全て未選択」にできない場合があるため、
    // 一時的に無効化してからチェックを外す
    bool wasExclusive = m_playlistButtons->exclusive();
    m_playlistButtons->setExclusive(false);

    if (QAbstractButton* btn = m_playlistButtons->checkedButton()) {
        btn->setChecked(false);
    }

    m_playlistButtons->setExclusive(wasExclusive);
}

QString ControlBar::formatTime(qint64 ms) const
{
    if (ms < 0) ms = 0;
    int seconds = (ms / 1000) % 60;
    int minutes = (ms / 60000) % 60;
    int hours   = (ms / 3600000);

    if (hours > 0) {
        return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

void ControlBar::showContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);

    // --- 再生コントロールのアクションを作成 ---
    QAction *playPauseAction;
    if (m_isPlaying) {
        playPauseAction = new QAction(m_pauseIcon, "一時停止", this); // ★キャッシュを使用
    } else {
        playPauseAction = new QAction(m_playIcon, "再生", this); // ★キャッシュを使用
    }
    connect(playPauseAction, &QAction::triggered, this, &ControlBar::playPauseClicked);

    // ★Stop, Next, Prev もキャッシュしたアイコンを使うように修正
    QAction *stopAction = new QAction(m_stopIcon, "停止", this);
    connect(stopAction, &QAction::triggered, this, &ControlBar::stopClicked);

    QAction *nextAction = new QAction(m_nextIcon, "次へ", this);
    nextAction->setEnabled(m_canGoNext);
    connect(nextAction, &QAction::triggered, this, &ControlBar::nextClicked);

    QAction *prevAction = new QAction(m_prevIcon, "前へ", this);
    prevAction->setEnabled(m_canGoPrev);
    connect(prevAction, &QAction::triggered, this, &ControlBar::prevClicked);

    contextMenu.addAction(playPauseAction);
    contextMenu.addAction(stopAction);
    contextMenu.addAction(nextAction);
    contextMenu.addAction(prevAction);
    contextMenu.addSeparator();

    QAction *repeatAction = new QAction(m_repeatIcons[m_currentLoopMode], "リピート切替", this);
    QAction *shuffleAction = new QAction(m_shuffleIcons[m_currentShuffleMode], "シャッフル切替", this);

    connect(repeatAction, &QAction::triggered, this, &ControlBar::repeatClicked);
    connect(shuffleAction, &QAction::triggered, this, &ControlBar::shuffleClicked);

    contextMenu.addAction(repeatAction);
    contextMenu.addAction(shuffleAction);
    contextMenu.addSeparator();

    // --- 既存の表示/非表示サブメニューを追加 ---
    contextMenu.addMenu(m_visibilityMenu);

    // メニューを表示
    contextMenu.exec(mapToGlobal(pos));
}

void ControlBar::setActiveButton(int id)
{
    QAbstractButton* button = m_playlistButtons->button(id);
    if (button) {
        button->setChecked(true);
    }
}

void ControlBar::setButtonChecked(int id, bool checked)
{
    QAbstractButton* btn = m_playlistButtons->button(id);
    if (!btn) return;

    if (checked) {
        btn->setChecked(true);
    } else {
        // チェックを外す場合、Exclusive(排他)モードのままだと外れない場合があるため
        // 一時的に排他モードを解除して操作する
        bool wasExclusive = m_playlistButtons->exclusive();
        m_playlistButtons->setExclusive(false);

        btn->setChecked(false);

        m_playlistButtons->setExclusive(wasExclusive);
    }
}

void ControlBar::setCompactMode(bool compact)
{
    m_isCompact = compact;

    if (m_isCompact) {
        // --- コンパクトモード用の設定 ---
        // スペーサーの幅を小さくする
        ui->horizontalSpacer_4->changeSize(4, 2, QSizePolicy::Fixed, QSizePolicy::Minimum);
        ui->horizontalSpacer_5->changeSize(4, 2, QSizePolicy::Fixed, QSizePolicy::Minimum);

        // 既存のプレイリストボタンを小さくする
        for (QAbstractButton* button : m_playlistButtons->buttons()) {
            button->setMaximumHeight(28); // 高さを低く設定
            // 必要であればフォントサイズなども変更できます
            // QFont font = button->font();
            // font.setPointSize(8);
            // button->setFont(font);
        }
    } else {
        // --- フルモード用の設定 (デフォルト値) ---
        ui->horizontalSpacer_4->changeSize(24, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
        ui->horizontalSpacer_5->changeSize(24, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        for (QAbstractButton* button : m_playlistButtons->buttons()) {
            button->setMaximumHeight(56);
        }
    }
}

void ControlBar::setTrackInfo(const QString &info)
{
    ui->trackInfoLabel->setText(info);
}

void ControlBar::setTimeLabel(const QString &text)
{
    ui->timeLabel->setText(text);
}

void ControlBar::setPlaylistButtonState(int id, bool isEmpty, bool isPlaying, const QString &tooltip)
{
    QAbstractButton* btn = m_playlistButtons->button(id);
    if (!btn) return;

    // プロパティをセット
    btn->setProperty("isEmpty", isEmpty);
    btn->setProperty("isPlaying", isPlaying);
    btn->setToolTip(tooltip);

    // スタイルを強制的に再計算させる (これがないと見た目が即座に変わらない場合がある)
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
}

void ControlBar::setPlaylistButtonVisible(int id, bool visible)
{
    QAbstractButton* button = m_playlistButtons->button(id);
    if (button) {
        button->setVisible(visible);
    }
}

void ControlBar::setProgressDuration(int duration)
{
    m_totalDuration = duration; // ★値を保持
    ui->progressSlider->setRange(0, duration);
    updateTimeLabels();         // ★表示更新
}

void ControlBar::setProgressPosition(int position)
{
    // ユーザーがスライダーを操作中でない場合のみ値を更新
    if (!ui->progressSlider->isSliderDown()) {
        ui->progressSlider->setValue(position);
    }

    m_currentPosition = position; // ★値を保持 (スライダー操作中に関わらず時間は更新してOK)
    updateTimeLabels();           // ★表示更新
}

void ControlBar::setSeekEnabled(bool enabled)
{
    ui->progressSlider->setEnabled(enabled);
}

void ControlBar::setVolume(int percent, const QIcon& zeroIcon, const QIcon& downIcon, const QIcon& upIcon)
{
    if (isUpdatingVolume) return;
    isUpdatingVolume = true;

    ui->volumeSpinBox->setValue(percent);

    // パーセントからスライダー内部値へ変換
    float sliderValue = 0.0f;
    if (percent <= 100) {
        sliderValue = (percent / 100.0f) * 160.0f;
    } else {
        sliderValue = 160.0f + ((percent - 100) / 100.0f) * 40.0f;
    }
    ui->volumeSlider->setValue(qRound(sliderValue));

    // アイコン更新
    m_volumeIcons[1] = zeroIcon;
    m_volumeIcons[2] = downIcon;
    m_volumeIcons[3] = upIcon;
    if (percent == 0) ui->volumeMuteButton->setIcon(m_volumeIcons[1]);
    else if (percent <= 100) ui->volumeMuteButton->setIcon(m_volumeIcons[2]);
    else ui->volumeMuteButton->setIcon(m_volumeIcons[3]);

    isUpdatingVolume = false;
}

void ControlBar::setMuted(bool isMuted, int lastVolume, const QIcon& speakerMuteIcon, const QIcon& speakerZeroIcon, const QIcon& speakerDownIcon, const QIcon& speakerUpIcon)
{
    m_volumeIcons[0] = speakerMuteIcon;
    m_volumeIcons[1] = speakerZeroIcon;
    m_volumeIcons[2] = speakerDownIcon;
    m_volumeIcons[3] = speakerUpIcon;

    if (isMuted) {
        ui->volumeMuteButton->setIcon(speakerMuteIcon);
    } else {
        // ミュート解除時は、直前の音量に基づいてアイコンを復元
        setVolume(lastVolume, speakerZeroIcon, speakerDownIcon, speakerUpIcon);
    }
}

void ControlBar::toggleTimeDisplayMode()
{
    m_showRemainingTime = !m_showRemainingTime;

    // ツールチップと表示を更新
    m_totalTimeButton->setToolTip("残り時間/合計時間表示(クリックで切替)");

    updateTimeLabels();
}

void ControlBar::updatePlayPauseIcon(bool isPlaying, const QIcon& playIcon, const QIcon& pauseIcon)
{
    m_isPlaying = isPlaying;
    m_playIcon = playIcon;
    m_pauseIcon = pauseIcon;
    ui->playPauseButton->setIcon(isPlaying ? pauseIcon : playIcon);
    ui->playPauseButton->setToolTip(isPlaying ? "一時停止" : "再生");
}

void ControlBar::updateRepeatIcon(LoopMode mode, const QIcon& repeatIcon, const QIcon& repeatOneIcon, const QIcon& repeatAllIcon)
{
    m_currentLoopMode = mode;

    m_repeatIcons[0] = repeatIcon;
    m_repeatIcons[1] = repeatOneIcon;
    m_repeatIcons[2] = repeatAllIcon;

    switch (mode) {
    case NoLoop:    ui->repeatButton->setIcon(m_repeatIcons[0]); ui->repeatButton->setToolTip("ループなし"); break;
    case RepeatOne: ui->repeatButton->setIcon(m_repeatIcons[1]); ui->repeatButton->setToolTip("1トラックリピート"); break;
    case RepeatAll: ui->repeatButton->setIcon(m_repeatIcons[2]); ui->repeatButton->setToolTip("全体ループ"); break;
    }
}

void ControlBar::updateShuffleIcon(ShuffleMode mode, const QIcon& shuffleOffIcon, const QIcon& shuffleOnIcon, const QIcon& shuffleRandomIcon)
{
    m_currentShuffleMode = mode;

    m_shuffleIcons[0] = shuffleOffIcon;
    m_shuffleIcons[1] = shuffleOnIcon;
    m_shuffleIcons[2] = shuffleRandomIcon;

    switch (mode) {
    case ShuffleOff:      ui->shuffleButton->setIcon(m_shuffleIcons[0]); ui->shuffleButton->setToolTip("シャッフルなし"); break;
    case ShuffleNoRepeat: ui->shuffleButton->setIcon(m_shuffleIcons[1]); ui->shuffleButton->setToolTip("シャッフル再生"); break;
    case ShuffleRepeat:   ui->shuffleButton->setIcon(m_shuffleIcons[2]); ui->shuffleButton->setToolTip("ランダム再生"); break;
    }
}

void ControlBar::updateButtonStates(bool canGoNext, bool canGoPrev, const QIcon& nextIcon, const QIcon& prevIcon, const QIcon& stopIcon)
{
    m_canGoNext = canGoNext;
    m_canGoPrev = canGoPrev;
    m_nextIcon = nextIcon;
    m_prevIcon = prevIcon;
    m_stopIcon = stopIcon; // ★ stopIconもキャッシュする

    ui->nextButton->setEnabled(canGoNext);
    ui->prevButton->setEnabled(canGoPrev);

    ui->nextButton->setIcon(m_nextIcon);
    ui->prevButton->setIcon(m_prevIcon);
    ui->stopButton->setIcon(m_stopIcon);
}

void ControlBar::updatePlaylistButtonText(int id, const QString &text)
{
    QAbstractButton* button = m_playlistButtons->button(id);
    if (button) {
        button->setText(text);
    }
}

void ControlBar::updateTimeLabels()
{
    QString currStr = formatTime(m_currentPosition);
    QString totalStr = formatTime(m_totalDuration);

    // 左側: 現在時間
    m_currentTimeLabel->setText(formatTime(m_currentPosition));

    // 右側: 合計 または 残り
    if (m_showRemainingTime) {
        qint64 remaining = m_totalDuration - m_currentPosition;
        if (remaining < 0) remaining = 0;
        m_totalTimeButton->setText("-" + formatTime(remaining));
    } else {
        m_totalTimeButton->setText(formatTime(m_totalDuration));
    }
    ui->timeLabel->setText(QString("%1 / %2").arg(currStr).arg(totalStr));
}

void ControlBar::wheelEvent(QWheelEvent *event)
{
    // angleDelta().y()で垂直方向の回転量を取得
    int delta = event->angleDelta().y();

    if (delta > 0) {
        // ホイールを上に回した場合: 現在の音量に5を加算
        int nextVol = qMin(200, ui->volumeSpinBox->value() + 5);
        emit volumeChanged(nextVol);
    } else if (delta < 0) {
        // ホイールを下に回した場合: 現在の音量から5を減算
        // ★修正: 0未満にならないように制限
        int nextVol = qMax(0, ui->volumeSpinBox->value() - 5);
        emit volumeChanged(nextVol);
    }

    // イベントを受け取ったことを通知
    event->accept();
}
