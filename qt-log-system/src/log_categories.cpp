#include "log_categories.h"

// ============================================================================
// 日志分类定义
// Q_LOGGING_CATEGORY(变量名, 分类字符串, 默认最低级别)
// 第三个参数为该分类的默认最低输出级别，低于此级别的日志将被过滤
// ============================================================================

// 网络模块 - 默认输出 Debug 及以上所有级别
Q_LOGGING_CATEGORY(logNetwork,  "app.network",  QtDebugMsg)

// 数据库模块 - 默认输出 Debug 及以上所有级别
Q_LOGGING_CATEGORY(logDatabase, "app.database", QtDebugMsg)

// 文件IO模块 - 默认输出 Debug 及以上所有级别
Q_LOGGING_CATEGORY(logFileIO,   "app.fileio",   QtDebugMsg)

// UI模块 - 默认输出 Debug 及以上所有级别
Q_LOGGING_CATEGORY(logUI,       "app.ui",       QtDebugMsg)
