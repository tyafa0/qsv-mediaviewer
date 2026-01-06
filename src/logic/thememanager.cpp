#include "thememanager.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSettings>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_currentTheme("light") // デフォルト
{
}

void ThemeManager::loadAllIcons()
{
    const QStringList iconNames = {
        "play_arrow", "pause", "panorama", "panorama_off", "image_inset", "no_image", "image",
        "panorama_horizontal", "panorama_vertical", "slideshow_play", "slideshow_pause",
        "pin", "pin_off", "text_size",
        "stop", "skip_previous", "skip_next", "folder_data", "music_library", "photo_library", "bookshelf",
        "repeat", "repeat_one", "repeat_on",
        "shuffle", "shuffle_on", "random",
        "volume_up", "volume_down", "volume_mute", "volume_off1",
        "arrow_right", "arrow_left", "arrow_downward", "arrow_upward",
        "arrow_back", "arrow_forward", "arrow_upward",
        "arrow_back_false", "arrow_forward_false", "arrow_upward_false"
    };

    qDebug() << "--- Loading all icons ---";
    for (const QString& name : iconNames) {
        QIcon lightIcon(QString(":/icons/light/%1_light.png").arg(name));
        if (!lightIcon.isNull()) {
            m_lightIcons.insert(name, lightIcon);
        } else {
            qDebug() << "Failed to load light icon:" << name;
        }

        QIcon darkIcon(QString(":/icons/dark/%1_dark.png").arg(name));
        if (!darkIcon.isNull()) {
            m_darkIcons.insert(name, darkIcon);
        } else {
            qDebug() << "Failed to load dark icon:" << name;
        }
    }
    qDebug() << "--- Icon loading finished ---";
}

void ThemeManager::applyTheme(const QString &themeName)
{
    // QSSファイルの読み込み
    QString qssPath = QString(":/%1.qss").arg(themeName);
    QFile qssFile(qssPath);

    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&qssFile);
        QString styleSheet = stream.readAll();
        qssFile.close();

        qApp->setStyleSheet(styleSheet);
        qDebug() << "Theme applied:" << themeName;

        m_currentTheme = themeName;

        // 設定の保存もここで行うか、MainWindowで行うか。
        // 責務分離としては「変更通知」だけしてMainWindowが保存するのが綺麗だが、
        // 簡易的にここで保存しても良い。今回はMainWindow側で保存ロジックを残す形にします。

        emit themeChanged(m_currentTheme);

    } else {
        qDebug() << "Error: Could not open theme file:" << qssPath;
    }
}

QString ThemeManager::currentTheme() const
{
    return m_currentTheme;
}

QIcon ThemeManager::getIcon(const QString &name) const
{
    if (m_currentTheme == "dark") {
        return m_darkIcons.value(name, QIcon());
    }
    return m_lightIcons.value(name, QIcon());
}
