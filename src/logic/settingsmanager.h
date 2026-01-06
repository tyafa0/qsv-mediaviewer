#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include "imageviewcontroller.h"
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QMap>

enum class VideoSwitchPolicy {
    Default,    // 現在の仕様（スライドショーが空の時のみ切り替え）
    Always,     // 常にビデオページへ切り替え
    Never       // 切り替えない（現在のページを維持）
};

// 設定データを保持する構造体 (MainWindowから移動)
struct AppSettings {
    int chunkSize = 50;
    int preloadRange = 5;
    int progressTimerInterval = 16;
    int uiUpdateTimerInterval = 16;
    bool docksAreLocked = true;
    bool syncUiStateAcrossLists = true;
    bool scanSubdirectories = false;
    int fileScanLimit = 2000;
    bool autoUpdatePreviews = true;
    QString theme = "dark";
    QString lastViewedFile;
    QString lastBookshelfPath;
    int lastVolume = 50;
    bool contextMenuEnabled = false;
    bool isPanoramaMode = false;
    FitMode fitMode = FitInside;
    LayoutDirection layoutDirection = Forward;
    VideoSwitchPolicy switchOnOpenFile = VideoSwitchPolicy::Default;    // ファイルを開く/D&D
    VideoSwitchPolicy switchOnItemClick = VideoSwitchPolicy::Default;   // ダブルクリック/Enter
    VideoSwitchPolicy switchOnAutoPlay = VideoSwitchPolicy::Default;    // 自動再生/連続再生
    double slideshowInterval;
};

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject *parent = nullptr);

    // 設定の読み込み・保存
    void loadSettings();
    void saveSettings(); // 現在の m_settings を保存

    // 個別の値を保存するためのヘルパー
    void saveLastViewedFile(const QString& filePath);
    void saveLastVolume(int volume);
    void savePlaylistCount(int count);
    void saveRecentFiles(const QStringList &recentFiles);
    void saveBookshelfPath(const QString& path);
    void saveSlideshowInterval(double interval);
    void saveViewStates(bool isPanorama, FitMode fitMode, LayoutDirection layoutDirection);

    // 本棚のキャッシュ
    void setDirectoryHistory(const QString& dirPath, const QString& filePath);
    QString getDirectoryHistory(const QString& dirPath) const;

    // Windowsのレジストリ設定 (コンテキストメニュー登録)
    void updateRegistrySettings(bool enabled);

    // ゲッター
    AppSettings& settings() { return m_settings; }
    const AppSettings& settings() const { return m_settings; }

    QStringList recentFiles() const { return m_recentFiles; }
    int initialPlaylistCount() const { return m_initialPlaylistCount; }

    static const QString author;

signals:
    // 設定が変更されたことを通知（必要に応じて）
    void settingsChanged(const AppSettings &settings);

private:
    AppSettings m_settings;
    QStringList m_recentFiles;
    int m_initialPlaylistCount;
    QMap<QString, QString> m_directoryViewHistory;

    // 内部ヘルパー
    void optimizeForFirstLaunch(QSettings &settings);
};

#endif // SETTINGSMANAGER_H
