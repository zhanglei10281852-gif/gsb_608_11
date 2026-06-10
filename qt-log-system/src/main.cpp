// ============================================================================
// Qt 模块级分级日志系统 - 主函数 & 演示代码
// 基于 QLoggingCategory 原生 API，兼容 Qt6
// ============================================================================

#include <QCoreApplication>
#include <cstdio>

#include "log_categories.h"
#include "log_helper.h"
#include "log_formatter.h"
#include "network_module.h"
#include "database_module.h"
#include "fileio_module.h"

// ============================================================================
// 辅助函数: 打印分隔线
// ============================================================================
static void printSection(const char *title)
{
    fprintf(stderr, "\n%s\n", "================================================================");
    fprintf(stderr, "  %s\n", title);
    fprintf(stderr, "%s\n\n", "================================================================");
    fflush(stderr);
}

// ============================================================================
// 场景1: 默认状态 - 所有模块输出所有级别日志
// ============================================================================
static void scenario_default()
{
    printSection("Scenario 1: Default - All modules, all levels");

    NetworkModule net;
    DatabaseModule db;
    FileIOModule file;

    net.sendRequest("https://api.example.com/users");
    db.connect("localhost", 3306);
    file.readFile("/data/config.json");
}

// ============================================================================
// 场景2: 禁用数据库模块所有日志
// ============================================================================
static void scenario_disable_database()
{
    printSection("Scenario 2: Disable all database module logs");

    LogHelper::disableModule("app.database");

    NetworkModule net;
    DatabaseModule db;
    FileIOModule file;

    net.sendRequest("https://api.example.com/data");
    db.connect("db-server", 5432);
    db.executeQuery("SELECT * FROM t");
    file.writeFile("/data/output.csv");

    LogHelper::enableModule("app.database");
}

// ============================================================================
// 场景3: 仅保留文件模块 Critical 日志
// ============================================================================
static void scenario_fileio_critical_only()
{
    printSection("Scenario 3: FileIO module - Critical only");

    LogHelper::setModuleLevel("app.fileio", LogHelper::Level::Critical);

    FileIOModule file;
    file.readFile("/data/test.txt");
    file.writeFile("/data/test.txt");
    file.simulateDiskFull();

    LogHelper::enableModule("app.fileio");
}

// ============================================================================
// 场景4: 全局默认过滤 Debug，仅网络模块开启 Debug
// ============================================================================
static void scenario_global_filter_with_exception()
{
    printSection("Scenario 4: Global filter Debug, but enable Debug for network only");

    LogHelper::setGlobalLevel(LogHelper::Level::Info);
    LogHelper::setModuleLevel("app.network", LogHelper::Level::Debug);

    NetworkModule net;
    DatabaseModule db;
    FileIOModule file;

    net.sendRequest("https://api.example.com/debug");
    net.handleResponse(200);
    db.executeQuery("INSERT INTO logs VALUES(...)");
    file.readFile("/var/log/app.log");

    LogHelper::setGlobalLevel(LogHelper::Level::Debug);
}

// ============================================================================
// 场景5: 批量配置多个模块不同级别
// ============================================================================
static void scenario_batch_config()
{
    printSection("Scenario 5: Batch config - different levels per module");

    LogHelper::setModuleLevels({
        {"app.network",  LogHelper::Level::Warning},
        {"app.database", LogHelper::Level::Off},
        {"app.fileio",   LogHelper::Level::Info}
    });

    NetworkModule net;
    DatabaseModule db;
    FileIOModule file;

    net.sendRequest("https://api.example.com/batch");
    net.simulateTimeout();
    db.simulateConnectionLost();
    file.readFile("/data/batch.dat");
    file.simulateDiskFull();

    LogHelper::setGlobalLevel(LogHelper::Level::Debug);
}

// ============================================================================
// 场景6: 使用原始规则字符串（高级用法）
// ============================================================================
static void scenario_raw_rules()
{
    printSection("Scenario 6: Raw filter rules string");

    LogHelper::applyRawRules(
        "app.*.debug=false\n"
        "app.*.info=false\n"
        "app.network.debug=true\n"
        "app.network.info=true\n"
    );

    NetworkModule net;
    DatabaseModule db;

    net.sendRequest("https://api.example.com/raw");
    db.executeQuery("DROP TABLE students");
    db.simulateConnectionLost();

    LogHelper::applyRawRules("");
}

// ============================================================================
// 主函数
// ============================================================================
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 安装日志格式化处理器（一行代码即可集成）
    LogFormatter::install();

    // 注意: 本演示程序未调用 app.exec() 启动事件循环。
    // 日志系统的所有功能（分类过滤、级别控制、格式化输出）均为同步操作，
    // 不依赖 Qt 事件循环。QCoreApplication 仅用于初始化 Qt 内部基础设施
    // （如时区、编码等），确保 QDateTime 等工具类正常工作。

    fprintf(stderr, "Qt Log System Demo - Module-level & Level-based Control\n");
    fprintf(stderr, "Qt Version: %s\n", qVersion());
    fprintf(stderr, "========================================================\n");

    scenario_default();
    scenario_disable_database();
    scenario_fileio_critical_only();
    scenario_global_filter_with_exception();
    scenario_batch_config();
    scenario_raw_rules();

    printSection("All scenarios completed");

    // 日志系统不依赖事件循环，直接返回即可，无需 app.exec()
    return 0;
}
