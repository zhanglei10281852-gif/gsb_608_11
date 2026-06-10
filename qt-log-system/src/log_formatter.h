#ifndef LOG_FORMATTER_H
#define LOG_FORMATTER_H

#include <QtGlobal>
#include <QString>

// ============================================================================
// LogFormatter - 日志格式化输出模块
// 提供标准化的日志消息处理器，输出格式: [时间] [级别] [模块名] 消息内容
// 线程安全: 内部使用 QMutex 保护输出操作（stderr + 文件 + 滚动切割共用同一把锁）
//
// 使用方式 (默认仅输出 stderr):
//   #include "log_formatter.h"
//   LogFormatter::install();  // 在 main() 中尽早调用
//
// 启用文件输出 + 按大小滚动 (可选):
//   LogFormatter::enableFileOutput("/var/log/app.log",
//                                  5 * 1024 * 1024 /* 5MB */,
//                                  5              /* 最多保留 5 个历史文件 */);
//
// 关闭文件输出（恢复默认仅 stderr）:
//   LogFormatter::disableFileOutput();
// ============================================================================

class LogFormatter
{
public:
    // 默认滚动阈值与历史文件数（暴露常量便于测试与上层引用）
    static constexpr qint64 kDefaultMaxFileSize   = 5LL * 1024 * 1024; // 5 MB
    static constexpr int    kDefaultMaxBackupFiles = 5;

    // 安装自定义消息处理器
    // 建议在 QCoreApplication 创建后尽早调用
    static void install();

    // 卸载自定义消息处理器，恢复 Qt 默认行为
    // 同时会关闭已开启的文件输出
    static void uninstall();

    // 消息处理器函数（可直接传给 qInstallMessageHandler）
    // 输出格式: [yyyy-MM-dd hh:mm:ss.zzz] [LEVEL   ] [category       ] message
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext &context,
                               const QString &msg);

    // ------------------------------------------------------------------
    // 文件输出 + 按大小滚动
    // ------------------------------------------------------------------

    // 开启文件输出（与 stderr 输出并存）。
    // - filePath:        日志主文件路径，必填；父目录会自动尝试创建。
    // - maxFileSize:     单个文件的最大字节数，达到/超过后触发滚动（>0 才生效）。
    // - maxBackupFiles:  历史归档文件最大保留数量（>=0；0 表示不保留任何历史，
    //                    达阈值时直接清空当前文件继续写）。
    // 返回 true 表示打开文件成功。
    static bool enableFileOutput(const QString &filePath,
                                 qint64 maxFileSize = kDefaultMaxFileSize,
                                 int maxBackupFiles = kDefaultMaxBackupFiles);

    // 关闭文件输出（恢复默认仅 stderr 行为）
    static void disableFileOutput();

    // 查询当前文件输出是否启用
    static bool isFileOutputEnabled();

    // 获取当前主日志文件路径（未启用时返回空字符串）
    static QString currentLogFilePath();
};

#endif // LOG_FORMATTER_H
