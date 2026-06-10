#include "log_helper.h"
#include <QLoggingCategory>

// 静态成员初始化
QMap<QString, LogHelper::Level> LogHelper::s_moduleLevels;
QMutex LogHelper::s_mutex;

// ============================================================================
// 设置单个模块的最低日志级别
// 线程安全: QMutexLocker 保护 s_moduleLevels 的读写
// ============================================================================
void LogHelper::setModuleLevel(const QString &category, Level level)
{
    QMutexLocker locker(&s_mutex);
    s_moduleLevels[category] = level;
    rebuildAndApplyRules();
}

// ============================================================================
// 批量设置多个模块的日志级别
// ============================================================================
void LogHelper::setModuleLevels(const QMap<QString, Level> &moduleLevels)
{
    QMutexLocker locker(&s_mutex);
    for (auto it = moduleLevels.constBegin(); it != moduleLevels.constEnd(); ++it) {
        s_moduleLevels[it.key()] = it.value();
    }
    rebuildAndApplyRules();
}

// ============================================================================
// 禁用指定模块的所有日志
// ============================================================================
void LogHelper::disableModule(const QString &category)
{
    setModuleLevel(category, Level::Off);
}

// ============================================================================
// 启用指定模块的所有日志（恢复到 Debug 级别）
// ============================================================================
void LogHelper::enableModule(const QString &category)
{
    setModuleLevel(category, Level::Debug);
}

// ============================================================================
// 设置全局默认级别（使用通配符 app.*）
// ============================================================================
void LogHelper::setGlobalLevel(Level level)
{
    setModuleLevel("app.*", level);
}

// ============================================================================
// 直接应用原始过滤规则字符串
// 清空内部映射表，完全替换为用户传入的原始规则
// ============================================================================
void LogHelper::applyRawRules(const QString &rules)
{
    QMutexLocker locker(&s_mutex);
    s_moduleLevels.clear();
    QLoggingCategory::setFilterRules(rules);
}

// ============================================================================
// 获取级别的可读名称
// ============================================================================
QString LogHelper::levelName(Level level)
{
    switch (level) {
    case Level::Debug:    return QStringLiteral("Debug");
    case Level::Info:     return QStringLiteral("Info");
    case Level::Warning:  return QStringLiteral("Warning");
    case Level::Critical: return QStringLiteral("Critical");
    case Level::Off:      return QStringLiteral("Off");
    }
    return QStringLiteral("Unknown");
}

// ============================================================================
// 从模块级别映射表重新生成完整规则字符串并应用
// 每次调用都生成全新的规则字符串，避免旧规则无限堆积
// 注意: 调用前必须已持有 s_mutex 锁
// ============================================================================
void LogHelper::rebuildAndApplyRules()
{
    QString rules;
    for (auto it = s_moduleLevels.constBegin(); it != s_moduleLevels.constEnd(); ++it) {
        rules += buildRulesForLevel(it.key(), it.value());
    }
    QLoggingCategory::setFilterRules(rules);
}

// ============================================================================
// 根据级别生成单个模块的过滤规则
// 例: category="app.network", level=Warning
//     生成: "app.network.debug=false\napp.network.info=false\n
//            app.network.warning=true\napp.network.critical=true\n"
// ============================================================================
QString LogHelper::buildRulesForLevel(const QString &category, Level level)
{
    QString rules;
    rules += QString("%1.debug=%2\n")
                 .arg(category, level <= Level::Debug ? "true" : "false");
    rules += QString("%1.info=%2\n")
                 .arg(category, level <= Level::Info ? "true" : "false");
    rules += QString("%1.warning=%2\n")
                 .arg(category, level <= Level::Warning ? "true" : "false");
    rules += QString("%1.critical=%2\n")
                 .arg(category, level <= Level::Critical ? "true" : "false");
    return rules;
}
