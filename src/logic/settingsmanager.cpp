#include "settingsmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QDebug>

#if defined(Q_OS_WIN)
#include <shlobj.h>
#include <windows.h>
#endif


#if defined(Q_OS_WIN)
bool WriteRegistryString(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& data)
{
    HKEY hKey;
    // キーを作成または開く
    LONG result = RegCreateKeyExW(hKeyRoot, subKey.c_str(), 0, NULL,
                                  REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS) return false;

    const wchar_t* pValueName = valueName.empty() ? NULL : valueName.c_str();
    DWORD dataSize = (data.size() + 1) * sizeof(wchar_t);
    result = RegSetValueExW(hKey, pValueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(data.c_str()), dataSize);
    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

bool DeleteRegistryKey(HKEY hKeyRoot, const std::wstring& subKey)
{
    // 再帰的に削除
    return RegDeleteTreeW(hKeyRoot, subKey.c_str()) == ERROR_SUCCESS;
}
#endif

const QString SettingsManager::author = "Anonymous";

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_initialPlaylistCount(3)
{
}

void SettingsManager::loadSettings()
{
    QSettings settings(author, "QSupportViewer");

    // --- 1. General グループの読み込み ---
    settings.beginGroup("General");

    // 初回起動時の最適化チェック
    bool isFirstLaunch = settings.value("firstLaunch", true).toBool();
    int defaultProgressInterval = 16;
    int defaultUiInterval = 16;

    if (isFirstLaunch) {
        optimizeForFirstLaunch(settings);
        defaultProgressInterval = settings.value("progressTimerInterval", 16).toInt();
        defaultUiInterval = settings.value("uiUpdateTimerInterval", 16).toInt();
    }

    m_settings.chunkSize = settings.value("chunkSize", 50).toInt();
    m_settings.preloadRange = settings.value("preloadRange", 5).toInt();
    m_settings.progressTimerInterval = settings.value("progressTimerInterval", defaultProgressInterval).toInt();
    m_settings.uiUpdateTimerInterval = settings.value("uiUpdateTimerInterval", defaultUiInterval).toInt();
    m_settings.docksAreLocked = settings.value("docksAreLocked", true).toBool();
    m_settings.syncUiStateAcrossLists = settings.value("syncUiStateAcrossLists", true).toBool();
    m_settings.scanSubdirectories = settings.value("scanSubdirectories", false).toBool();
    m_settings.fileScanLimit = settings.value("fileScanLimit", 2000).toInt();
    m_settings.autoUpdatePreviews = settings.value("autoUpdatePreviews", true).toBool();
    m_settings.theme = settings.value("theme", "light").toString();
    m_settings.lastVolume = settings.value("lastVolume", 32).toInt();
    m_settings.contextMenuEnabled = settings.value("contextMenuEnabled", false).toBool();
    m_settings.switchOnOpenFile = static_cast<VideoSwitchPolicy>(settings.value("switchOnOpenFile", static_cast<int>(VideoSwitchPolicy::Default)).toInt());
    m_settings.switchOnItemClick = static_cast<VideoSwitchPolicy>(settings.value("switchOnItemClick", static_cast<int>(VideoSwitchPolicy::Default)).toInt());
    m_settings.switchOnAutoPlay = static_cast<VideoSwitchPolicy>(settings.value("switchOnAutoPlay", static_cast<int>(VideoSwitchPolicy::Default)).toInt());
    m_settings.slideshowInterval = settings.value("slideshowInterval", 3.0).toDouble();

    m_recentFiles = settings.value("recentFiles").toStringList();
    m_initialPlaylistCount = settings.value("playlistCount", 3).toInt();

    settings.endGroup();
    // ▲ Generalグループ終了。これ以降はルートレベル、または別のキー階層を読む

    // --- 2. その他の設定 (ルートレベルで管理されているもの) ---
    // saveBookshelfPath や saveViewStates はグループを指定せずに保存しているため、
    // ここで読み込む必要があります。
    m_settings.lastViewedFile = settings.value("Bookshelf/lastViewedFile", "").toString();
    m_settings.lastBookshelfPath = settings.value("Bookshelf/lastPath", QDir::homePath()).toString();


    m_settings.isPanoramaMode = settings.value("View/isPanoramaMode", false).toBool();
    int fitVal = settings.value("View/fitMode", static_cast<int>(FitInside)).toInt();
    m_settings.fitMode = static_cast<FitMode>(fitVal);
    int dirVal = settings.value("View/layoutDirection", static_cast<int>(Forward)).toInt();
    m_settings.layoutDirection = static_cast<LayoutDirection>(dirVal);

    qDebug() << "Settings loaded.";
    qDebug() << "  Bookshelf Path:" << m_settings.lastBookshelfPath;
    qDebug() << "  Panorama Mode:" << m_settings.isPanoramaMode;

    settings.beginGroup("DirectoryHistory");
    int size = settings.beginReadArray("History");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString dir = settings.value("dir").toString();
        QString file = settings.value("file").toString();
        if (!dir.isEmpty() && !file.isEmpty()) {
            m_directoryViewHistory.insert(dir, file);
        }
    }
    settings.endArray();
    settings.endGroup();

    qDebug() << "History loaded items:" << m_directoryViewHistory.size();
}

