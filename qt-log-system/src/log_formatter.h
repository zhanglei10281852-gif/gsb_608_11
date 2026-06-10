#ifndef LOG_FORMATTER_H
#define LOG_FORMATTER_H

#include <QtGlobal>
#include <QString>

class QFile;

// ============================================================================
// LogFormatter - 日志格式化输出模块
// 提供标准化的日志消息处理器，输出格式: [时间] [级别] [模块名] 消息内容
// 线程安全: 内部使用 QMutex 保护输出操作（stderr + 文件）
//
// 使用方式:
//   #include "log_formatter.h"
//   LogFormatter::install();  // 在 main() 中尽早调用（默认仅 stderr）
//
//   // 可选：同时输出到文件（启用按大小滚动）
//   LogFormatter::setLogFile("/var/log/app.log");
//   // 或自定义滚动参数:
//   LogFormatter::setLogFile("/var/log/app.log", 10*1024*1024, 10);
//
//   // 关闭文件输出:
//   LogFormatter::disableLogFile();
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

    // 启用文件日志输出（在 stderr 之外同时写入文件）
    //   filePath  - 日志文件路径（如 "/var/log/app.log"）
    //   maxSize   - 单个日志文件最大字节数，超过则滚动（默认 5 MB）
    //   maxFiles  - 历史归档文件最大保留数（默认 5，超出删除最旧的）
    // 多次调用会先关闭之前的文件，再用新参数打开
    static void setLogFile(const QString &filePath,
                           qint64 maxSize = 5 * 1024 * 1024,
                           int maxFiles = 5);

    // 关闭文件日志输出（仅保留 stderr）
    static void disableLogFile();

private:
    // 执行滚动切割（调用前必须已持有 s_outputMutex）
    static void rotateLogFile();
};

#endif // LOG_FORMATTER_H
