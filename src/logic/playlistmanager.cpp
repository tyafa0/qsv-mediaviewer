#include "playlistmanager.h"
#include <QFileInfo>
#include <QTimer>
#include <QDebug>
#include <random> // std::shuffle用
#include <algorithm> // std::sort用

PlaylistManager::PlaylistManager(QObject *parent)
    : QObject(parent)
    , m_currentPlaylistIndex(0)
    , m_playingPlaylistIndex(-1)
    , m_currentLoopMode(NoLoop)
    , m_shuffleMode(ShuffleOff)
    , m_shuffledIndex(0)
    , m_asyncOperations(0)
    , m_playHistoryIndex(-1)
{
}

PlaylistManager::~PlaylistManager()
{
}

// --- ゲッター関数 ---

int PlaylistManager::getCurrentTrackIndex() const
{
    if (m_currentPlaylistIndex < 0 || m_currentPlaylistIndex >= m_currentTrackIndexes.size()) {
        return -1;
    }
    return m_currentTrackIndexes[m_currentPlaylistIndex];
}

int PlaylistManager::getCurrentTrackCount() const
{
    if (m_currentPlaylistIndex < 0 || m_currentPlaylistIndex >= m_allPlaylists.size()) {
        return 0;
    }
    return m_allPlaylists[m_currentPlaylistIndex].count();
}

QStringList PlaylistManager::getPlaylist(int playlistIndex) const
{
    if (playlistIndex < 0 || playlistIndex >= m_allPlaylists.size()) {
        return QStringList();
    }
    return m_allPlaylists[playlistIndex];
}

bool PlaylistManager::canGoNext() const
{
    if (getCurrentTrackCount() == 0) return false;

    if (m_currentLoopMode == RepeatAll || m_shuffleMode == ShuffleRepeat) {
        return true;
    }
    if (m_shuffleMode == ShuffleNoRepeat) {
        return m_shuffledIndex < m_shuffledPlaylist.size() - 1;
    }
    // NoLoop or RepeatOne
    return getCurrentTrackIndex() < getCurrentTrackCount() - 1;
}

bool PlaylistManager::canGoPrevious() const
{
    if (getCurrentTrackCount() == 0) return false;
    // シャッフルOFF時は、先頭でなければ戻れる（ループ時は末尾に戻れるので常にtrue）
    if (m_shuffleMode == ShuffleOff) {
        return (m_currentLoopMode == RepeatAll) || (getCurrentTrackIndex() > 0);
    }
    // シャッフル時は履歴があるかどうか
    return m_playHistoryIndex > 0;
}


// --- メインロジック ---

void PlaylistManager::addPlaylist()
{
    m_allPlaylists.append(QStringList());
    m_currentTrackIndexes.append(-1);
}

void PlaylistManager::playTrackAtIndex(int trackIndex, int playlistIndex)
{
    bool isDuplicate = false;
    if (m_playHistoryIndex >= 0 && m_playHistoryIndex < m_playHistory.size()) {
        // 現在の履歴の末尾（現在再生中の曲）と同じインデックスなら重複とみなす
        if (m_playHistory[m_playHistoryIndex] == trackIndex && m_currentPlaylistIndex == playlistIndex) {
            isDuplicate = true;
        }
    }

    if (!isDuplicate) {
        // 1. (手動再生時) 「進む」履歴をクリア
        if (m_playHistoryIndex < m_playHistory.size() - 1) {
            m_playHistory = m_playHistory.mid(0, m_playHistoryIndex + 1);
        }

        // 2. 新しい曲を履歴に追加
        m_playHistory.append(trackIndex);
        m_playHistoryIndex = m_playHistory.size() - 1;
    }

    // 3. 内部再生関数を呼び出す
    playTrackInternal(trackIndex, playlistIndex);
}

void PlaylistManager::playTrackInternal(int trackIndex, int playlistIndex)
{
    // --- ここは playTrackAtIndex の元のロジック ---
    m_currentPlaylistIndex = playlistIndex; //
    m_playingPlaylistIndex = playlistIndex; //

    if (playlistIndex < 0 || playlistIndex >= m_allPlaylists.size()) { //
        emit playbackShouldStop(); //
        return;
    }

    QStringList& playlist = m_allPlaylists[playlistIndex]; //
    if (trackIndex < 0 || trackIndex >= playlist.size()) { //
        emit playbackShouldStop(); //
        return;
    }

    // トラックインデックスを保存
    m_currentTrackIndexes[playlistIndex] = trackIndex; //

    // シャッフルモード(重複なし)の場合、再生した曲がシャッフルリストの何番目かを同期
    if (m_shuffleMode == ShuffleNoRepeat) { //
        m_shuffledIndex = m_shuffledPlaylist.indexOf(trackIndex); //
    }

    // 再生すべき曲をシグナルで通知
    emit trackReadyToPlay(playlist.at(trackIndex), playlistIndex, trackIndex); //
}