void SettingsManager::saveSettings()
{
    QSettings settings(author, "QSupportViewer");
    settings.beginGroup("General");

    settings.setValue("chunkSize", m_settings.chunkSize);
    settings.setValue("preloadRange", m_settings.preloadRange);
    settings.setValue("progressTimerInterval", m_settings.progressTimerInterval);
    settings.setValue("uiUpdateTimerInterval", m_settings.uiUpdateTimerInterval);
    settings.setValue("docksAreLocked", m_settings.docksAreLocked);
    settings.setValue("syncUiStateAcrossLists", m_settings.syncUiStateAcrossLists);
    settings.setValue("scanSubdirectories", m_settings.scanSubdirectories);
    settings.setValue("fileScanLimit", m_settings.fileScanLimit);
    settings.setValue("autoUpdatePreviews", m_settings.autoUpdatePreviews);
    settings.setValue("contextMenuEnabled", m_settings.contextMenuEnabled);
    settings.setValue("theme", m_settings.theme);
    settings.setValue("switchOnOpenFile", static_cast<int>(m_settings.switchOnOpenFile));
    settings.setValue("switchOnItemClick", static_cast<int>(m_settings.switchOnItemClick));
    settings.setValue("switchOnAutoPlay", static_cast<int>(m_settings.switchOnAutoPlay));
    settings.setValue("slideshowInterval", m_settings.slideshowInterval);

    settings.endGroup();

    settings.beginGroup("DirectoryHistory");
    settings.beginWriteArray("History");
    int i = 0;
    QMapIterator<QString, QString> it(m_directoryViewHistory);
    while (it.hasNext()) {
        it.next();
        settings.setArrayIndex(i++);
        settings.setValue("dir", it.key());
        settings.setValue("file", it.value());
    }
    settings.endArray();
    settings.endGroup();
    qDebug() << "Settings saved.";
}

void SettingsManager::saveLastViewedFile(const QString& filePath)
{
    QSettings settings(author, "QSupportViewer");
    settings.setValue("Bookshelf/lastViewedFile", filePath);
    m_settings.lastViewedFile = filePath;
}

void SettingsManager::saveLastVolume(int volume)
{
    QSettings settings(author, "QSupportViewer");
    settings.beginGroup("General");
    settings.setValue("lastVolume", volume);
    settings.endGroup();
    m_settings.lastVolume = volume;
}

void SettingsManager::savePlaylistCount(int count)
{
    QSettings settings(author, "QSupportViewer");
    settings.beginGroup("General");
    settings.setValue("playlistCount", count);
    settings.endGroup();
}

void SettingsManager::saveRecentFiles(const QStringList &recentFiles)
{
    QSettings settings(author, "QSupportViewer");
    settings.beginGroup("General");
    settings.setValue("recentFiles", recentFiles);
    settings.endGroup();
    m_recentFiles = recentFiles;
}

void SettingsManager::saveBookshelfPath(const QString& path)
{
    QSettings settings(author, "QSupportViewer");
    // ルート直下の "Bookshelf/lastPath" に保存
    settings.setValue("Bookshelf/lastPath", path);
    m_settings.lastBookshelfPath = path;
}

void SettingsManager::saveSlideshowInterval(double interval)
{
    QSettings settings(author, "QSupportViewer");
    settings.beginGroup("General");
    settings.setValue("slideshowInterval", interval);
    settings.endGroup();

    m_settings.slideshowInterval = interval;
}

void SettingsManager::saveViewStates(bool isPanorama, FitMode fitMode, LayoutDirection layoutDirection)
{
    QSettings settings(author, "QSupportViewer");
    // ルート直下の "View/..." に保存
    settings.setValue("View/isPanoramaMode", isPanorama);
    settings.setValue("View/fitMode", static_cast<int>(fitMode));
    settings.setValue("View/layoutDirection", static_cast<int>(layoutDirection));

    m_settings.isPanoramaMode = isPanorama;
    m_settings.fitMode = fitMode;
    m_settings.layoutDirection = layoutDirection;
}

void SettingsManager::setDirectoryHistory(const QString& dirPath, const QString& filePath)
{
    if (dirPath.isEmpty() || filePath.isEmpty()) return;
    m_directoryViewHistory.insert(dirPath, filePath);
}

QString SettingsManager::getDirectoryHistory(const QString& dirPath) const
{
    return m_directoryViewHistory.value(dirPath, QString());
}

