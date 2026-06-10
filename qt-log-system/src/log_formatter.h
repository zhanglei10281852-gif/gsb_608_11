#ifndef LOG_FORMATTER_H
#define LOG_FORMATTER_H

#include <QtGlobal>
#include <QString>
#include <QFile>
#include <QMutex>

// ============================================================================
// LogFormatter - 日志格式化输出模块
// 提供标准化的日志消息处理器，输出格式: [时间] [级别] [模块名] 消息内容
// 支持 stderr 和文件双路输出，文件输出支持按大小滚动
// 线程安全: 内部使用 QMutex 保护输出操作
//
// 使用方式:
//   #include "log_formatter.h"
//   LogFormatter::install();  // 在 main() 中尽早调用
//
// 文件输出配置（可选，默认关闭）:
//   LogFormatter::setFileOutputEnabled(true);
//   LogFormatter::setLogFilePath("app.log");
//   LogFormatter::setMaxFileSize(10 * 1024 * 1024);  // 10MB
//   LogFormatter::setMaxBackupFiles(5);
// ============================================================================

class LogFormatter
{
public:
    // 安装自定义消息处理器
    // 建议在 QCoreApplication 创建后尽早调用
    static void install();

    // 卸载自定义消息处理器，恢复 Qt 默认行为
    static void uninstall();

    // 消息处理器函数（可直接传给 qInstallMessageHandler）
    // 输出格式: [yyyy-MM-dd hh:mm:ss.zzz] [LEVEL   ] [category       ] message
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext &context,
                               const QString &msg);

    // ========================================================================
    // 文件输出配置
    // ========================================================================

    // 启用/禁用文件输出（默认禁用）
    static void setFileOutputEnabled(bool enabled);

    // 设置日志文件路径（默认: "app.log"）
    static void setLogFilePath(const QString &filePath);

    // 设置单个日志文件的最大字节数，超过则滚动（默认: 10MB）
    static void setMaxFileSize(qint64 maxSizeBytes);

    // 设置最多保留的历史归档文件数量（默认: 5个）
    static void setMaxBackupFiles(int count);

    // 获取当前配置
    static bool isFileOutputEnabled();
    static QString logFilePath();
    static qint64 maxFileSize();
    static int maxBackupFiles();

private:
    // 格式化单条日志消息
    static QString formatMessage(QtMsgType type,
                                 const QMessageLogContext &context,
                                 const QString &msg);

    // 写入文件（调用前必须已持有 s_outputMutex 锁）
    static void writeToFileLocked(const QString &formattedMessage);

    // 检查并执行滚动（调用前必须已持有 s_outputMutex 锁）
    static void rolloverIfNeededLocked();

    // 执行滚动切割（调用前必须已持有 s_outputMutex 锁）
    static void doRolloverLocked();

    // 关闭当前文件（调用前必须已持有 s_outputMutex 锁）
    static void closeFileLocked();

    // 文件输出开关
    static bool s_fileOutputEnabled;

    // 日志文件路径
    static QString s_logFilePath;

    // 单个文件最大字节数
    static qint64 s_maxFileSize;

    // 最大备份文件数
    static int s_maxBackupFiles;

    // 当前打开的日志文件
    static QFile s_logFile;

    // 当前文件大小追踪
    static qint64 s_currentFileSize;

    // 保护所有输出操作和文件状态的互斥锁
    static QMutex s_outputMutex;
};

#endif // LOG_FORMATTER_H
