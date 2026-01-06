#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QMap>
#include <QIcon>
#include <QString>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(QObject *parent = nullptr);

    // 初期化
    void loadAllIcons();

    // 操作
    void applyTheme(const QString &themeName);
    QString currentTheme() const;

    // アイコン取得
    QIcon getIcon(const QString &name) const;

signals:
    void themeChanged(const QString &themeName);

private:
    QString m_currentTheme;
    QMap<QString, QIcon> m_lightIcons;
    QMap<QString, QIcon> m_darkIcons;
};

#endif // THEMEMANAGER_H
