// ============================================================================
// LogFormatter 单元测试
// 基于 QTest 框架，覆盖文件输出、滚动切割、配置接口等功能
// ============================================================================

#include <QtTest/QtTest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <QTemporaryDir>

#include "log_formatter.h"
#include "log_categories.h"

// ============================================================================
// 测试类
// ============================================================================
class TestLogFormatter : public QObject
{
    Q_OBJECT

private:
    // 测试用临时目录
    QTemporaryDir m_tempDir;

    // 获取临时目录下的日志文件路径
    QString logPath(const QString &name)
    {
        return m_tempDir.path() + QDir::separator() + name;
    }

    // 统计某个目录下匹配模式的文件数量
    int countBackupFiles(const QString &baseName)
    {
        QDir dir(m_tempDir.path());
        QStringList filters;
        filters << baseName + ".*";
        QStringList entries = dir.entryList(filters, QDir::Files);
        return entries.size();
    }

    // 获取所有备份文件名（排序后）
    QStringList listBackupFiles(const QString &baseName)
    {
        QDir dir(m_tempDir.path());
        QStringList filters;
        filters << baseName + ".*";
        QStringList entries = dir.entryList(filters, QDir::Files, QDir::Name);
        return entries;
    }

    // 读取文件全部内容
    QString readFile(const QString &filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();
        return QString::fromUtf8(file.readAll());
    }

    // 写入指定字节数的日志（通过写多条日志来累积大小）
    void writeLogs(int count, const QLoggingCategory &cat, const QString &prefix)
    {
        for (int i = 0; i < count; ++i) {
            qCInfo(cat) << qPrintable(prefix + QString::number(i));
        }
    }

private slots:
    // ------------------------------------------------------------------
    // 框架钩子
    // ------------------------------------------------------------------
    void initTestCase()
    {
        // 确保临时目录有效
        QVERIFY(m_tempDir.isValid());
    }

    void init()
    {
        // 每个测试前重置配置
        LogFormatter::setFileOutputEnabled(false);
        LogFormatter::setLogFilePath("app.log");
        LogFormatter::setMaxFileSize(10 * 1024 * 1024);
        LogFormatter::setMaxBackupFiles(5);
        // 安装处理器
        LogFormatter::install();
    }

    void cleanup()
    {
        // 卸载处理器
        LogFormatter::uninstall();
        // 清理临时目录中的文件
        QDir dir(m_tempDir.path());
        QStringList entries = dir.entryList(QDir::Files);
        for (const QString &f : entries) {
            dir.remove(f);
        }
    }

    // ==================================================================
    // 1. 默认配置测试
    // ==================================================================
    void testDefaultConfig_FileOutputDisabled()
    {
        QCOMPARE(LogFormatter::isFileOutputEnabled(), false);
    }

    void testDefaultConfig_LogFilePath()
    {
        QCOMPARE(LogFormatter::logFilePath(), QStringLiteral("app.log"));
    }

    void testDefaultConfig_MaxFileSize()
    {
        // 默认 10MB
        QCOMPARE(LogFormatter::maxFileSize(), 10LL * 1024 * 1024);
    }

    void testDefaultConfig_MaxBackupFiles()
    {
        QCOMPARE(LogFormatter::maxBackupFiles(), 5);
    }

    // ==================================================================
    // 2. 配置接口测试
    // ==================================================================
    void testSetFileOutputEnabled()
    {
        LogFormatter::setFileOutputEnabled(true);
        QCOMPARE(LogFormatter::isFileOutputEnabled(), true);

        LogFormatter::setFileOutputEnabled(false);
        QCOMPARE(LogFormatter::isFileOutputEnabled(), false);
    }

    void testSetLogFilePath()
    {
        QString path = logPath("test.log");
        LogFormatter::setLogFilePath(path);
        QCOMPARE(LogFormatter::logFilePath(), path);
    }

    void testSetMaxFileSize()
    {
        LogFormatter::setMaxFileSize(2048);
        QCOMPARE(LogFormatter::maxFileSize(), 2048LL);

        // 负值应该被忽略
        LogFormatter::setMaxFileSize(-100);
        QCOMPARE(LogFormatter::maxFileSize(), 2048LL);
    }

