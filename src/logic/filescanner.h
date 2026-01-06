#ifndef FILESCANNER_H
#define FILESCANNER_H

#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QDirIterator>
#include <QTimer>

struct ScanSettings {
    bool scanSubdirectories = false;
    int fileScanLimit = 2000;
    int chunkSize = 50;
    QStringList audioVideoExtensions;
    QStringList imageExtensions;
};

class FileScanner : public QObject
{
    Q_OBJECT
public:
    explicit FileScanner(QObject *parent = nullptr);
    ~FileScanner();

public slots:
    // スキャン開始 (ドロップ先情報などもパススルーする)
    void startScan(const QList<QUrl> &urls, int dropTargetIndex, const ScanSettings &settings);

    // 強制停止
    void stopScan();

signals:
    // 状態通知
    void scanStarted();
    void scanFinished(const QStringList &audioVideoFiles, const QStringList &imageFiles, int dropTargetIndex, bool limitReached);

    // 進捗 (必要であればプログレスバー等に使用)
    void scanProgress(int processedCount);

private slots:
    void processChunk();

private:
    // 入力データ
    QList<QUrl> m_scanQueue;
    ScanSettings m_settings;
    int m_dropTargetIndex;

    // 作業用データ
    QDirIterator *m_dirIterator;
    QStringList m_foundAudioVideoFiles;
    QStringList m_foundImageFiles;
    int m_processedTotal;

    // 内部状態
    bool m_isScanning;
};

#endif // FILESCANNER_H
