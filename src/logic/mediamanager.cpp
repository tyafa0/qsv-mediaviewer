#include "mediamanager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTime>
#include <QTimer>
#include <QtMath>

#include <SDL.h>
#include <SDL_mixer.h>

MediaManager* MediaManager::instance = nullptr;

MediaManager::MediaManager(WId wid, QObject *parent)
    : QObject(parent)
    , m_mpv(nullptr)
    , m_mpvEventTimer(nullptr)
    , m_progressTimer(nullptr)
    , m_music(nullptr)
    , m_volumeGain(0.2f)
    , m_currentVolumePercent(20)
    , m_isMuted(false)
    , m_volumeBeforeMute(20)
    , m_isPlaying(false)
    , m_isPaused(false)
{
    instance = this;

    // --- SDLの初期化 ---
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        qDebug() << "SDL Init Error:" << SDL_GetError();
    }
    if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT, AUDIO_CHANNELS, AUDIO_CHUNK_SIZE) < 0) {
        qDebug() << "SDL_mixer Init Error:" << Mix_GetError();
    }
    // SDLコールバックの登録
    Mix_RegisterEffect(MIX_CHANNEL_POST, amplifyEffect, nullptr, this);
    Mix_HookMusicFinished(musicFinishedCallback);

    // --- mpvの初期化 ---
    m_mpv = mpv_create();
    if (m_mpv) {
        mpv_initialize(m_mpv);
        mpv_set_property(m_mpv, "wid", MPV_FORMAT_INT64, &wid);
        mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);

        // mpvのイベントを監視するためのタイマーを開始
        m_mpvEventTimer = new QTimer(this);
        connect(m_mpvEventTimer, &QTimer::timeout, this, &MediaManager::handleMpvEvents);
    }

    // --- 再生位置更新タイマーのセットアップ ---
    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, &MediaManager::updateProgress);
}

MediaManager::~MediaManager()
{
    if (m_music) { Mix_FreeMusic(m_music); }
    Mix_CloseAudio();
    SDL_Quit();

    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
    }
}

void MediaManager::play(const QString& filePath)
{
    stop(); // まず現在の再生を停止

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    QStringList audioExtensions = {"mp3", "wav", "ogg", "flac"};
    QStringList videoExtensions = {"mp4", "mkv", "avi", "mov", "wmv"};

    if (audioExtensions.contains(suffix)) {
        // --- 音声ファイルの場合 (SDL) ---
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) { return; }
        m_currentMusicData = file.readAll();
        file.close();

        SDL_RWops* rw = SDL_RWFromConstMem(m_currentMusicData.constData(), m_currentMusicData.size());
        if (!rw) return;

        m_music = Mix_LoadMUS_RW(rw, 1); // 1 = auto-free the RWops
        if (!m_music) {
            qDebug() << "Mix_LoadMUS_RW Error:" << Mix_GetError();
            return;
        }

        if (Mix_PlayMusic(m_music, 1) == -1) { // ループはPlaylistManagerが担当
            qDebug() << "Mix_PlayMusic Error:" << Mix_GetError();
        }
        m_progressTimer->start(10); // TODO: この間隔は設定から渡す
        m_isPlaying = true;
        m_isPaused = false;
        emit playbackStateChanged(true);

    } else if (videoExtensions.contains(suffix)) {
        // --- ビデオファイルの場合 (mpv) ---
        emit loadingStateChanged(true); // 読込中表示を開始

        // mpvに再生コマンドを送る
        QString command = QString("loadfile \"%1\"").arg(filePath);
        mpv_command_string(m_mpv, command.toUtf8().constData());
        m_mpvEventTimer->start(MPV_EVENT_TIMER_INTERVAL);
        m_isPlaying = true;
        m_isPaused = false;
        // playbackStateChangedは "file-loaded" か "pause" プロパティ変更イベントで発行
    }
}

void MediaManager::stop()
{
    // SDLの停止
    Mix_HookMusicFinished(nullptr); // 一時的にコールバックを解除
    Mix_HaltMusic();
    if (m_music) {
        Mix_FreeMusic(m_music);
        m_music = nullptr;
    }
    // mpvの停止
    mpv_command_string(m_mpv, "stop");

    // タイマーの停止
    m_progressTimer->stop();
    m_mpvEventTimer->stop();

    m_isPlaying = false;
    m_isPaused = false;
    emit playbackStateChanged(false);
    emit loadingStateChanged(false);

    Mix_HookMusicFinished(musicFinishedCallback); // コールバックを再登録
}