void PlaylistManager::requestNextTrack()
{
    // 現在のプレイリストが空なら何もしない
    if (getCurrentTrackCount() == 0) return;

    int playlistIndex = m_currentPlaylistIndex;
    QStringList& currentPlaylist = m_allPlaylists[playlistIndex];

    // 1曲リピートの場合 (手動操作でもリピートを優先するか、スキップするかは設計次第ですが、
    // 既存コードに合わせてリピート優先としています)
    if (m_currentLoopMode == RepeatOne) {
        playTrackAtIndex(getCurrentTrackIndex(), playlistIndex);
        return;
    }

    int newIndex = -1;

    switch (m_shuffleMode) {
    case ShuffleOff: // 通常のシーケンシャル再生
    {
        // ★履歴に関係なく、現在のリスト位置の「次」を取得
        int currentIndex = getCurrentTrackIndex();
        newIndex = currentIndex + 1;

        if (newIndex >= currentPlaylist.size()) {
            if (m_currentLoopMode == RepeatAll) {
                newIndex = 0; // 全曲リピートなら最初に戻る
            } else {
                emit playbackShouldStop(); // ループなしなら停止
                return;
            }
        }
    }
    break;

    case ShuffleNoRepeat: // 重複なしランダム再生
    {
        if (m_shuffledPlaylist.isEmpty()) generateShuffledPlaylist();

        m_shuffledIndex++; // 再生リストの次の曲へ
        if (m_shuffledIndex >= m_shuffledPlaylist.size()) {
            // リストの最後まで再生しきった
            if (m_currentLoopMode == RepeatAll) {
                // リセットして再生成
                int lastPlayedIndex = m_shuffledPlaylist.isEmpty() ? -1 : m_shuffledPlaylist.last();
                generateShuffledPlaylist();
                // 直前と同じ曲が先頭に来ないようにスワップ
                if (!m_shuffledPlaylist.isEmpty() && m_shuffledPlaylist.size() > 1 &&
                    m_shuffledPlaylist.first() == lastPlayedIndex) {
                    m_shuffledPlaylist.swapItemsAt(0, 1);
                }
                m_shuffledIndex = 0;
                newIndex = m_shuffledPlaylist.value(m_shuffledIndex, -1);
            } else {
                emit playbackShouldStop(); // ループなしなら停止
                return;
            }
        } else {
            newIndex = m_shuffledPlaylist.value(m_shuffledIndex, -1);
        }
    }
    break;

    case ShuffleRepeat: // 重複ありランダム再生
    {
        newIndex = QRandomGenerator::global()->bounded(currentPlaylist.size());
    }
    break;
    }

    if (newIndex != -1) {
        playTrackAtIndex(newIndex, playlistIndex);
    }
}

void PlaylistManager::requestPreviousTrack()
{
    // 現在のプレイリストが空なら何もしない
    if (getCurrentTrackCount() == 0) return;

    // 1曲リピートの場合
    if (m_currentLoopMode == RepeatOne) {
        playTrackAtIndex(getCurrentTrackIndex(), m_currentPlaylistIndex);
        return;
    }

    // シャッフルモードが有効な場合は、既存の「履歴」ロジックを使用
    if (m_shuffleMode != ShuffleOff) {
        if (m_playHistoryIndex > 0) {
            m_playHistoryIndex--;
            int trackIndex = m_playHistory.at(m_playHistoryIndex);
            playTrackInternal(trackIndex, m_currentPlaylistIndex);
        }
        return;
    }

    // --- 【修正】 通常再生（シャッフルOFF）のロジック ---
    // 履歴に依存せず、現在のインデックスを基準に計算する
    int currentIndex = getCurrentTrackIndex();
    int newIndex = currentIndex - 1;

    // リストの先頭より前になった場合
    if (newIndex < 0) {
        if (m_currentLoopMode == RepeatAll) {
            // 全リピートなら末尾へループ
            newIndex = getCurrentTrackCount() - 1;
        } else {
            // ループなしなら先頭に戻る (停止はせず頭出し)
            newIndex = 0;
        }
    }

    // 決定したインデックスで再生
    playTrackAtIndex(newIndex, m_currentPlaylistIndex);
}

// --- 状態変更スロット ---

void PlaylistManager::setLoopMode(LoopMode mode)
{
    m_currentLoopMode = mode;
}

void PlaylistManager::setShuffleMode(ShuffleMode mode)
{
    m_shuffleMode = mode;
    if (m_shuffleMode == ShuffleNoRepeat) {
        generateShuffledPlaylist();
    }
    clearPlayHistory();
}

void PlaylistManager::setCurrentPlaylistIndex(int index)
{
    if (index >= 0 && index < m_allPlaylists.size()) {
        m_currentPlaylistIndex = index;
    }
}

void PlaylistManager::stopPlayback()
{
    m_playingPlaylistIndex = -1;
    // 現在のトラックインデックスはリセットしない（停止した場所を覚えておく）
}

// --- データ操作 ---

