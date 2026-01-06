#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <mpv/client.h>
#include <SDL_mixer.h>
#include <qwindowdefs.h>

class MediaManager : public QObject
{
    Q_OBJECT

public:
    // コンストラクタで、mpvが描画する先のウィンドウID (wid) を受け取ります
    explicit MediaManager(WId wid, QObject *parent = nullptr);
    ~MediaManager();

    // --- 現在の状態を外部に公開するゲッター ---
    bool isPlaying() const { return m_isPlaying; }
    bool isPaused() const { return m_isPaused; }
    bool isMuted() const { return m_isMuted; }
    int getCurrentVolume() const;
    int getVolumeBeforeMute() const { return m_volumeBeforeMute; }

public slots:
    // --- MainWindowから呼び出されるスロット ---
    void play(const QString& filePath);
    void stop();
    void togglePlayback();
    void setPosition(int ms);
    void setVolume(int percent);
    void handleMuteClicked();
    void handlePositionSliderPressed();
    void handlePositionSliderReleased();

signals:
    // --- MainWindow (UI) に状態変化を通知するシグナル ---
    void playbackStateChanged(bool isPlaying);
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void timeLabelChanged(const QString& newTimeLabel);
    void trackFinished();
    void volumeChanged(int percent, bool isMuted);
    void loadingStateChanged(bool isLoading);

private slots:
    // --- 内部タイマーで呼び出されるスロット ---
    void handleMpvEvents();
    void handleMusicFinished();
    void updateProgress();

private:
    // --- ヘルパー関数 ---
    QString formatTime(qint64 ms);
    bool isMpvActive() const;

    // --- SDLコールバック ---
    static void musicFinishedCallback();
    static void amplifyEffect(int chan, void *stream, int len, void *udata);

    // --- データメンバ ---
    static MediaManager* instance; // SDLコールバック用
    mpv_handle *m_mpv;
    QTimer *m_mpvEventTimer;
    QTimer *m_progressTimer;

    Mix_Music *m_music;
    QByteArray m_currentMusicData;

    // --- 状態変数 ---
    float m_volumeGain;
    int m_currentVolumePercent;
    bool m_isMuted;
    int m_volumeBeforeMute;
    bool m_isPlaying;
    bool m_isPaused;

    // --- 定数 ---
    static const int MPV_EVENT_TIMER_INTERVAL = 20;
    static const int AUDIO_FREQUENCY = 44100;
    static const int AUDIO_CHANNELS = 2;
    static const int AUDIO_CHUNK_SIZE = 2048;
};

#endif // MEDIAMANAGER_H
