#include "log_formatter.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QTextStream>
#include <cstdio>

static QMutex s_outputMutex;
static QFile *s_logFile = nullptr;
static LogFormatter::FileConfig s_fileConfig;

// ============================================================================
// 打开日志文件（调用前必须持有 s_outputMutex）
// ============================================================================
bool LogFormatter::openFileLocked()
{
    if (s_logFile && s_logFile->isOpen()) {
        return true;
    }

    if (!s_fileConfig.enabled) {
        return false;
    }

    if (s_fileConfig.filePath.isEmpty()) {
        return false;
    }

    s_logFile = new QFile(s_fileConfig.filePath);
    if (!s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete s_logFile;
        s_logFile = nullptr;
        return false;
    }

    return true;
}

// ============================================================================
// 关闭日志文件（调用前必须持有 s_outputMutex）
// ============================================================================
void LogFormatter::closeFileLocked()
{
    if (s_logFile) {
        if (s_logFile->isOpen()) {
            s_logFile->flush();
            s_logFile->close();
        }
        delete s_logFile;
        s_logFile = nullptr;
    }
}

// ============================================================================
// 执行滚动切割（调用前必须持有 s_outputMutex）
//
// 命名规则:
//   当前日志:      app.log
//   第 1 次归档:   app.log.1  (最新的归档)
//   第 2 次归档:   app.log.2
//   ...
//   第 N 次归档:   app.log.N  (最旧的归档，超过 maxBackups 时删除)
// ============================================================================
void LogFormatter::rotateLogFilesLocked()
{
    closeFileLocked();

    const QString &basePath = s_fileConfig.filePath;
    const int maxBackups = s_fileConfig.maxBackups;

    if (maxBackups <= 0) {
        QFile::remove(basePath);
        return;
    }

    QString oldestPath = QStringLiteral("%1.%2").arg(basePath).arg(maxBackups);
    QFile::remove(oldestPath);

    for (int i = maxBackups - 1; i >= 1; --i) {
        QString src = QStringLiteral("%1.%2").arg(basePath).arg(i);
        QString dst = QStringLiteral("%1.%2").arg(basePath).arg(i + 1);
        if (QFile::exists(src)) {
            QFile::remove(dst);
            QFile::rename(src, dst);
        }
    }

    if (QFile::exists(basePath)) {
        QString dst = QStringLiteral("%1.1").arg(basePath);
        QFile::remove(dst);
        QFile::rename(basePath, dst);
    }
}

// ============================================================================
// 安装自定义消息处理器
// ============================================================================
void LogFormatter::install()
{
    qInstallMessageHandler(LogFormatter::messageHandler);
}

// ============================================================================
// 卸载自定义消息处理器
// ============================================================================
void LogFormatter::uninstall()
{
    QMutexLocker locker(&s_outputMutex);
    closeFileLocked();
    qInstallMessageHandler(nullptr);
}

// ============================================================================
// 设置文件输出配置
// ============================================================================
void LogFormatter::setFileConfig(const FileConfig &config)
{
    QMutexLocker locker(&s_outputMutex);
    closeFileLocked();
    s_fileConfig = config;
    if (s_fileConfig.enabled) {
        if (s_fileConfig.maxFileSize <= 0) {
            s_fileConfig.maxFileSize = 10 * 1024 * 1024;
        }
        if (s_fileConfig.maxBackups < 0) {
            s_fileConfig.maxBackups = 5;
        }
    }
}

// ============================================================================
// 便捷方法: 启用文件输出
// ============================================================================
void LogFormatter::enableFileOutput(const QString &filePath,
                                    qint64 maxFileSize,
                                    int maxBackups)
{
    FileConfig cfg;
    cfg.enabled = true;
    cfg.filePath = filePath;
    cfg.maxFileSize = maxFileSize;
    cfg.maxBackups = maxBackups;
    setFileConfig(cfg);
}

// ============================================================================
// 便捷方法: 禁用文件输出
// ============================================================================
void LogFormatter::disableFileOutput()
{
    FileConfig cfg;
    cfg.enabled = false;
    setFileConfig(cfg);
}

// ============================================================================
// 消息处理器实现
// 输出格式: [时间] [级别] [模块名] 消息内容
// ============================================================================
void LogFormatter::messageHandler(QtMsgType type,
                                  const QMessageLogContext &context,
                                  const QString &msg)
{
    const char *levelTag = nullptr;
    switch (type) {
    case QtDebugMsg:    levelTag = "DEBUG";    break;
    case QtInfoMsg:     levelTag = "INFO";     break;
    case QtWarningMsg:  levelTag = "WARNING";  break;
    case QtCriticalMsg: levelTag = "CRITICAL"; break;
    case QtFatalMsg:    levelTag = "FATAL";    break;
    }

    const char *category = context.category ? context.category : "default";
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    QMutexLocker locker(&s_outputMutex);

    fprintf(stderr, "[%s] [%-8s] [%-15s] %s\n",
            qPrintable(timestamp),
            levelTag,
            category,
            qPrintable(msg));
    fflush(stderr);

    if (s_fileConfig.enabled) {
        bool shouldRotate = false;

        if (openFileLocked()) {
            qint64 currentSize = s_logFile->size();
            if (currentSize >= s_fileConfig.maxFileSize) {
                shouldRotate = true;
            }
        }

        if (shouldRotate) {
            rotateLogFilesLocked();
            openFileLocked();
        }

        if (s_logFile && s_logFile->isOpen()) {
            QString line = QString::asprintf("[%s] [%-8s] [%-15s] %s\n",
                                             qPrintable(timestamp),
                                             levelTag,
                                             category,
                                             qPrintable(msg));
            s_logFile->write(line.toUtf8());
            s_logFile->flush();
        }
    }

    if (type == QtFatalMsg) {
        abort();
    }
}
