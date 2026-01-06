#ifndef PLAYLISTMANAGER_H
#define PLAYLISTMANAGER_H

#include <QObject>
#include <QStringList>
#include <QList>
#include <QRandomGenerator> // <--- 追加
#include <algorithm> // <--- 追加

// --- enum定義を mainwindow.h から移動 ---
enum LoopMode {
    NoLoop,
    RepeatOne,
    RepeatAll
};

enum ShuffleMode {
    ShuffleOff,      // ランダム再生なし
    ShuffleNoRepeat, // 重複なしランダム
    ShuffleRepeat    // 重複ありランダム
};

class PlaylistManager : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistManager(QObject *parent = nullptr);
    ~PlaylistManager();

    // --- MainWindow (UI) から状態を取得するためのゲッター ---
    LoopMode getLoopMode() const { return m_currentLoopMode; }
    ShuffleMode getShuffleMode() const { return m_shuffleMode; }
    Q_INVOKABLE QStringList getPlaylist(int playlistIndex) const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    int getCurrentTrackCount() const;

public slots:
    // --- MainWindow (UI) や MediaManager からのイベントを処理するスロット ---
    void addPlaylist();
    void requestNextTrack();
    void requestPreviousTrack();
    void playTrackAtIndex(int trackIndex, int playlistIndex);
    void addFilesToPlaylist(const QStringList &files, int playlistIndex);
    void removePlaylist(int index);
    void removeIndices(int playlistIndex, const QList<int>& indices);
    void clearPlaylist(int playlistIndex);
    void syncPlaylistOrder(int playlistIndex, int from, int to);
    void setLoopMode(LoopMode mode);
    void setShuffleMode(ShuffleMode mode);
    void setCurrentPlaylistIndex(int index);
    void stopPlayback();

signals:
    // --- MediaManager や MainWindow に通知するシグナル ---
    void trackReadyToPlay(const QString& filePath, int playlistIndex, int trackIndex);
    void playbackShouldStop();
    void playlistDataChanged(int playlistIndex); // プレビュー更新用
    void loadingStateChanged(bool isLoading); // ファイル追加時のローディング用

private slots:
    void addFilesToPlaylistChunked(const QStringList &files, int playlistIndex, int startIndex);

private:
    void clearPlayHistory();
    void playTrackInternal(int trackIndex, int playlistIndex);
    void generateShuffledPlaylist();
    int getCurrentTrackIndex() const;

    // --- データメンバ ---
    QList<QStringList> m_allPlaylists;
    QList<int> m_currentTrackIndexes;

    // --- 状態変数 ---
    int m_currentPlaylistIndex;
    int m_playingPlaylistIndex;
    LoopMode m_currentLoopMode;
    ShuffleMode m_shuffleMode;
    QList<int> m_playHistory;
    int m_playHistoryIndex;

    // --- シャッフル用 ---
    QList<int> m_shuffledPlaylist;
    int m_shuffledIndex;

    // --- 非同期処理用 ---
    int m_asyncOperations;
};

#endif // PLAYLISTMANAGER_H
