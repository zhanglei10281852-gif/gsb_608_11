#include "log_formatter.h"
#include <QDateTime>
#include <QMutex>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <cstdio>
#include <memory>

// ============================================================================
// 内部状态
// ----------------------------------------------------------------------------
// 同一把锁同时保护 stderr 输出、文件写入、滚动切割三个操作，
// 确保多线程下不会出现 1) 行内容交错、2) 滚动瞬间窗口期写丢/写错文件 的问题。
// ============================================================================
static QMutex                   s_outputMutex;
static std::unique_ptr<QFile>   s_logFile;       // 当前打开的日志文件（nullptr 表示未启用）
static QString                  s_logFilePath;   // 当前日志文件绝对/原始路径
static qint64                   s_maxFileSize    = LogFormatter::kDefaultMaxFileSize;
static int                      s_maxBackupFiles = LogFormatter::kDefaultMaxBackupFiles;
static qint64                   s_currentSize    = 0; // 当前文件已写入字节数（与文件大小一致）

// ============================================================================
// 内部辅助函数（要求调用方已持有 s_outputMutex）
// ============================================================================

// 关闭并清理当前日志文件（不删文件，仅释放句柄/重置计数）
static void closeCurrentLogFile_locked()
{
    if (s_logFile) {
        s_logFile->flush();
        s_logFile->close();
        s_logFile.reset();
    }
    s_currentSize = 0;
}

// 打开（或重新打开）日志文件以追加方式写入；成功返回 true。
static bool openLogFile_locked(const QString &path)
{
    closeCurrentLogFile_locked();

    QFileInfo fi(path);
    QDir parent = fi.absoluteDir();
    if (!parent.exists()) {
        parent.mkpath(QStringLiteral("."));
    }

    auto file = std::unique_ptr<QFile>(new QFile(path));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    s_logFile     = std::move(file);
    s_currentSize = s_logFile->size();
    return true;
}

// 执行滚动切割：把当前主文件归档为带时间戳的备份文件，并保证历史归档不超过上限。
//
// 归档命名规则:
//   <主文件名>.<yyyyMMdd-hhmmsszzz>(.<seq>).bak
//   - 时间戳为切割发生的本地时间，毫秒精度，保证可读且通常天然递增
//   - 同一毫秒内若已存在同名归档（极少数并发/快速滚动场景），自动追加 .1/.2/...
//   - 后缀使用 ".bak"，便于通过 *.bak 通配筛选历史文件
//
// 清理规则:
//   切割完成后，按文件 mtime（修改时间）升序对所有匹配 "<主文件名>.*.bak"
//   的兄弟文件排序，删除最旧的若干个，直到剩余数量 <= maxBackupFiles。
//   特殊：maxBackupFiles == 0 时不保留任何历史，刚归档的文件也会被删除（等价于清空）。
static void rotateLogFile_locked()
{
    if (s_logFilePath.isEmpty()) {
        return;
    }

    // 1. 关闭当前文件
    closeCurrentLogFile_locked();

    QFileInfo mainInfo(s_logFilePath);
    QDir      dir       = mainInfo.absoluteDir();
    QString   baseName  = mainInfo.fileName();

    // 2. 选定不重名的归档路径
    if (QFile::exists(s_logFilePath)) {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmsszzz"));
        QString archivePath = dir.filePath(baseName + QStringLiteral(".") + stamp + QStringLiteral(".bak"));
        int seq = 1;
        while (QFile::exists(archivePath)) {
            archivePath = dir.filePath(baseName + QStringLiteral(".") + stamp
                                       + QStringLiteral(".") + QString::number(seq)
                                       + QStringLiteral(".bak"));
            ++seq;
        }
        // 重命名失败时（如目标位置异常），退化为直接删除主文件，至少保证后续写入不超阈值
        if (!QFile::rename(s_logFilePath, archivePath)) {
            QFile::remove(s_logFilePath);
        }
    }

    // 3. 清理多余的历史归档：枚举 baseName.*.bak，按 mtime 排序，删除最旧的
    const QStringList nameFilters{ baseName + QStringLiteral(".*.bak") };
    QFileInfoList backups = dir.entryInfoList(nameFilters,
                                              QDir::Files | QDir::NoDotAndDotDot,
                                              QDir::Time | QDir::Reversed); // 旧 -> 新
    int allowed = s_maxBackupFiles < 0 ? 0 : s_maxBackupFiles;
    while (backups.size() > allowed) {
        const QFileInfo oldest = backups.takeFirst();
        QFile::remove(oldest.absoluteFilePath());
    }

    // 4. 重新打开主文件供后续写入
    openLogFile_locked(s_logFilePath);
}

// 把一行已格式化的文本写入文件，并按需触发滚动
static void writeToFile_locked(const QByteArray &line)
{
    if (!s_logFile) return;

    s_logFile->write(line);
    s_logFile->flush();
    s_currentSize += line.size();

    if (s_maxFileSize > 0 && s_currentSize >= s_maxFileSize) {
        rotateLogFile_locked();
    }
}

// ============================================================================
// 安装/卸载
// ============================================================================
void LogFormatter::install()
{
    qInstallMessageHandler(LogFormatter::messageHandler);
}

void LogFormatter::uninstall()
{
    qInstallMessageHandler(nullptr);
    QMutexLocker locker(&s_outputMutex);
    closeCurrentLogFile_locked();
    s_logFilePath.clear();
}

// ============================================================================
// 消息处理器实现
// 输出格式: [时间] [级别] [模块名] 消息内容
// stderr 输出与文件输出共用同一行文本（保证内容完全一致）。
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

    // 1) stderr 输出：与历史行为完全一致（格式、fprintf+fflush）
    fprintf(stderr, "[%s] [%-8s] [%-15s] %s\n",
            qPrintable(timestamp),
            levelTag,
            category,
            qPrintable(msg));
    fflush(stderr);

    // 2) 文件输出（可选）：使用 QString::asprintf 生成与 stderr 格式完全一致的一行
    if (s_logFile) {
        const QString formatted = QString::asprintf("[%s] [%-8s] [%-15s] %s\n",
                                                    qPrintable(timestamp),
                                                    levelTag,
                                                    category,
                                                    qPrintable(msg));
        writeToFile_locked(formatted.toUtf8());
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

// ============================================================================
// 文件输出 + 滚动 公共接口
// ============================================================================
bool LogFormatter::enableFileOutput(const QString &filePath, qint64 maxFileSize, int maxBackupFiles)
{
    if (filePath.isEmpty()) return false;

    QMutexLocker locker(&s_outputMutex);
    s_logFilePath    = filePath;
    s_maxFileSize    = maxFileSize;
    s_maxBackupFiles = maxBackupFiles < 0 ? 0 : maxBackupFiles;

    if (!openLogFile_locked(filePath)) {
        s_logFilePath.clear();
        return false;
    }
    return true;
}

void LogFormatter::disableFileOutput()
{
    QMutexLocker locker(&s_outputMutex);
    closeCurrentLogFile_locked();
    s_logFilePath.clear();
}

bool LogFormatter::isFileOutputEnabled()
{
    QMutexLocker locker(&s_outputMutex);
    return static_cast<bool>(s_logFile);
}

QString LogFormatter::currentLogFilePath()
{
    QMutexLocker locker(&s_outputMutex);
    return s_logFilePath;
}
