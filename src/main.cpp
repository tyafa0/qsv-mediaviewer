#include <SDL.h>
#include "mainwindow.h"
#include "settingsmanager.h"
#include <QApplication>
#include <QLocalSocket>
#include <QLocalServer>
#include <QFileInfo>
#include <QTextStream>
#include <QStyleFactory>
#include <QMessageBox>
#include <QTimer> // 追加

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // アプリケーション情報
    app.setApplicationName("QSupportViewer");
    app.setApplicationDisplayName("QSupportViewer");
    app.setApplicationVersion("0.6.0");
    app.setOrganizationName(SettingsManager::author);
    app.setOrganizationDomain("github.com");

    QStringList args = app.arguments();

    // --- レジストリ登録・解除モード ---
    if (args.contains("--register") || args.contains("--unregister")) {
        SettingsManager settingsManager;
        bool registerMode = args.contains("--register");
        settingsManager.updateRegistrySettings(registerMode);
        return 0;
    }

    qDebug() << QStyleFactory::keys();
    app.setStyle(QStyleFactory::create("windowsvista"));

    const QString serverName = "QSupportViewer_LocalServer";
    QLocalSocket socket;
    socket.connectToServer(serverName);

    // --- 2重起動チェック (クライアント側) ---
    if (socket.waitForConnected(500)) {
        QTextStream stream(&socket);
// エンコーディングをUTF-8に明示
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif

        // 引数を送信 (最初の1つはexeパスなのでスキップ)
        for (int i = 1; i < args.size(); ++i) {
            // 引数がオプション系でない場合のみ送信
            if (!args[i].startsWith("--")) {
                stream << args[i] << Qt::endl;
            }
        }
        stream.flush();
        // データが確実に書き込まれるまで待つ
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0; // 送信して終了
    }
    else {
        // --- 最初のインスタンス (サーバー側) ---
        QLocalServer server;
        // 以前のクラッシュ等で残っている古いサーバーファイルを削除
        QLocalServer::removeServer(serverName);
        server.listen(serverName);

        MainWindow w;

        // 他のインスタンスから接続があった時の処理
        QObject::connect(&server, &QLocalServer::newConnection, [&]() {
            QLocalSocket* clientSocket = server.nextPendingConnection();

            QObject::connect(clientSocket, &QLocalSocket::readyRead, [&, clientSocket]() {
                QTextStream stream(clientSocket);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                stream.setEncoding(QStringConverter::Utf8);
#else
                stream.setCodec("UTF-8");
#endif

                QStringList files;
                while (!stream.atEnd()) {
                    QString line = stream.readLine().trimmed();
                    if (!line.isEmpty()) {
                        files.append(line);
                    }
                }

                if (!files.isEmpty()) {
                    // GUIスレッドで安全に実行するためにQTimer::singleShotを使用
                    QTimer::singleShot(0, [&w, files](){
                        w.addFilesFromExplorer(files, false);
                    });
                }
            });

            // 切断時のメモリ解放
            QObject::connect(clientSocket, &QLocalSocket::disconnected,
                             clientSocket, &QLocalSocket::deleteLater);
        });

        // 自分自身の起動引数処理
        QStringList files;
        QString debugMsg = "起動引数:\n"; // デバッグ用

        debugMsg += "Exe Path: " + app.applicationFilePath() + "\n\n";
        debugMsg += "Args:\n";

        for(int i = 1; i < args.size(); ++i) {
            if(!args[i].startsWith("--")) {
                files.append(args[i]);
                debugMsg += args[i] + "\n";
            }
        }

        if (!files.isEmpty()) {
            // QMessageBox::information(nullptr, "Debug Args", debugMsg); // 確認用 (後で削除)
            w.addFilesFromExplorer(files, true);
        }

        w.show();
        return app.exec();
    }
}
