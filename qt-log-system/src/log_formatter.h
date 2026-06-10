#ifndef LOG_FORMATTER_H
#define LOG_FORMATTER_H

#include <QtGlobal>
#include <QString>

// ============================================================================
// LogFormatter - 日志格式化输出模块
// 提供标准化的日志消息处理器，输出格式: [时间] [级别] [模块名] 消息内容
// 线程安全: 内部使用 QMutex 保护输出操作
//
// 使用方式:
//   #include "log_formatter.h"
//   LogFormatter::install();  // 在 main() 中尽早调用
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
};

#endif // LOG_FORMATTER_H