void PlaylistManager::addFilesToPlaylist(const QStringList &files, int playlistIndex)
{
    if (playlistIndex < 0 || playlistIndex >= m_allPlaylists.size() || files.isEmpty()) return;

    emit loadingStateChanged(true); // ローディング開始を通知
    m_asyncOperations++;
    addFilesToPlaylistChunked(files, playlistIndex, 0);

    if (m_shuffleMode == ShuffleNoRepeat && playlistIndex == m_currentPlaylistIndex) {
        generateShuffledPlaylist();
    }
}

void PlaylistManager::addFilesToPlaylistChunked(const QStringList &files, int playlistIndex, int startIndex)
{
    const int chunkSize = 50; // 一度に処理するファイル数
    QStringList& targetPlaylist = m_allPlaylists[playlistIndex];
    int originalItemCount = (startIndex == 0) ? targetPlaylist.count() : -1;

    int endIndex = qMin(startIndex + chunkSize, files.size());

    for (int i = startIndex; i < endIndex; ++i) {
        targetPlaylist.append(files.at(i));
    }

    if (endIndex < files.size()) {
        QTimer::singleShot(0, this, [=](){ addFilesToPlaylistChunked(files, playlistIndex, endIndex); });
    } else {
        // 全てのファイルの追加が完了
        emit playlistDataChanged(playlistIndex);

        // "再生中" ではない場合、追加したファイルの先頭から再生を開始
        if (m_playingPlaylistIndex == -1) {
            playTrackAtIndex(originalItemCount, playlistIndex);
        }

        m_asyncOperations--;
        if (m_asyncOperations == 0) {
            emit loadingStateChanged(false); // ローディング終了を通知
        }
    }
}

void PlaylistManager::removePlaylist(int index)
{
    if (index < 0 || index >= m_allPlaylists.size()) return;
    m_allPlaylists.removeAt(index);
    m_currentTrackIndexes.removeAt(index);
}

void PlaylistManager::removeIndices(int playlistIndex, const QList<int>& indices)
{
    if (playlistIndex < 0 || playlistIndex >= m_allPlaylists.size()) return;

    QStringList& playlist = m_allPlaylists[playlistIndex];

    // インデックスがずれないように、降順でソート
    QList<int> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());

    for (int index : sortedIndices) {
        if (index >= 0 && index < playlist.size()) {
            playlist.removeAt(index);
        }
    }
    emit playlistDataChanged(playlistIndex);
    if (m_shuffleMode == ShuffleNoRepeat) generateShuffledPlaylist();
    if (playlistIndex == m_currentPlaylistIndex) {
        clearPlayHistory();
    }
}

void PlaylistManager::clearPlaylist(int playlistIndex)
{
    if (playlistIndex < 0 || playlistIndex >= m_allPlaylists.size()) return;

    m_allPlaylists[playlistIndex].clear();
    m_currentTrackIndexes[playlistIndex] = -1;

    emit playlistDataChanged(playlistIndex);
    if (m_shuffleMode == ShuffleNoRepeat) generateShuffledPlaylist();
    if (playlistIndex == m_currentPlaylistIndex) {
        clearPlayHistory();
    }
}

void PlaylistManager::clearPlayHistory()
{
    m_playHistory.clear();
    m_playHistoryIndex = -1;

    if (m_playingPlaylistIndex >= 0 && m_playingPlaylistIndex < m_currentTrackIndexes.size()) {
        int currentTrack = m_currentTrackIndexes[m_playingPlaylistIndex]; //
        if (currentTrack != -1) {
            // 現在再生中のトラックを履歴の開始点として追加
            m_playHistory.append(currentTrack);
            m_playHistoryIndex = 0; // 履歴のインデックスを0に設定
            qDebug() << "Play history cleared and re-initialized with current track:" << currentTrack;
        }
    }
}

void PlaylistManager::syncPlaylistOrder(int playlistIndex, int from, int to)
{
    if (playlistIndex < 0 || playlistIndex >= m_allPlaylists.size()) return;

    int dest = (from < to) ? to - 1 : to;
    m_allPlaylists[playlistIndex].move(from, dest);

    // 再生中のインデックスを追跡
    if (playlistIndex == m_playingPlaylistIndex) {
        int& currentIndex = m_currentTrackIndexes[playlistIndex];
        if (currentIndex == from) {
            currentIndex = dest;
        } else if (from < dest && currentIndex > from && currentIndex <= dest) {
            currentIndex--;
        } else if (from > dest && currentIndex >= dest && currentIndex < from) {
            currentIndex++;
        }
    }

    emit playlistDataChanged(playlistIndex);

    if (playlistIndex == m_currentPlaylistIndex) {
        clearPlayHistory();
    }
}

void PlaylistManager::generateShuffledPlaylist()
{
    m_shuffledPlaylist.clear();
    m_shuffledIndex = 0;

    int trackCount = getCurrentTrackCount();
    if (trackCount == 0) return;

    for (int i = 0; i < trackCount; ++i) {
        m_shuffledPlaylist.append(i);
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_shuffledPlaylist.begin(), m_shuffledPlaylist.end(), g);

    qDebug() << "Generated shuffled playlist:" << m_shuffledPlaylist;
}
