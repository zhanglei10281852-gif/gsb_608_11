#include "log_formatter.h"
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <cstdio>

// ============================================================================
// 静态成员初始化
// ============================================================================
bool LogFormatter::s_fileOutputEnabled = false;
QString LogFormatter::s_logFilePath = QStringLiteral("app.log");
qint64 LogFormatter::s_maxFileSize = 10 * 1024 * 1024; // 10MB
int LogFormatter::s_maxBackupFiles = 5;
QFile LogFormatter::s_logFile;
qint64 LogFormatter::s_currentFileSize = 0;
QMutex LogFormatter::s_outputMutex;

// ============================================================================
// 安装自定义消息处理器
// ============================================================================
void LogFormatter::install()
{
    qInstallMessageHandler(LogFormatter::messageHandler);
}

// ============================================================================
// 卸载自定义消息处理器，恢复 Qt 默认行为
// ============================================================================
void LogFormatter::uninstall()
{
    qInstallMessageHandler(nullptr);
    QMutexLocker locker(&s_outputMutex);
    closeFileLocked();
}

// ============================================================================
// 消息处理器实现
// 输出格式: [时间] [级别] [模块名] 消息内容
// 线程安全: 通过 s_outputMutex 保护所有输出操作
// ============================================================================
void LogFormatter::messageHandler(QtMsgType type,
                                  const QMessageLogContext &context,
                                  const QString &msg)
{
    const QString formatted = formatMessage(type, context, msg);
    const QByteArray utf8Line = (formatted + QLatin1Char('\n')).toUtf8();

    QMutexLocker locker(&s_outputMutex);

    // 输出到 stderr（原有行为不变）
    fprintf(stderr, "%s", utf8Line.constData());
    fflush(stderr);

    // 输出到文件（新增功能，可配置开关）
    if (s_fileOutputEnabled) {
        writeToFileLocked(formatted);
    }

    // Fatal 消息触发中止
    if (type == QtFatalMsg) {
        closeFileLocked();
        abort();
    }
}

// ============================================================================
// 启用/禁用文件输出
// ============================================================================
void LogFormatter::setFileOutputEnabled(bool enabled)
{
    QMutexLocker locker(&s_outputMutex);
    if (s_fileOutputEnabled == enabled)
        return;
    s_fileOutputEnabled = enabled;
    if (!enabled) {
        closeFileLocked();
    }
}

// ============================================================================
// 设置日志文件路径
// ============================================================================
void LogFormatter::setLogFilePath(const QString &filePath)
{
    QMutexLocker locker(&s_outputMutex);
    if (s_logFilePath == filePath)
        return;
    closeFileLocked();
    s_logFilePath = filePath;
    s_currentFileSize = 0;
}

// ============================================================================
// 设置单个日志文件的最大字节数
// ============================================================================
void LogFormatter::setMaxFileSize(qint64 maxSizeBytes)
{
    QMutexLocker locker(&s_outputMutex);
    if (maxSizeBytes <= 0)
        return;
    s_maxFileSize = maxSizeBytes;
}

// ============================================================================
// 设置最多保留的历史归档文件数量
// ============================================================================
void LogFormatter::setMaxBackupFiles(int count)
{
    QMutexLocker locker(&s_outputMutex);
    if (count < 0)
        return;
    s_maxBackupFiles = count;
}

// ============================================================================
// 获取当前配置
// ============================================================================
bool LogFormatter::isFileOutputEnabled()
{
    QMutexLocker locker(&s_outputMutex);
    return s_fileOutputEnabled;
}

QString LogFormatter::logFilePath()
{
    QMutexLocker locker(&s_outputMutex);
    return s_logFilePath;
}

qint64 LogFormatter::maxFileSize()
{
    QMutexLocker locker(&s_outputMutex);
    return s_maxFileSize;
}

int LogFormatter::maxBackupFiles()
{
    QMutexLocker locker(&s_outputMutex);
    return s_maxBackupFiles;
}

// ============================================================================
// 格式化单条日志消息
// 输出格式: [yyyy-MM-dd hh:mm:ss.zzz] [LEVEL   ] [category       ] message
// ============================================================================
QString LogFormatter::formatMessage(QtMsgType type,
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

    // 组装格式: [时间] [级别] [模块名] 消息
    // 使用 -8 和 -15 的左对齐宽度，与原始 fprintf 格式保持一致
    const QString levelStr = QString::fromUtf8(levelTag).leftJustified(8, ' ');
    const QString categoryStr = QString::fromUtf8(category).leftJustified(15, ' ');
    return QStringLiteral("[%1] [%2] [%3] %4")
        .arg(timestamp,
             levelStr,
             categoryStr,
             msg);
}

