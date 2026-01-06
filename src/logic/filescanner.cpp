#include "filescanner.h"
#include <QFileInfo>
#include <QDir>

FileScanner::FileScanner(QObject *parent)
    : QObject(parent)
    , m_dropTargetIndex(-1)
    , m_dirIterator(nullptr)
    , m_processedTotal(0)
    , m_isScanning(false)
{
}

FileScanner::~FileScanner()
{
    stopScan();
}

void FileScanner::startScan(const QList<QUrl> &urls, int dropTargetIndex, const ScanSettings &settings)
{
    // 実行中なら停止
    stopScan();

    m_scanQueue = urls;
    m_dropTargetIndex = dropTargetIndex;
    m_settings = settings;

    m_foundAudioVideoFiles.clear();
    m_foundImageFiles.clear();
    m_processedTotal = 0;
    m_isScanning = true;

    emit scanStarted();

    // 処理開始
    QTimer::singleShot(0, this, &FileScanner::processChunk);
}

void FileScanner::stopScan()
{
    m_isScanning = false;
    m_scanQueue.clear();
    if (m_dirIterator) {
        delete m_dirIterator;
        m_dirIterator = nullptr;
    }
}

void FileScanner::processChunk()
{
    if (!m_isScanning) return;

    int processedInThisChunk = 0;
    bool limitReached = false;

    // 拡張子リストの結合（判定高速化のため）
    QStringList allExtensions = m_settings.audioVideoExtensions + m_settings.imageExtensions;

    while (processedInThisChunk < m_settings.chunkSize) {
        // --- 上限チェック ---
        if (m_foundAudioVideoFiles.size() + m_foundImageFiles.size() >= m_settings.fileScanLimit) {
            limitReached = true;
            stopScan(); // イテレータ破棄
            break;
        }

        // --- イテレータ処理 (フォルダの中身を走査中) ---
        if (m_dirIterator && m_dirIterator->hasNext()) {
            QString filePath = m_dirIterator->next();
            QString suffix = QFileInfo(filePath).suffix().toLower();

            if (allExtensions.contains(suffix)) {
                if (m_settings.imageExtensions.contains(suffix)) {
                    m_foundImageFiles.append(filePath);
                } else {
                    m_foundAudioVideoFiles.append(filePath);
                }
            }
            processedInThisChunk++;
            m_processedTotal++;
            continue;
        }

        // イテレータ終了 -> 次のキューへ
        if (m_dirIterator) {
            delete m_dirIterator;
            m_dirIterator = nullptr;
        }

        if (m_scanQueue.isEmpty()) {
            break; // 全て完了
        }

        // --- 次のキューを取り出し ---
        QUrl url = m_scanQueue.takeFirst();
        QString path = url.toLocalFile();
        QFileInfo fileInfo(path);

        if (fileInfo.isDir()) {
            QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot;
            QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
            if (m_settings.scanSubdirectories) {
                flags = QDirIterator::Subdirectories;
            }
            m_dirIterator = new QDirIterator(path, filters, flags);
        } else if (fileInfo.isFile()) {
            QString suffix = fileInfo.suffix().toLower();
            if (allExtensions.contains(suffix)) {
                if (m_settings.imageExtensions.contains(suffix)) {
                    m_foundImageFiles.append(path);
                } else {
                    m_foundAudioVideoFiles.append(path);
                }
            }
        }
        processedInThisChunk++;
        m_processedTotal++;
    }

    emit scanProgress(m_processedTotal);

    // --- 継続判定 ---
    if (limitReached || (m_scanQueue.isEmpty() && !m_dirIterator)) {
        // 完了
        m_isScanning = false;
        emit scanFinished(m_foundAudioVideoFiles, m_foundImageFiles, m_dropTargetIndex, limitReached);
    } else {
        // 次のチャンクへ
        QTimer::singleShot(0, this, &FileScanner::processChunk);
    }
}
