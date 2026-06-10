#ifndef LOG_FORMATTER_H
#define LOG_FORMATTER_H

#include <QtGlobal>
#include <QString>

class QFile;

// ============================================================================
// LogFormatter - 日志格式化输出模块
// 提供标准化的日志消息处理器，输出格式: [时间] [级别] [模块名] 消息内容
// 线程安全: 内部使用 QMutex 保护输出操作
//
// 使用方式:
//   #include "log_formatter.h"
//   LogFormatter::install();  // 在 main() 中尽早调用（默认仅输出到 stderr）
//
//   // 可选: 启用文件输出 + 大小滚动
//   LogFormatter::FileConfig cfg;
//   cfg.enabled      = true;
//   cfg.filePath     = "./app.log";
//   cfg.maxFileSize  = 10 * 1024 * 1024;  // 10 MB
//   cfg.maxBackups   = 5;                  // 保留 5 个历史文件
//   LogFormatter::setFileConfig(cfg);
// ============================================================================

class LogFormatter
{
public:
    // 文件输出配置
    struct FileConfig {
        bool    enabled     = false;            // 是否启用文件输出，默认关闭
        QString filePath    = QStringLiteral("app.log");  // 日志文件路径
        qint64  maxFileSize = 10 * 1024 * 1024; // 单文件最大字节数，默认 10MB
        int     maxBackups  = 5;                 // 最多保留历史文件数，默认 5
    };

    // 安装自定义消息处理器
    // 建议在 QCoreApplication 创建后尽早调用
    static void install();

    // 卸载自定义消息处理器，恢复 Qt 默认行为
    static void uninstall();

    // 设置文件输出配置（可在 install() 前后调用，线程安全）
    // 若 config.enabled == true，则日志同时输出到 stderr 和指定文件
    // 若 config.enabled == false，则仅输出到 stderr（默认行为）
    static void setFileConfig(const FileConfig &config);

    // 便捷方法: 启用文件输出
    static void enableFileOutput(const QString &filePath,
                                 qint64 maxFileSize = 10 * 1024 * 1024,
                                 int maxBackups = 5);

    // 便捷方法: 禁用文件输出
    static void disableFileOutput();

    // 消息处理器函数（可直接传给 qInstallMessageHandler）
    // 输出格式: [yyyy-MM-dd hh:mm:ss.zzz] [LEVEL   ] [category       ] message
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext &context,
                               const QString &msg);

private:
    // 打开日志文件（调用前必须持有 s_outputMutex）
    static bool openFileLocked();

    // 执行滚动切割（调用前必须持有 s_outputMutex）
    static void rotateLogFilesLocked();

    // 关闭并刷新日志文件（调用前必须持有 s_outputMutex）
    static void closeFileLocked();
};

#endif // LOG_FORMATTER_H
