// ============================================================================
// LogHelper 单元测试
// 基于 QTest 框架，覆盖 LogHelper 所有公共接口及边界场景
// ============================================================================

#include <QtTest/QtTest>
#include <QLoggingCategory>
#include <QList>
#include <QPair>
#include <QMutex>

#include "log_categories.h"
#include "log_helper.h"

// ============================================================================
// 日志捕获器 - 通过 qInstallMessageHandler 拦截日志输出
// 记录每条日志的级别、分类名、消息内容，供测试断言使用
// ============================================================================
struct CapturedLog {
    QtMsgType type;
    QString category;
    QString message;
};

static QList<CapturedLog> s_capturedLogs;
static QMutex s_captureMutex;

void testMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&s_captureMutex);
    s_capturedLogs.append({type, QString::fromUtf8(context.category), msg});
}

// ============================================================================
// 测试类
// ============================================================================
class TestLogHelper : public QObject
{
    Q_OBJECT

private:
    // 清空捕获的日志 & 重置过滤规则
    void resetState()
    {
        {
            QMutexLocker locker(&s_captureMutex);
            s_capturedLogs.clear();
        }
        // 重置为全部开启（applyRawRules 内部有自己的锁保护）
        LogHelper::applyRawRules(
            "app.*.debug=true\n"
            "app.*.info=true\n"
            "app.*.warning=true\n"
            "app.*.critical=true\n"
        );
    }

    // 检查捕获的日志中是否包含指定分类+级别的记录
    bool hasLog(QtMsgType type, const QString &category)
    {
        QMutexLocker locker(&s_captureMutex);
        for (const auto &log : s_capturedLogs) {
            if (log.type == type && log.category == category)
                return true;
        }
        return false;
    }

    // 统计指定分类的日志条数
    int countLogs(const QString &category)
    {
        QMutexLocker locker(&s_captureMutex);
        int count = 0;
        for (const auto &log : s_capturedLogs) {
            if (log.category == category)
                ++count;
        }
        return count;
    }

    // 统计指定分类+级别的日志条数
    int countLogs(QtMsgType type, const QString &category)
    {
        QMutexLocker locker(&s_captureMutex);
        int count = 0;
        for (const auto &log : s_capturedLogs) {
            if (log.type == type && log.category == category)
                ++count;
        }
        return count;
    }

    // 获取所有捕获日志的快照
    QList<CapturedLog> snapshot()
    {
        QMutexLocker locker(&s_captureMutex);
        return s_capturedLogs;
    }

    // 发射一组四级别日志到指定分类
    void emitAllLevels(const QLoggingCategory &cat)
    {
        qCDebug(cat) << "test-debug";
        qCInfo(cat) << "test-info";
        qCWarning(cat) << "test-warning";
        qCCritical(cat) << "test-critical";
    }

private slots:
    // ------------------------------------------------------------------
    // 框架钩子
    // ------------------------------------------------------------------
    void initTestCase()
    {
        qInstallMessageHandler(testMessageHandler);
    }

    void init()
    {
        resetState();
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(nullptr);
    }

    // ==================================================================
    // 1. levelName() 测试
    // ==================================================================
    void testLevelName_Debug()
    {
        QCOMPARE(LogHelper::levelName(LogHelper::Level::Debug), QStringLiteral("Debug"));
    }

    void testLevelName_Info()
    {
        QCOMPARE(LogHelper::levelName(LogHelper::Level::Info), QStringLiteral("Info"));
    }

    void testLevelName_Warning()
    {
        QCOMPARE(LogHelper::levelName(LogHelper::Level::Warning), QStringLiteral("Warning"));
    }

    void testLevelName_Critical()
    {
        QCOMPARE(LogHelper::levelName(LogHelper::Level::Critical), QStringLiteral("Critical"));
    }

    void testLevelName_Off()
    {
        QCOMPARE(LogHelper::levelName(LogHelper::Level::Off), QStringLiteral("Off"));
    }

    // ==================================================================
    // 2. 默认状态测试 - 所有级别均可输出
    // ==================================================================
    void testDefaultState_AllLevelsEnabled()
    {
        emitAllLevels(logNetwork());
        QVERIFY(hasLog(QtDebugMsg, "app.network"));
        QVERIFY(hasLog(QtInfoMsg, "app.network"));
        QVERIFY(hasLog(QtWarningMsg, "app.network"));
        QVERIFY(hasLog(QtCriticalMsg, "app.network"));
        QCOMPARE(countLogs("app.network"), 4);
    }