    void testSetMaxBackupFiles()
    {
        LogFormatter::setMaxBackupFiles(10);
        QCOMPARE(LogFormatter::maxBackupFiles(), 10);

        // 负值应该被忽略
        LogFormatter::setMaxBackupFiles(-1);
        QCOMPARE(LogFormatter::maxBackupFiles(), 10);
    }

    // ==================================================================
    // 3. 文件输出功能测试
    // ==================================================================
    void testFileOutput_Disabled_NoFileCreated()
    {
        QString path = logPath("no_output.log");
        LogFormatter::setLogFilePath(path);
        // 不启用文件输出

        qCInfo(logNetwork()) << "this should not go to file";

        QVERIFY(!QFile::exists(path));
    }

    void testFileOutput_Enabled_FileCreated()
    {
        QString path = logPath("output.log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setFileOutputEnabled(true);

        qCInfo(logNetwork()) << "hello file log";

        // 卸载以确保文件关闭
        LogFormatter::uninstall();

        QVERIFY(QFile::exists(path));
    }

    void testFileOutput_ContentFormat()
    {
        QString path = logPath("format.log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setFileOutputEnabled(true);

        qCInfo(logNetwork()) << "format-test-message";

        LogFormatter::uninstall();

        QString content = readFile(path);
        QVERIFY(!content.isEmpty());
        // 级别标签左对齐，宽度为 8 个字符
        QVERIFY(content.contains("INFO"));
        QVERIFY(content.contains("[INFO"));
        QVERIFY(content.contains("app.network"));
        QVERIFY(content.contains("format-test-message"));
        // 检查时间戳格式 [yyyy-MM-dd hh:mm:ss.zzz]
        QVERIFY(content.startsWith("["));
        QVERIFY(content.contains("] ["));
        // 检查三段式结构: [时间] [级别] [模块名] 消息
        int firstBracket = content.indexOf(']');
        int secondBracket = content.indexOf(']', firstBracket + 1);
        int thirdBracket = content.indexOf(']', secondBracket + 1);
        QVERIFY(firstBracket > 0);
        QVERIFY(secondBracket > firstBracket);
        QVERIFY(thirdBracket > secondBracket);
    }

    void testFileOutput_MultipleLines()
    {
        QString path = logPath("multiline.log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setFileOutputEnabled(true);

        for (int i = 0; i < 10; ++i) {
            qCDebug(logDatabase()) << "line" << i;
        }

        LogFormatter::uninstall();

        QString content = readFile(path);
        QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 10);
    }

    // ==================================================================
    // 4. 滚动切割测试
    // ==================================================================
    void testRollover_TriggersWhenSizeExceeded()
    {
        QString baseName = "rollover_test";
        QString path = logPath(baseName + ".log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setMaxFileSize(200); // 很小的限制，很快触发滚动
        LogFormatter::setMaxBackupFiles(3);
        LogFormatter::setFileOutputEnabled(true);

        // 写入足够多的日志来触发至少一次滚动
        for (int i = 0; i < 50; ++i) {
            qCInfo(logFileIO()) << "rollover test log entry number" << i;
        }

        LogFormatter::uninstall();

        // 应该有备份文件
        int backupCount = countBackupFiles(baseName + ".log");
        QVERIFY(backupCount >= 1);
        QVERIFY(backupCount <= 3);
    }

    void testRollover_BackupCountLimited()
    {
        QString baseName = "limit_test";
        QString path = logPath(baseName + ".log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setMaxFileSize(100); // 小阈值
        LogFormatter::setMaxBackupFiles(3); // 最多 3 个备份
        LogFormatter::setFileOutputEnabled(true);

        // 写很多，确保滚动多次
        for (int i = 0; i < 200; ++i) {
            qCInfo(logFileIO()) << "limit test entry" << i;
        }

        LogFormatter::uninstall();

        QStringList backups = listBackupFiles(baseName + ".log");
        // 备份文件数不应超过设置的 maxBackupFiles
        QVERIFY(backups.size() <= 3);

        // 验证备份编号
        for (const QString &name : backups) {
            // 文件名应该是 baseName.log.1, .2, .3 等
            QVERIFY(name.startsWith(baseName + ".log."));
        }
    }

    void testRollover_BackupNamingOrder()
    {
        QString baseName = "order_test";
        QString path = logPath(baseName + ".log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setMaxFileSize(100);
        LogFormatter::setMaxBackupFiles(5);
        LogFormatter::setFileOutputEnabled(true);

        // 写足够多，确保有多次滚动
        for (int i = 0; i < 300; ++i) {
            qCInfo(logFileIO()) << "order test" << i;
        }

        LogFormatter::uninstall();

        QStringList backups = listBackupFiles(baseName + ".log");

        // 验证文件名格式：.1, .2, .3...
        QSet<int> numbers;
        for (const QString &name : backups) {
            QString suffix = name.section('.', -1);
            bool ok = false;
            int num = suffix.toInt(&ok);
            QVERIFY(ok);
            numbers.insert(num);
        }

        // 编号应该连续从 1 开始
        for (int i = 1; i <= backups.size(); ++i) {
            QVERIFY(numbers.contains(i));
        }
    }

    void testRollover_ZeroBackupFiles()
    {
        QString baseName = "zero_backup";
        QString path = logPath(baseName + ".log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setMaxFileSize(100);
        LogFormatter::setMaxBackupFiles(0); // 不保留备份
        LogFormatter::setFileOutputEnabled(true);

        for (int i = 0; i < 100; ++i) {
            qCInfo(logFileIO()) << "zero backup test" << i;
        }

        LogFormatter::uninstall();

        // 应该没有备份文件
        int backups = countBackupFiles(baseName + ".log");
        QCOMPARE(backups, 0);
        // 当前日志文件应该存在
        QVERIFY(QFile::exists(path));
    }

    void testRollover_CurrentFileAlwaysExists()
    {
        QString baseName = "current_exists";
        QString path = logPath(baseName + ".log");
        LogFormatter::setLogFilePath(path);
        LogFormatter::setMaxFileSize(50);
        LogFormatter::setMaxBackupFiles(2);
        LogFormatter::setFileOutputEnabled(true);

        for (int i = 0; i < 100; ++i) {
            qCInfo(logFileIO()) << "current file test" << i;
        }

        // 不卸载，直接检查当前文件是否存在
        QVERIFY(QFile::exists(path));

        LogFormatter::uninstall();
    }

    // ==================================================================
    // 5. 启用/禁用切换测试
    // ==================================================================
    void testToggleFileOutput_OnOffOn()
    {
        QString path = logPath("toggle.log");
        LogFormatter::setLogFilePath(path);

        // 开启
        LogFormatter::setFileOutputEnabled(true);
        qCInfo(logNetwork()) << "first message";

        // 关闭
        LogFormatter::setFileOutputEnabled(false);
        qCInfo(logNetwork()) << "should not be in file";

        // 再开启
        LogFormatter::setFileOutputEnabled(true);
        qCInfo(logNetwork()) << "second message";

        LogFormatter::uninstall();

        QString content = readFile(path);
        QVERIFY(content.contains("first message"));
        QVERIFY(!content.contains("should not be in file"));
        QVERIFY(content.contains("second message"));
    }

    // ==================================================================
    // 6. 路径变更测试
    // ==================================================================
    void testChangeFilePath_NewFileCreated()
    {
        QString path1 = logPath("path1.log");
        QString path2 = logPath("path2.log");

        LogFormatter::setLogFilePath(path1);
        LogFormatter::setFileOutputEnabled(true);
        qCInfo(logNetwork()) << "in file 1";

        LogFormatter::setLogFilePath(path2);
        qCInfo(logNetwork()) << "in file 2";

        LogFormatter::uninstall();

        QString content1 = readFile(path1);
        QString content2 = readFile(path2);

        QVERIFY(content1.contains("in file 1"));
        QVERIFY(!content1.contains("in file 2"));
        QVERIFY(content2.contains("in file 2"));
        QVERIFY(!content2.contains("in file 1"));
    }

    // ==================================================================
    // 7. 子目录自动创建测试
    // ==================================================================
    void testSubDirectory_CreatedAutomatically()
    {
        QString subPath = logPath("subdir/nested/deep.log");
        LogFormatter::setLogFilePath(subPath);
        LogFormatter::setFileOutputEnabled(true);

        qCInfo(logNetwork()) << "in subdirectory";

        LogFormatter::uninstall();

        QVERIFY(QFile::exists(subPath));
    }
};

QTEST_GUILESS_MAIN(TestLogFormatter)
#include "tst_log_formatter.moc"