void MediaManager::togglePlayback()
{
    if (!m_isPlaying && !m_isPaused) {
        // 完全に停止している場合、何もしない（再生開始はplay()スロット経由）
        return;
    }

    // --- mpvの再生状態を取得 ---
    int mpv_pause_flag = 1; // 1 = Paused/Stopped
    if (isMpvActive()) {
        mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &mpv_pause_flag);
    }

    // --- 現在再生中かどうかを判定 ---
    bool isSdlPlaying = Mix_PlayingMusic() && !Mix_PausedMusic();
    bool isMpvPlaying = isMpvActive() && mpv_pause_flag == 0;
    bool isPlaying = isSdlPlaying || isMpvPlaying;

    // --- 現在一時停止中かどうかを判定 ---
    bool isSdlPaused = Mix_PlayingMusic() && Mix_PausedMusic();
    bool isMpvPaused = isMpvActive() && mpv_pause_flag == 1;
    bool isPaused = isSdlPaused || isMpvPaused;

    if (isPlaying) {
        // --- 1. 再生中の場合 → 一時停止 ---
        if (isSdlPlaying) Mix_PauseMusic();
        if (isMpvPlaying) mpv_command_string(m_mpv, "set pause yes");
        m_isPlaying = false;
        m_isPaused = true;
        emit playbackStateChanged(false);

    } else if (isPaused) {
        // --- 2. 一時停止中の場合 → 再生再開 ---
        if (isSdlPaused) Mix_ResumeMusic();
        if (isMpvPaused) mpv_command_string(m_mpv, "set pause no");
        m_isPlaying = true;
        m_isPaused = false;
        emit playbackStateChanged(true);
    }
}

void MediaManager::setPosition(int pos)
{
    if (m_music) { // SDL
        Mix_SetMusicPosition(pos / 1000.0);
    } else { // mpv
        double pos_sec = pos / 1000.0;
        mpv_set_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos_sec);
    }
}

void MediaManager::setVolume(int percent)
{
    m_currentVolumePercent = percent;

    // 1. 内部変数とmpvの音量を更新
    if (!m_isMuted) {
        // --- SDL_mixer用のゲイン (3乗カーブ) ---
        float normalized = percent / 100.0f;
        this->m_volumeGain = normalized * normalized * normalized;

        // mpv用の音量を更新
        if (m_mpv) {
            double mpv_volume = static_cast<double>(percent);
            mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &mpv_volume);
        }
    }
    // 2. UIに変更を通知
    emit volumeChanged(percent, m_isMuted);
}

void MediaManager::handleMuteClicked()
{
    m_isMuted = !m_isMuted;

    if (m_mpv) {
        int mute_flag = m_isMuted ? 1 : 0;
        mpv_set_property(m_mpv, "mute", MPV_FORMAT_FLAG, &mute_flag);
    }

    int currentVolume;
    if (m_isMuted) {
        // 現在の音量を保存 (メンバ変数を使えばよいので m_volumeBeforeMute も不要になりますが、念のためロジック維持)
        m_volumeBeforeMute = m_currentVolumePercent; // ★修正
        m_volumeGain = 0.0f;
        currentVolume = m_volumeBeforeMute;
    } else {
        // 保存しておいた音量に戻す
        currentVolume = m_volumeBeforeMute;

        // ★修正: 復帰時も setVolume と同じ計算を行う (ここ重要)
        float normalized = currentVolume / 100.0f;
        m_volumeGain = normalized * normalized * normalized;

        if (m_mpv) {
            double mpv_volume = static_cast<double>(currentVolume);
            mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &mpv_volume);
        }
    }

    emit volumeChanged(currentVolume, m_isMuted);
}

void MediaManager::handlePositionSliderPressed()
{
    if (m_progressTimer->isActive()) {
        m_progressTimer->stop();
    }
}

void MediaManager::handlePositionSliderReleased()
{
    if (m_music && Mix_PlayingMusic()) {
        m_progressTimer->start(10); // TODO: 設定から渡す
    }
}