    void testDefaultState_AllModulesEnabled()
    {
        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        emitAllLevels(logFileIO());
        emitAllLevels(logUI());

        QCOMPARE(countLogs("app.network"), 4);
        QCOMPARE(countLogs("app.database"), 4);
        QCOMPARE(countLogs("app.fileio"), 4);
        QCOMPARE(countLogs("app.ui"), 4);
    }

    // ==================================================================
    // 3. setModuleLevel() 测试 - 单模块级别控制
    // ==================================================================
    void testSetModuleLevel_DebugLevel_AllPass()
    {
        LogHelper::setModuleLevel("app.network", LogHelper::Level::Debug);
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    void testSetModuleLevel_InfoLevel_FilterDebug()
    {
        LogHelper::setModuleLevel("app.network", LogHelper::Level::Info);
        emitAllLevels(logNetwork());

        QVERIFY(!hasLog(QtDebugMsg, "app.network"));
        QVERIFY(hasLog(QtInfoMsg, "app.network"));
        QVERIFY(hasLog(QtWarningMsg, "app.network"));
        QVERIFY(hasLog(QtCriticalMsg, "app.network"));
        QCOMPARE(countLogs("app.network"), 3);
    }

    void testSetModuleLevel_WarningLevel_FilterDebugAndInfo()
    {
        LogHelper::setModuleLevel("app.database", LogHelper::Level::Warning);
        emitAllLevels(logDatabase());

        QVERIFY(!hasLog(QtDebugMsg, "app.database"));
        QVERIFY(!hasLog(QtInfoMsg, "app.database"));
        QVERIFY(hasLog(QtWarningMsg, "app.database"));
        QVERIFY(hasLog(QtCriticalMsg, "app.database"));
        QCOMPARE(countLogs("app.database"), 2);
    }

    void testSetModuleLevel_CriticalLevel_OnlyCritical()
    {
        LogHelper::setModuleLevel("app.fileio", LogHelper::Level::Critical);
        emitAllLevels(logFileIO());

        QVERIFY(!hasLog(QtDebugMsg, "app.fileio"));
        QVERIFY(!hasLog(QtInfoMsg, "app.fileio"));
        QVERIFY(!hasLog(QtWarningMsg, "app.fileio"));
        QVERIFY(hasLog(QtCriticalMsg, "app.fileio"));
        QCOMPARE(countLogs("app.fileio"), 1);
    }

    void testSetModuleLevel_OffLevel_NonePass()
    {
        LogHelper::setModuleLevel("app.ui", LogHelper::Level::Off);
        emitAllLevels(logUI());
        QCOMPARE(countLogs("app.ui"), 0);
    }

    // ==================================================================
    // 4. 模块隔离性测试 - 设置一个模块不影响其他模块
    // ==================================================================
    void testModuleIsolation_DisableOneModuleOthersUnaffected()
    {
        LogHelper::disableModule("app.database");

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        emitAllLevels(logFileIO());

        // 网络和文件模块不受影响
        QCOMPARE(countLogs("app.network"), 4);
        QCOMPARE(countLogs("app.fileio"), 4);
        // 数据库模块全部被过滤
        QCOMPARE(countLogs("app.database"), 0);
    }

    void testModuleIsolation_DifferentLevelsPerModule()
    {
        LogHelper::setModuleLevel("app.network", LogHelper::Level::Warning);
        LogHelper::setModuleLevel("app.fileio", LogHelper::Level::Info);

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        emitAllLevels(logFileIO());

        QCOMPARE(countLogs("app.network"), 2);   // Warning + Critical
        QCOMPARE(countLogs("app.database"), 4);   // 未设置，全部通过
        QCOMPARE(countLogs("app.fileio"), 3);     // Info + Warning + Critical
    }

    // ==================================================================
    // 5. disableModule() / enableModule() 测试
    // ==================================================================
    void testDisableModule_AllLogsSuppressed()
    {
        LogHelper::disableModule("app.network");
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 0);
    }

    void testEnableModule_AllLogsRestored()
    {
        // 先禁用
        LogHelper::disableModule("app.network");
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 0);

        // 清空捕获，再启用
        {
            QMutexLocker locker(&s_captureMutex);
            s_capturedLogs.clear();
        }