// ============================================================================
// 写入文件（调用前必须已持有 s_outputMutex 锁）
// ============================================================================
void LogFormatter::writeToFileLocked(const QString &formattedMessage)
{
    const QByteArray utf8Line = (formattedMessage + QLatin1Char('\n')).toUtf8();

    // 检查是否需要滚动
    if (s_currentFileSize + utf8Line.size() > s_maxFileSize) {
        rolloverIfNeededLocked();
    }

    // 如果文件未打开，尝试打开
    if (!s_logFile.isOpen()) {
        s_logFile.setFileName(s_logFilePath);

        // 确保目录存在
        QFileInfo fileInfo(s_logFilePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 以追加模式打开
        if (!s_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return; // 打开失败，静默忽略
        }

        // 记录当前文件大小
        s_currentFileSize = s_logFile.size();

        // 如果现有文件已经超过大小限制，先滚动
        if (s_currentFileSize >= s_maxFileSize) {
            rolloverIfNeededLocked();
            if (!s_logFile.isOpen()) {
                if (!s_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                    return;
                }
                s_currentFileSize = 0;
            }
        }
    }

    // 写入文件
    const qint64 bytesWritten = s_logFile.write(utf8Line);
    if (bytesWritten > 0) {
        s_currentFileSize += bytesWritten;
    }
    s_logFile.flush();
}

// ============================================================================
// 检查并执行滚动（调用前必须已持有 s_outputMutex 锁）
// ============================================================================
void LogFormatter::rolloverIfNeededLocked()
{
    if (s_maxBackupFiles <= 0 && s_maxFileSize <= 0)
        return;

    if (s_currentFileSize < s_maxFileSize)
        return;

    doRolloverLocked();
}

// ============================================================================
// 执行滚动切割（调用前必须已持有 s_outputMutex 锁）
// 归档命名规则:
//   当前日志: app.log
//   第1个归档: app.log.1
//   第2个归档: app.log.2
//   ...
//   第N个归档: app.log.N
// 滚动时:
//   app.log.N-1 -> app.log.N (如果已存在则删除最旧的 app.log.N)
//   ...
//   app.log.1 -> app.log.2
//   app.log -> app.log.1
//   然后创建新的 app.log
// ============================================================================
void LogFormatter::doRolloverLocked()
{
    // 先关闭当前文件
    closeFileLocked();

    QFileInfo baseInfo(s_logFilePath);
    QString basePath = baseInfo.absoluteFilePath();
    QDir dir = baseInfo.absoluteDir();

    // 如果 maxBackupFiles 为 0，只清空当前文件即可
    if (s_maxBackupFiles <= 0) {
        QFile::remove(basePath);
        return;
    }

    // 删除最旧的归档文件（第 N 号）
    const QString oldestBackup = basePath + QLatin1Char('.') + QString::number(s_maxBackupFiles);
    if (QFile::exists(oldestBackup)) {
        QFile::remove(oldestBackup);
    }

    // 从 N-1 到 1，依次重命名：.k -> .(k+1)
    for (int i = s_maxBackupFiles - 1; i >= 1; --i) {
        const QString src = basePath + QLatin1Char('.') + QString::number(i);
        const QString dst = basePath + QLatin1Char('.') + QString::number(i + 1);
        if (QFile::exists(src)) {
            if (QFile::exists(dst)) {
                QFile::remove(dst);
            }
            QFile::rename(src, dst);
        }
    }

    // 当前日志文件重命名为 .1
    const QString firstBackup = basePath + QLatin1String(".1");
    if (QFile::exists(basePath)) {
        if (QFile::exists(firstBackup)) {
            QFile::remove(firstBackup);
        }
        QFile::rename(basePath, firstBackup);
    }

    // 重置当前文件大小
    s_currentFileSize = 0;
}

// ============================================================================
// 关闭当前文件（调用前必须已持有 s_outputMutex 锁）
// ============================================================================
void LogFormatter::closeFileLocked()
{
    if (s_logFile.isOpen()) {
        s_logFile.close();
    }
    s_currentFileSize = 0;
}
