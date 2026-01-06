#ifndef CONTROLBAR_H
#define CONTROLBAR_H

#include <QAction>
#include <QButtonGroup>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QWheelEvent>
#include <QWidget>

// クラスの前方宣言
namespace Ui {
class ControlBar;
}

class ControlBar : public QWidget
{
    Q_OBJECT

public:
    explicit ControlBar(QWidget *parent = nullptr);
    ~ControlBar();
    int position() const;
    int volume() const;

    // LoopModeとShuffleModeをMainWindowと共有するためのenum
    // MainWindow.hからコピーしてきても、ここで再定義しても良い
    enum LoopMode { NoLoop, RepeatOne, RepeatAll };
    enum ShuffleMode { ShuffleOff, ShuffleNoRepeat, ShuffleRepeat };

signals:
    // === ユーザーの操作を外部（MainWindow）に通知するためのシグナル ===
    void playPauseClicked();
    void stopClicked();
    void nextClicked();
    void prevClicked();
    void repeatClicked();
    void shuffleClicked();
    void positionSliderMoved(int position);
    void positionSliderPressed();
    void positionSliderReleased();
    void volumeChanged(int volume);
    void muteClicked();
    void playlistButtonClicked(int id);


public slots:
    // === 外部（MainWindow）からこのウィジェットの状態を更新するためのスロット ===
    void addPlaylistButton(int id, const QString &text);
    void clearPlaylistButtons();
    void clearPlaylistSelection();
    void setSeekEnabled(bool enabled);
    void setActiveButton(int id);
    void setButtonChecked(int id, bool checked);
    void setCompactMode(bool compact);
    void setMuted(bool isMuted, int lastVolume, const QIcon& speakerMuteIcon, const QIcon& speakerZeroIcon, const QIcon& speakerDownIcon, const QIcon& speakerUpIcon);
    void setPlaylistButtonVisible(int id, bool visible);
    void setPlaylistButtonState(int id, bool isEmpty, bool isPlaying, const QString &tooltip);
    void setProgressDuration(int duration);
    void setProgressPosition(int position);
    void setTrackInfo(const QString &info);
    void setTimeLabel(const QString &text);
    void setVolume(int percent, const QIcon& zeroIcon, const QIcon& downIcon, const QIcon& upIcon);
    void toggleTimeDisplayMode();
    void updateButtonStates(bool canGoNext, bool canGoPrev, const QIcon& nextIcon, const QIcon& prevIcon, const QIcon& stopIcon);
    void updatePlaylistButtonText(int id, const QString &text);
    void updatePlayPauseIcon(bool isPlaying, const QIcon& playIcon, const QIcon& pauseIcon);
    void updateRepeatIcon(LoopMode mode, const QIcon& repeatIcon, const QIcon& repeatOneIcon, const QIcon& repeatAllIcon);
    void updateShuffleIcon(ShuffleMode mode, const QIcon& shuffleOffIcon, const QIcon& shuffleOnIcon, const QIcon& shuffleRandomIcon);

protected:
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void showContextMenu(const QPoint &pos);

private:
    Ui::ControlBar *ui;
    QButtonGroup* m_playlistButtons;
    QMenu* m_visibilityMenu;
    QList<QAction*> m_buttonActions;

    QIcon m_playIcon, m_pauseIcon, m_stopIcon, m_nextIcon, m_prevIcon;
    QIcon m_volumeIcons[4]; // 0:mute, 1:zero, 2:down, 3:up
    QIcon m_repeatIcons[3]; // 0:NoLoop, 1:RepeatOne, 2:RepeatAll
    QIcon m_shuffleIcons[3];

    bool isUpdatingVolume = false;
    bool m_isPlaying;
    LoopMode m_currentLoopMode;
    ShuffleMode m_currentShuffleMode;
    bool m_canGoNext;
    bool m_canGoPrev;
    bool m_isCompact;

    QLabel* m_currentTimeLabel;      // 左側: 現在時間
    QPushButton* m_totalTimeButton;  // 右側: 合計/残り時間 (クリック可能にするためボタン)

    qint64 m_currentPosition = 0;    // 現在位置 (ms)
    qint64 m_totalDuration = 0;      // 合計時間 (ms)
    bool m_showRemainingTime = false; // 残り時間表示モードフラグ

    QString formatTime(qint64 ms) const;
    void updateTimeLabels(); // 表示更新用
};

#endif // CONTROLBAR_H
