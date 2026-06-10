#include "log_formatter.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <cstdio>

// ============================================================================
// 文件日志静态状态（全部受 s_outputMutex 保护）
// ============================================================================
static QMutex s_outputMutex;
static QFile *s_logFile = nullptr;
static QString s_logFilePath;
static qint64  s_maxFileSize = 5 * 1024 * 1024;
static int     s_maxFiles    = 5;
static bool    s_fileEnabled = false;

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
    if (s_logFile) {
        s_logFile->flush();
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }
    s_fileEnabled = false;
    locker.unlock();
    qInstallMessageHandler(nullptr);
}

// ============================================================================
// 启用文件日志输出
// ============================================================================
void LogFormatter::setLogFile(const QString &filePath, qint64 maxSize, int maxFiles)
{
    QMutexLocker locker(&s_outputMutex);

    // 关闭之前的文件
    if (s_logFile) {
        s_logFile->flush();
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }

    s_logFilePath = filePath;
    s_maxFileSize = (maxSize > 0) ? maxSize : (5 * 1024 * 1024);
    s_maxFiles    = (maxFiles > 0) ? maxFiles : 5;

    // 确保目录存在
    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    // 以追加模式打开：若文件已存在则续写，方便重启后继续写
    s_logFile = new QFile(filePath);
    if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        s_fileEnabled = true;
    } else {
        // 打开失败则不启用文件输出（不抛出，不崩溃，仅静默降级为 stderr）
        delete s_logFile;
        s_logFile = nullptr;
        s_fileEnabled = false;
    }
}

// ============================================================================
// 关闭文件日志输出
// ============================================================================
void LogFormatter::disableLogFile()
{
    QMutexLocker locker(&s_outputMutex);
    if (s_logFile) {
        s_logFile->flush();
        s_logFile->close();
        delete s_logFile;
        s_logFile = nullptr;
    }
    s_fileEnabled = false;
}

// ============================================================================
// 滚动切割
// 调用前必须已持有 s_outputMutex
//
// 命名规则:
//   当前活动文件 :  <filePath>           （如 app.log）
//   最新归档     :  <filePath>.1         （上一个刚写满的文件）
//   次新归档     :  <filePath>.2
//   ...
//   最旧归档     :  <filePath>.<maxFiles>
//
// 滚动流程:
//   1. 关闭当前文件
//   2. 若 <filePath>.<maxFiles> 存在则删除（淘汰最旧）
//   3. 从 <maxFiles-1> 到 1 依次重命名: <filePath>.i -> <filePath>.(i+1)
//   4. 将当前活动文件重命名为 <filePath>.1
//   5. 打开新的空文件作为活动文件
// ============================================================================
void LogFormatter::rotateLogFile()
{
    if (!s_logFile || s_logFilePath.isEmpty())
        return;

    s_logFile->flush();
    s_logFile->close();

    // 淘汰最旧归档
    QString oldest = s_logFilePath + QStringLiteral(".%1").arg(s_maxFiles);
    if (QFile::exists(oldest)) {
        QFile::remove(oldest);
    }

    // 依次后移: i -> i+1（从大到小遍历）
    for (int i = s_maxFiles - 1; i >= 1; --i) {
        QString src = s_logFilePath + QStringLiteral(".%1").arg(i);
        QString dst = s_logFilePath + QStringLiteral(".%1").arg(i + 1);
        if (QFile::exists(src)) {
            QFile::remove(dst);
            QFile::rename(src, dst);
        }
    }

    // 当前文件 -> .1
    QString firstArchive = s_logFilePath + QStringLiteral(".1");
    QFile::remove(firstArchive);
    QFile::rename(s_logFilePath, firstArchive);

    // 重新打开空文件
    s_logFile->setFileName(s_logFilePath);
    s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

// ============================================================================
// 消息处理器实现
// 输出格式: [时间] [级别] [模块名] 消息内容
// 线程安全: QMutexLocker 保护 fprintf 和 QFile 写入
// ============================================================================
void LogFormatter::messageHandler(QtMsgType type,
                                  const QMessageLogContext &context,
                                  const QString &msg)
{
    // 级别标签
    const char *levelTag = nullptr;
    switch (type) {
    case QtDebugMsg:    levelTag = "DEBUG";    break;
    case QtInfoMsg:     levelTag = "INFO";     break;
    case QtWarningMsg:  levelTag = "WARNING";  break;
    case QtCriticalMsg: levelTag = "CRITICAL"; break;
    case QtFatalMsg:    levelTag = "FATAL";    break;
    }

    // 模块名（来自 QLoggingCategory）
    const char *category = context.category ? context.category : "default";

    // 时间戳
    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    // 线程安全输出（stderr + 文件）
    QMutexLocker locker(&s_outputMutex);

    // stderr 输出（保持原有行为完全不变）
    fprintf(stderr, "[%s] [%-8s] [%-15s] %s\n",
            qPrintable(timestamp),
            levelTag,
            category,
            qPrintable(msg));
    fflush(stderr);

    // 文件输出（格式与 stderr 完全一致）
    if (s_fileEnabled && s_logFile && s_logFile->isOpen()) {
        const QString levelStr = QString::fromLatin1(levelTag);
        const QString categoryStr = QString::fromUtf8(category);
        const QString line = QStringLiteral("[%1] [%2] [%3] %4\n")
                                 .arg(timestamp)
                                 .arg(levelStr, -8)
                                 .arg(categoryStr, -15)
                                 .arg(msg);
        s_logFile->write(line.toUtf8());
        s_logFile->flush();

        // 超阈值则滚动
        if (s_logFile->size() > s_maxFileSize) {
            rotateLogFile();
        }
    }

    locker.unlock();

    // Fatal 消息触发中止
    if (type == QtFatalMsg) {
        abort();
    }
}
