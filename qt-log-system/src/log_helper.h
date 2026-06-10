#ifndef LOG_HELPER_H
#define LOG_HELPER_H

#include <QString>
#include <QMap>
#include <QMutex>
#include <QtGlobal>

// ============================================================================
// LogHelper - 日志辅助工具类
// 提供便捷的模块级别设置、批量配置、模块启用/禁用等静态方法
// 底层完全基于 Qt 原生 QLoggingCategory::setFilterRules() 实现
//
// 线程安全: 所有公共方法均通过 QMutex 保护内部状态，可在多线程环境下安全调用
// 规则管理: 使用 QMap 维护每个模块的当前级别，每次变更时重新生成完整规则字符串，
//           避免规则无限追加导致的内存增长和解析性能下降
// ============================================================================

class LogHelper
{
public:
    // 日志级别枚举，从低到高
    enum class Level {
        Debug    = 0,   // 调试信息
        Info     = 1,   // 一般信息
        Warning  = 2,   // 警告信息
        Critical = 3,   // 严重错误
        Off      = 4    // 完全关闭
    };

    // 设置单个模块的最低日志级别
    // 例: setModuleLevel("app.network", Level::Warning)
    //     -> 网络模块仅输出 Warning 和 Critical，过滤 Debug 和 Info
    static void setModuleLevel(const QString &category, Level level);

    // 批量设置多个模块的日志级别
    // 例: setModuleLevels({{"app.network", Level::Warning}, {"app.database", Level::Off}})
    static void setModuleLevels(const QMap<QString, Level> &moduleLevels);

    // 禁用指定模块的所有日志输出
    static void disableModule(const QString &category);

    // 启用指定模块的所有日志输出（恢复到 Debug 级别）
    static void enableModule(const QString &category);

    // 设置全局默认级别（影响所有 app.* 模块）
    static void setGlobalLevel(Level level);

    // 直接应用原始过滤规则字符串（高级用法）
    // 注意: 此方法会清空内部模块级别映射，完全替换为传入的原始规则
    // 规则格式参考 Qt 文档: https://doc.qt.io/qt-6/qloggingcategory.html
    static void applyRawRules(const QString &rules);

    // 获取级别的可读名称
    static QString levelName(Level level);

private:
    // 根据级别生成单个模块的过滤规则字符串
    static QString buildRulesForLevel(const QString &category, Level level);

    // 从 s_moduleLevels 重新生成完整规则字符串并应用
    // 调用前必须已持有 s_mutex 锁
    static void rebuildAndApplyRules();

    // 模块级别映射表: category -> Level
    // 每次变更时从此表重新生成完整规则，避免规则无限追加
    static QMap<QString, Level> s_moduleLevels;

    // 保护 s_moduleLevels 的互斥锁，确保多线程安全
    static QMutex s_mutex;
};

#endif // LOG_HELPER_H