void SettingsManager::updateRegistrySettings(bool enabled)
{
#if defined(Q_OS_WIN)
    QString appPath = QCoreApplication::applicationFilePath();
    QString nativeAppPath = QDir::toNativeSeparators(appPath);
    std::wstring exePath = nativeAppPath.toStdWString();

    // アイコン: "C:\Path\To\App.exe",0
    std::wstring iconVal = L"\"" + exePath + L"\",0";
    // コマンド: "C:\Path\To\App.exe" "%1"
    std::wstring cmdVal = L"\"" + exePath + L"\" \"%1\"";

    // ★ 修正方針:
    // 複雑な条件分岐（SystemFileAssociationsなど）は環境依存で失敗するため廃止し、
    // 「*」(全ファイル) と 「Directory」(フォルダ) への登録のみを行う「確実な構成」に戻します。
    // メニュー名は、どちらの動作にも違和感のない「QSupportViewerで開く」とします。

    // 登録するキー
    struct RegEntry {
        std::wstring keyPath;
        std::wstring menuText;
    };

    std::vector<RegEntry> entries;
    entries.push_back({L"Software\\Classes\\*\\shell\\QSupportViewer", L"QSupportViewerで開く"});
    entries.push_back({L"Software\\Classes\\Directory\\shell\\QSupportViewer", L"QSupportViewerで開く"});

    // ★ 掃除用リスト: これまでの試行錯誤で作成された可能性のあるキーを全て削除します
    std::vector<std::wstring> keysCleanUp = {
        // 今回使うキーも一旦消す
        L"Software\\Classes\\*\\shell\\QSupportViewer",
        L"Software\\Classes\\Directory\\shell\\QSupportViewer",
        // 以前のキー名
        L"Software\\Classes\\*\\shell\\QSupportViewerPlaylist",
        L"Software\\Classes\\*\\shell\\QSupportViewerOpen",
        L"Software\\Classes\\Directory\\shell\\QSupportViewerPlaylist",
        // 失敗したSystemFileAssociations系
        L"Software\\Classes\\SystemFileAssociations\\image\\shell\\QSupportViewerOpen",
        L"Software\\Classes\\SystemFileAssociations\\audio\\shell\\QSupportViewerPlaylist",
        L"Software\\Classes\\SystemFileAssociations\\video\\shell\\QSupportViewerPlaylist"
    };

    // 拡張子ごとのキー (.jpg 等) も掃除が必要ならループで削除できますが、
    // 数が多いため、主要なものが残っていればここで追加削除してください。

    bool success = false;

    if (enabled) {
        // 1. まず掃除 (競合を防ぐため)
        for (const auto& k : keysCleanUp) {
            DeleteRegistryKey(HKEY_CURRENT_USER, k);
        }

        // 2. 登録実行
        bool ok = true;
        for (const auto& entry : entries) {
            if (!WriteRegistryString(HKEY_CURRENT_USER, entry.keyPath, L"", entry.menuText)) ok = false;
            if (!WriteRegistryString(HKEY_CURRENT_USER, entry.keyPath, L"Icon", iconVal)) ok = false;
            if (!WriteRegistryString(HKEY_CURRENT_USER, entry.keyPath + L"\\command", L"", cmdVal)) ok = false;
        }
        success = ok;
    } else {
        // 削除モード
        for (const auto& k : keysCleanUp) {
            DeleteRegistryKey(HKEY_CURRENT_USER, k);
        }

        // 拡張子ごとの登録を試した際の残骸掃除 (主要な画像/動画のみ)
        QStringList exts = { "jpg", "jpeg", "png", "bmp", "gif", "mp3", "wav", "mp4", "mkv", "avi" };
        for (const QString& ext : exts) {
            std::wstring base = L"Software\\Classes\\." + ext.toStdWString() + L"\\shell\\";
            DeleteRegistryKey(HKEY_CURRENT_USER, base + L"QSupportViewerOpen");
            DeleteRegistryKey(HKEY_CURRENT_USER, base + L"QSupportViewerPlaylist");
        }

        success = true;
    }

    if (success) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
        qDebug() << "Registry updated successfully (Reverted to robust method).";
        return;
    }

    // --- 権限昇格ロジック (変更なし) ---
    if (!QCoreApplication::arguments().contains("--register") &&
        !QCoreApplication::arguments().contains("--unregister")) {
        QString param = enabled ? "--register" : "--unregister";
        ShellExecuteW(NULL, L"runas", (LPCWSTR)nativeAppPath.utf16(), (LPCWSTR)param.utf16(), NULL, SW_SHOWNORMAL);
    }
#else
    Q_UNUSED(enabled);
#endif
}

void SettingsManager::optimizeForFirstLaunch(QSettings &settings)
{
    qDebug() << "First launch detected. Optimizing timer settings...";
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        qreal refreshRate = screen->refreshRate();
        if (refreshRate > 0) {
            int optimalInterval = qRound(1000.0 / refreshRate);
            qDebug() << "Screen refresh rate:" << refreshRate << "Hz, Optimal interval:" << optimalInterval << "ms";
            settings.setValue("progressTimerInterval", optimalInterval);
            settings.setValue("uiUpdateTimerInterval", optimalInterval);
        }
    }
    settings.setValue("firstLaunch", false);
}