void MediaManager::handleMpvEvents()
{
    while (m_mpv) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break; // イベントがなければループを抜ける
        }

        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            // ファイルの読み込みが完了したら、読込中表示を終了
            emit loadingStateChanged(false);
            m_isPlaying = true;
            m_isPaused = false;
            emit playbackStateChanged(true);
        }

        if (event->event_id == MPV_EVENT_END_FILE) {
            mpv_event_end_file *end_file_event = (mpv_event_end_file *)event->data;
            if (end_file_event->reason == MPV_END_FILE_REASON_EOF) {
                // 再生が「自然に終了した(EOF)」場合のみ、次の曲へ進む
                m_isPlaying = false;
                m_isPaused = false;
                emit trackFinished();
            } else {
                // 停止ボタンなどで停止した場合
                m_isPlaying = false;
                m_isPaused = false;
                emit playbackStateChanged(false);
            }
            emit loadingStateChanged(false);
        }

        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            mpv_event_property *prop = (mpv_event_property *)event->data;
            if (strcmp(prop->name, "pause") == 0) {
                // mpvの "pause" プロパティが変更された
                int* pause_flag = (int*)prop->data;
                if (*pause_flag == 0) { // 再生開始
                    m_isPlaying = true;
                    m_isPaused = false;
                } else { // 一時停止
                    m_isPlaying = false;
                    m_isPaused = true;
                }
                emit playbackStateChanged(m_isPlaying);
            }
        }
    }

    // 再生位置と時間表示の更新
    if (isMpvActive() && m_isPlaying) {
        double position = 0;
        double duration = 0;
        mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &position);
        mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &duration);

        qint64 pos_ms = static_cast<qint64>(position * 1000);
        qint64 dur_ms = static_cast<qint64>(duration * 1000);

        emit positionChanged(pos_ms);
        emit durationChanged(dur_ms);

        QString current = formatTime(pos_ms);
        QString total = formatTime(dur_ms);
        emit timeLabelChanged(current + " / " + total);
    }
}

void MediaManager::handleMusicFinished()
{
    m_isPlaying = false;
    m_isPaused = false;
    emit trackFinished();
}

void MediaManager::updateProgress()
{
    if (!m_music || !Mix_PlayingMusic()) return;

    double position = Mix_GetMusicPosition(m_music);
    double duration = Mix_MusicDuration(m_music);

    qint64 pos_ms = static_cast<qint64>(position * 1000);
    qint64 dur_ms = static_cast<qint64>(duration * 1000);

    emit positionChanged(pos_ms);
    emit durationChanged(dur_ms);

    const QString current = formatTime(pos_ms);
    const QString total = formatTime(dur_ms);
    emit timeLabelChanged(current + " / " + total);
}

QString MediaManager::formatTime(qint64 ms)
{
    QTime t(0, 0);
    t = t.addMSecs(ms);
    return ms >= 3600000 ? t.toString("hh:mm:ss") : t.toString("mm:ss");
}

bool MediaManager::isMpvActive() const
{
    if (!m_mpv) return false;
    char *path = nullptr;
    if (mpv_get_property(m_mpv, "path", MPV_FORMAT_STRING, &path) == 0 && path) {
        mpv_free(path);
        return true;
    }
    return false;
}

int MediaManager::getCurrentVolume() const
{
    if (m_isMuted) return 0;

    return m_currentVolumePercent;
}

void MediaManager::musicFinishedCallback()
{
    if (instance) {
        QMetaObject::invokeMethod(instance, "handleMusicFinished", Qt::QueuedConnection);
    }
}

void MediaManager::amplifyEffect(int chan, void *stream, int len, void *udata)
{
    MediaManager* manager = static_cast<MediaManager*>(udata);
    if (!manager || manager->m_volumeGain == 1.0f || manager->m_isMuted) return;

    Sint16* samples = (Sint16*)stream;
    int numSamples = len / sizeof(Sint16);
    float gain = manager->m_isMuted ? 0.0f : manager->m_volumeGain; // ミュート対応

    for (int i = 0; i < numSamples; ++i) {
        float amplifiedSample = samples[i] * gain;
        if (amplifiedSample > 32767) amplifiedSample = 32767;
        else if (amplifiedSample < -32768) amplifiedSample = -32768;
        samples[i] = static_cast<Sint16>(amplifiedSample);
    }
}
