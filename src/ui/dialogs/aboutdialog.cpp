#include "aboutdialog.h"
#include "settingsmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QApplication>
#include <QDate>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("このアプリについて"));
    resize(500, 400); // 適切なサイズに設定
}

AboutDialog::~AboutDialog()
{
}

void AboutDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // タブウィジェットの作成
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createGeneralTab(), tr("一般"));
    m_tabWidget->addTab(createLicenseTab(), tr("ライセンス"));

    mainLayout->addWidget(m_tabWidget);

    // 下部のボタンエリア
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    // Qtについてボタン（Qtの標準ダイアログを呼び出す）
    QPushButton *aboutQtButton = new QPushButton(tr("About Qt"), this);
    connect(aboutQtButton, &QPushButton::clicked, qApp, &QApplication::aboutQt);
    buttonLayout->addWidget(aboutQtButton);

    // 閉じるボタン
    QPushButton *closeButton = new QPushButton(tr("閉じる"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    closeButton->setDefault(true);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);
}

QWidget* AboutDialog::createGeneralTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setAlignment(Qt::AlignCenter);

    // アプリアイコン（リソースにあれば設定してください）
    // QLabel *iconLabel = new QLabel(this);
    // iconLabel->setPixmap(QPixmap(":/icons/app_icon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    // layout->addWidget(iconLabel, 0, Qt::AlignCenter);

    // アプリ名とバージョン
    QLabel *titleLabel = new QLabel(QString("<h2>%1</h2>").arg(QApplication::applicationDisplayName()), this);
    titleLabel->setTextFormat(Qt::RichText);
    layout->addWidget(titleLabel, 0, Qt::AlignCenter);

    QLabel *versionLabel = new QLabel(QString("Version %1").arg(QApplication::applicationVersion()), this);
    layout->addWidget(versionLabel, 0, Qt::AlignCenter);

    layout->addSpacing(20);

    // ★ GitHubリンクの追加
    QString githubId = QApplication::organizationName();
    QString githubUrl = QString("https://github.com/%1").arg(githubId);

    QString style = getLinkColorStyle(this);
    QString linkHtml = QString(
                           "<html><head><style>%1</style></head><body>"
                           "<a href='%2'>%3</a>"
                           "</body></html>"
                           ).arg(style, githubUrl, githubUrl);

    QLabel *urlLabel = new QLabel(this);
    urlLabel->setText(linkHtml);
    urlLabel->setTextFormat(Qt::RichText); // RichTextであることを明示
    urlLabel->setOpenExternalLinks(true);
    layout->addWidget(urlLabel, 0, Qt::AlignCenter);

    layout->addSpacing(20);

    // コピーライト
    QString currentYear = QString::number(QDate::currentDate().year());
    QLabel *copyrightLabel = new QLabel(QString("Copyright © %1 %2. All rights reserved.")
                                            .arg(currentYear)
                                            .arg(QApplication::organizationName()), this);
    layout->addWidget(copyrightLabel, 0, Qt::AlignCenter);

    layout->addStretch();
    return tab;
}

QWidget* AboutDialog::createLicenseTab()
{
    QWidget *tab = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(tab);

    QTextBrowser *licenseBrowser = new QTextBrowser(this);
    licenseBrowser->setOpenExternalLinks(true);

    // --- 修正: ライセンス表示のリンク色設定 ---
    QString style = getLinkColorStyle(this);

    // HTMLヘッダーにスタイルを埋め込む
    QString html = QString(R"(
        <html>
        <head>
            <style>
                %1
            </style>
        </head>
        <body>
            <h3>Third-Party Libraries</h3>
            <p>SupportViewer uses the following third-party libraries:</p>

            <ul>
                <li>
                    <b>Qt Toolkit</b> (LGPL v3)<br>
                    Copyright (C) The Qt Company Ltd.<br>
                    <a href="https://www.qt.io/">https://www.qt.io/</a>
                </li>
                <li>
                    <b>SDL 2.0</b> (zlib License)<br>
                    Copyright (C) Sam Lantinga<br>
                    <a href="https://www.libsdl.org/">https://www.libsdl.org/</a>
                </li>
                <li>
                    <b>libmpv</b> (LGPL v2.1+)<br>
                    Copyright (C) the mpv developers<br>
                    <a href="https://mpv.io/">https://mpv.io/</a>
                </li>
                <li>
                    <b>libopus, libogg, libvorbis</b> (BSD-like Licenses)<br>
                    Multimedia codecs used by the libraries above.
                </li>
            </ul>

            <hr>

            <h3>License Details</h3>
            <p>This software strictly complies with the license terms of the libraries used.</p>

            <h4>LGPL v3 / v2.1+ (Qt, libmpv)</h4>
            <p>These libraries are free software; you can redistribute them and/or modify them under the terms of the GNU Lesser General Public License.</p>

            <h4>zlib License (SDL 2.0)</h4>
            <p>This software is provided 'as-is', without any express or implied warranty.</p>

            <h4>BSD-like Licenses (Codecs)</h4>
            <p>Redistribution and use in source and binary forms, with or without modification, are permitted provided that the conditions are met.</p>
        </body>
        </html>
    )").arg(style);

    licenseBrowser->setHtml(html);
    layout->addWidget(licenseBrowser);

    return tab;
}

QString getLinkColorStyle(const QWidget* widget) {
    // ウィンドウ背景の明るさを取得 (0-255)
    int brightness = widget->palette().color(QPalette::Window).value();

    // 明るさが128未満ならダークモードとみなす
    bool isDarkMode = brightness < 128;

    // ダークモードなら「明るいライムグリーン」、ライトモードなら「濃い緑」
    QString colorCode = isDarkMode ? "#b3ffb3" : "#008000";

    // CSSスタイルを返す
    return QString("a { color: %1; text-decoration: underline; }").arg(colorCode);
}
