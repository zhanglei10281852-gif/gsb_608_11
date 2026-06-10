#ifndef LOG_CATEGORIES_H
#define LOG_CATEGORIES_H

#include <QLoggingCategory>

// ============================================================================
// 日志分类声明 - 集中管理所有业务模块的日志分类
// 命名规范: app.<模块名>
// 使用方式: 在业务代码中通过 qCDebug(logNetwork) << "message"; 输出日志
// ============================================================================

// 网络模块日志分类
Q_DECLARE_LOGGING_CATEGORY(logNetwork)

// 数据库模块日志分类
Q_DECLARE_LOGGING_CATEGORY(logDatabase)

// 文件IO模块日志分类
Q_DECLARE_LOGGING_CATEGORY(logFileIO)

// UI模块日志分类
Q_DECLARE_LOGGING_CATEGORY(logUI)

#endif // LOG_CATEGORIES_H