        LogHelper::enableModule("app.network");
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    void testDisableEnable_Cycle()
    {
        // 禁用 -> 启用 -> 禁用 -> 启用，验证状态切换正确
        for (int i = 0; i < 3; ++i) {
            LogHelper::disableModule("app.database");
            {
                QMutexLocker locker(&s_captureMutex);
                s_capturedLogs.clear();
            }
            emitAllLevels(logDatabase());
            QCOMPARE(countLogs("app.database"), 0);

            LogHelper::enableModule("app.database");
            {
                QMutexLocker locker(&s_captureMutex);
                s_capturedLogs.clear();
            }
            emitAllLevels(logDatabase());
            QCOMPARE(countLogs("app.database"), 4);
        }
    }

    // ==================================================================
    // 6. setGlobalLevel() 测试
    // ==================================================================
    void testSetGlobalLevel_Info_AllModulesFilterDebug()
    {
        LogHelper::setGlobalLevel(LogHelper::Level::Info);

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        emitAllLevels(logFileIO());
        emitAllLevels(logUI());

        // 所有模块的 Debug 都被过滤
        QVERIFY(!hasLog(QtDebugMsg, "app.network"));
        QVERIFY(!hasLog(QtDebugMsg, "app.database"));
        QVERIFY(!hasLog(QtDebugMsg, "app.fileio"));
        QVERIFY(!hasLog(QtDebugMsg, "app.ui"));

        // Info/Warning/Critical 正常输出
        QCOMPARE(countLogs("app.network"), 3);
        QCOMPARE(countLogs("app.database"), 3);
        QCOMPARE(countLogs("app.fileio"), 3);
        QCOMPARE(countLogs("app.ui"), 3);
    }

    void testSetGlobalLevel_Off_AllModulesSilenced()
    {
        LogHelper::setGlobalLevel(LogHelper::Level::Off);

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        emitAllLevels(logFileIO());
        emitAllLevels(logUI());

        QCOMPARE(countLogs("app.network"), 0);
        QCOMPARE(countLogs("app.database"), 0);
        QCOMPARE(countLogs("app.fileio"), 0);
        QCOMPARE(countLogs("app.ui"), 0);
    }

    void testSetGlobalLevel_Debug_AllModulesFullOutput()
    {
        // 先设为 Off，再恢复为 Debug
        LogHelper::setGlobalLevel(LogHelper::Level::Off);
        LogHelper::setGlobalLevel(LogHelper::Level::Debug);

        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    // ==================================================================
    // 7. 全局级别 + 单模块例外 测试
    // ==================================================================
    void testGlobalWithException_GlobalInfoButNetworkDebug()
    {
        // 全局 Info（过滤 Debug），但网络模块单独开启 Debug
        LogHelper::setGlobalLevel(LogHelper::Level::Info);
        LogHelper::setModuleLevel("app.network", LogHelper::Level::Debug);

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());

        // 网络模块: 4 条全部输出（包括 Debug）
        QCOMPARE(countLogs("app.network"), 4);
        QVERIFY(hasLog(QtDebugMsg, "app.network"));

        // 数据库模块: Debug 被全局规则过滤
        QCOMPARE(countLogs("app.database"), 3);
        QVERIFY(!hasLog(QtDebugMsg, "app.database"));
    }

    void testGlobalWithException_GlobalWarningButFileIOInfo()
    {
        LogHelper::setGlobalLevel(LogHelper::Level::Warning);
        LogHelper::setModuleLevel("app.fileio", LogHelper::Level::Info);

        emitAllLevels(logNetwork());
        emitAllLevels(logFileIO());

        // 网络: 仅 Warning + Critical
        QCOMPARE(countLogs("app.network"), 2);
        // 文件: Info + Warning + Critical
        QCOMPARE(countLogs("app.fileio"), 3);
        QVERIFY(hasLog(QtInfoMsg, "app.fileio"));
    }

    // ==================================================================
    // 8. setModuleLevels() 批量配置测试
    // ==================================================================
    void testSetModuleLevels_BatchConfig()
    {
        LogHelper::setModuleLevels({
            {"app.network",  LogHelper::Level::Warning},
            {"app.database", LogHelper::Level::Off},
            {"app.fileio",   LogHelper::Level::Info}
        });

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        emitAllLevels(logFileIO());
        emitAllLevels(logUI());

        QCOMPARE(countLogs("app.network"), 2);    // Warning + Critical
        QCOMPARE(countLogs("app.database"), 0);    // 全部关闭
        QCOMPARE(countLogs("app.fileio"), 3);      // Info + Warning + Critical
        QCOMPARE(countLogs("app.ui"), 4);           // 未配置，全部通过
    }

