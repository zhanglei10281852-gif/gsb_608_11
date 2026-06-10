#include "log_formatter.h"
#include <QDateTime>
#include <QMutex>
#include <cstdio>

// 保护 fprintf 输出的互斥锁，确保多线程环境下日志不会交错
static QMutex s_outputMutex;

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
    qInstallMessageHandler(nullptr);
}

// ============================================================================
// 消息处理器实现
// 输出格式: [时间] [级别] [模块名] 消息内容
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

    // 线程安全输出
    QMutexLocker locker(&s_outputMutex);
    fprintf(stderr, "[%s] [%-8s] [%-15s] %s\n",
            qPrintable(timestamp),
            levelTag,
            category,
            qPrintable(msg));
    fflush(stderr);

    // Fatal 消息触发中止
    if (type == QtFatalMsg) {
        abort();
    }
}