    void testSetModuleLevels_EmptyMap_NoChange()
    {
        QMap<QString, LogHelper::Level> empty;
        LogHelper::setModuleLevels(empty);

        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    void testSetModuleLevels_SingleEntry()
    {
        LogHelper::setModuleLevels({
            {"app.ui", LogHelper::Level::Critical}
        });

        emitAllLevels(logUI());
        QCOMPARE(countLogs("app.ui"), 1);
        QVERIFY(hasLog(QtCriticalMsg, "app.ui"));
    }

    // ==================================================================
    // 9. applyRawRules() 测试
    // ==================================================================
    void testApplyRawRules_DisableAllDebug()
    {
        LogHelper::applyRawRules("app.*.debug=false\n");

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());

        QVERIFY(!hasLog(QtDebugMsg, "app.network"));
        QVERIFY(!hasLog(QtDebugMsg, "app.database"));
        QVERIFY(hasLog(QtInfoMsg, "app.network"));
        QVERIFY(hasLog(QtInfoMsg, "app.database"));
    }

    void testApplyRawRules_ComplexRules()
    {
        LogHelper::applyRawRules(
            "app.*.debug=false\n"
            "app.*.info=false\n"
            "app.network.debug=true\n"
            "app.network.info=true\n"
        );

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());

        // 网络: 全部输出（具体规则覆盖通配符）
        QCOMPARE(countLogs("app.network"), 4);
        // 数据库: Debug/Info 被过滤
        QCOMPARE(countLogs("app.database"), 2);
        QVERIFY(!hasLog(QtDebugMsg, "app.database"));
        QVERIFY(!hasLog(QtInfoMsg, "app.database"));
    }

    void testApplyRawRules_EmptyString_ResetsToDefault()
    {
        // 先禁用所有
        LogHelper::setGlobalLevel(LogHelper::Level::Off);
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 0);

        // 清空捕获
        {
            QMutexLocker locker(&s_captureMutex);
            s_capturedLogs.clear();
        }

        // 应用空规则 -> 恢复默认（Q_LOGGING_CATEGORY 中定义的默认级别）
        LogHelper::applyRawRules("");
        emitAllLevels(logNetwork());
        // 默认级别为 QtDebugMsg，所以全部输出
        QCOMPARE(countLogs("app.network"), 4);
    }

    // ==================================================================
    // 10. 日志消息内容验证
    // ==================================================================
    void testLogMessageContent_CorrectText()
    {
        qCInfo(logNetwork()) << "hello-world-12345";

        auto logs = snapshot();
        bool found = false;
        for (const auto &log : logs) {
            if (log.category == "app.network" && log.message.contains("hello-world-12345")) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Expected log message 'hello-world-12345' not found in captured logs");
    }

    void testLogMessageContent_CategoryTag()
    {
        qCWarning(logFileIO()) << "disk-check";

        auto logs = snapshot();
        QVERIFY(!logs.isEmpty());
        QCOMPARE(logs.last().category, QStringLiteral("app.fileio"));
        QCOMPARE(logs.last().type, QtWarningMsg);
    }

    // ==================================================================
    // 11. 级别覆盖测试 - 后设置的规则覆盖先设置的
    // ==================================================================
    void testLevelOverride_LaterRuleWins()
    {
        // 先设为 Off
        LogHelper::setModuleLevel("app.network", LogHelper::Level::Off);
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 0);

        {
            QMutexLocker locker(&s_captureMutex);
            s_capturedLogs.clear();
        }

        // 再设为 Warning（后设置的规则覆盖）
        LogHelper::setModuleLevel("app.network", LogHelper::Level::Warning);
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 2);
    }

    void testLevelOverride_MultipleOverrides()
    {
        LogHelper::setModuleLevel("app.database", LogHelper::Level::Off);
        LogHelper::setModuleLevel("app.database", LogHelper::Level::Critical);
        LogHelper::setModuleLevel("app.database", LogHelper::Level::Info);

        emitAllLevels(logDatabase());
        // 最后一次设置为 Info，所以 Debug 被过滤，其余通过
        QCOMPARE(countLogs("app.database"), 3);
        QVERIFY(!hasLog(QtDebugMsg, "app.database"));
    }

    // ==================================================================
    // 12. 边界场景测试
    // ==================================================================
    void testEdgeCase_SetLevelOnUnknownCategory()
    {
        // 对不存在的分类设置级别不应崩溃
        LogHelper::setModuleLevel("app.nonexistent", LogHelper::Level::Warning);
        // 已有模块不受影响
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    void testEdgeCase_RapidToggle()
    {
        // 快速切换 100 次，验证不崩溃且最终状态正确
        for (int i = 0; i < 100; ++i) {
            if (i % 2 == 0)
                LogHelper::disableModule("app.network");
            else
                LogHelper::enableModule("app.network");
        }
        // 最后一次是 disable（i=99 是奇数 -> enable, i=98 是偶数 -> disable）
        // 实际上 i 从 0 到 99，最后 i=99 是奇数 -> enableModule
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    void testEdgeCase_EmptyCategory()
    {
        // 空分类名不应崩溃
        LogHelper::setModuleLevel("", LogHelper::Level::Warning);
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    // ==================================================================
    // 13. 全局 Off 后单模块恢复测试
    // ==================================================================
    void testGlobalOffThenRestoreSingleModule()
    {
        LogHelper::setGlobalLevel(LogHelper::Level::Off);

        // 所有模块静默
        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        QCOMPARE(countLogs("app.network"), 0);
        QCOMPARE(countLogs("app.database"), 0);

        {
            QMutexLocker locker(&s_captureMutex);
            s_capturedLogs.clear();
        }

        // 仅恢复网络模块
        LogHelper::enableModule("app.network");
        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());

        QCOMPARE(countLogs("app.network"), 4);
        QCOMPARE(countLogs("app.database"), 0);
    }

    // ==================================================================
    // 14. 各级别精确过滤验证（数据驱动）
    // ==================================================================
    void testPreciseFiltering_data()
    {
        QTest::addColumn<int>("level");
        QTest::addColumn<int>("expectedDebug");
        QTest::addColumn<int>("expectedInfo");
        QTest::addColumn<int>("expectedWarning");
        QTest::addColumn<int>("expectedCritical");

        //                              level  dbg  info  warn  crit
        QTest::newRow("Debug")    << 0  << 1  << 1  << 1  << 1;
        QTest::newRow("Info")     << 1  << 0  << 1  << 1  << 1;
        QTest::newRow("Warning")  << 2  << 0  << 0  << 1  << 1;
        QTest::newRow("Critical") << 3  << 0  << 0  << 0  << 1;
        QTest::newRow("Off")      << 4  << 0  << 0  << 0  << 0;
    }

    void testPreciseFiltering()
    {
        QFETCH(int, level);
        QFETCH(int, expectedDebug);
        QFETCH(int, expectedInfo);
        QFETCH(int, expectedWarning);
        QFETCH(int, expectedCritical);

        LogHelper::setModuleLevel("app.network", static_cast<LogHelper::Level>(level));
        emitAllLevels(logNetwork());

        QCOMPARE(countLogs(QtDebugMsg, "app.network"), expectedDebug);
        QCOMPARE(countLogs(QtInfoMsg, "app.network"), expectedInfo);
        QCOMPARE(countLogs(QtWarningMsg, "app.network"), expectedWarning);
        QCOMPARE(countLogs(QtCriticalMsg, "app.network"), expectedCritical);
    }

    // ==================================================================
    // 15. applyRawRules 覆盖 setModuleLevel 测试
    // ==================================================================
    void testApplyRawRules_OverridesPreviousSetModuleLevel()
    {
        LogHelper::setModuleLevel("app.network", LogHelper::Level::Off);
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 0);

        {
            QMutexLocker locker(&s_captureMutex);
            s_capturedLogs.clear();
        }

        // applyRawRules 完全替换之前的规则
        LogHelper::applyRawRules("app.network.debug=true\napp.network.info=true\n"
                                 "app.network.warning=true\napp.network.critical=true\n");
        emitAllLevels(logNetwork());
        QCOMPARE(countLogs("app.network"), 4);
    }

    // ==================================================================
    // 16. 多模块同时操作测试
    // ==================================================================
    void testMultiModuleSimultaneous()
    {
        LogHelper::disableModule("app.network");
        LogHelper::setModuleLevel("app.database", LogHelper::Level::Warning);
        LogHelper::setModuleLevel("app.fileio", LogHelper::Level::Critical);
        LogHelper::enableModule("app.ui");

        emitAllLevels(logNetwork());
        emitAllLevels(logDatabase());
        emitAllLevels(logFileIO());
        emitAllLevels(logUI());

        QCOMPARE(countLogs("app.network"), 0);
        QCOMPARE(countLogs("app.database"), 2);
        QCOMPARE(countLogs("app.fileio"), 1);
        QCOMPARE(countLogs("app.ui"), 4);
    }
};

QTEST_GUILESS_MAIN(TestLogHelper)
#include "tst_log_helper.moc"
