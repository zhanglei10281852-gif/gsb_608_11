// ============================================================================
// LogFormatter 单元测试 - 文件输出 & 大小滚动
// 基于 QTest 框架，覆盖文件写入、格式、滚动切割、历史文件数量限制等场景
// ============================================================================

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "log_categories.h"
#include "log_formatter.h"

Q_LOGGING_CATEGORY(logTest, "app.test")

class TestLogFormatter : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    void removeAllLogs(const QString &basePath)
    {
        QFile::remove(basePath);
        for (int i = 1; i <= 100; ++i) {
            QFile::remove(QStringLiteral("%1.%2").arg(basePath).arg(i));
        }
    }

    int countLogFiles(const QString &basePath)
    {
        int n = 0;
        if (QFile::exists(basePath)) ++n;
        for (int i = 1; i <= 100; ++i) {
            if (QFile::exists(QStringLiteral("%1.%2").arg(basePath).arg(i)))
                ++n;
        }
        return n;
    }

    QString readFile(const QString &p)
    {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
        return QString::fromUtf8(f.readAll());
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(m_tempDir.isValid(), "Failed to create temporary directory");
        LogFormatter::install();
    }

    void cleanupTestCase()
    {
        LogFormatter::disableFileOutput();
        LogFormatter::uninstall();
    }

    void init()
    {
        LogFormatter::disableFileOutput();
    }

    // ==================================================================
    // 1. 默认状态下不创建文件
    // ==================================================================
    void testDefault_NoFileCreated()
    {
        QString path = m_tempDir.filePath("default.log");
        removeAllLogs(path);

        qCInfo(logTest()) << "this should not create a file";

        QVERIFY2(!QFile::exists(path),
                 "Log file should NOT exist when file output is disabled");
    }

    // ==================================================================
    // 2. 启用文件输出后，文件存在且包含日志内容
    // ==================================================================
    void testEnableFile_FileExistsAndHasContent()
    {
        QString path = m_tempDir.filePath("enable.log");
        removeAllLogs(path);

        LogFormatter::enableFileOutput(path, 10 * 1024 * 1024, 5);

        const QString magic = "UNIQUE-MAGIC-STRING-abc123";
        qCInfo(logTest()) << magic;

        LogFormatter::disableFileOutput();

        QVERIFY2(QFile::exists(path), "Log file should exist after enabling file output");

        QString content = readFile(path);

        QVERIFY2(content.contains(magic),
                 qPrintable(QString("Log file should contain '%1', got:\n%2").arg(magic, content)));
    }

    // ==================================================================
    // 3. 文件输出格式与 stderr 格式一致:
    //    [yyyy-MM-dd hh:mm:ss.zzz] [LEVEL   ] [category       ] message
    // ==================================================================
    void testFileOutput_FormatMatchesSpecification()
    {
        QString path = m_tempDir.filePath("format.log");
        removeAllLogs(path);

        LogFormatter::enableFileOutput(path, 10 * 1024 * 1024, 5);

        qCDebug(logTest()) << "format-check-debug";
        qCWarning(logTest()) << "format-check-warning";

        LogFormatter::disableFileOutput();

        QString content = readFile(path);

        QRegularExpression re(
            "\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\] "
            "\\[DEBUG   \\] \\[app\\.test       \\] format-check-debug"
        );
        QVERIFY2(re.match(content).hasMatch(),
                 qPrintable(QString("Debug line format mismatch, content:\n%1").arg(content)));

        QRegularExpression re2(
            "\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\] "
            "\\[WARNING \\] \\[app\\.test       \\] format-check-warning"
        );
        QVERIFY2(re2.match(content).hasMatch(),
                 qPrintable(QString("Warning line format mismatch, content:\n%1").arg(content)));
    }

    // ==================================================================
    // 4. 超过大小阈值触发滚动切割
    // ==================================================================
    void testRotate_TriggeredWhenSizeExceeded()
    {
        QString path = m_tempDir.filePath("rotate.log");
        removeAllLogs(path);

        const qint64 maxSize = 2000;
        LogFormatter::enableFileOutput(path, maxSize, 5);

        for (int i = 0; i < 100; ++i) {
            qCInfo(logTest()) << QString("padding-line-with-enough-content-%1").arg(i);
        }

        LogFormatter::disableFileOutput();

        QVERIFY2(QFile::exists(path), "Current log file should exist after rotation");
        QVERIFY2(QFile::exists(path + ".1"),
                 "Rotated archive .1 should exist after size exceeded");

        QFileInfo fi(path);
        QVERIFY2(fi.size() < maxSize + 1000,
                 qPrintable(QString("Current log size should be under limit, got %1")
                                .arg(fi.size())));
    }

    // ==================================================================
    // 5. 历史文件数量被正确限制（超出 maxBackups 的最旧文件被删除）
    // ==================================================================
    void testRotate_MaxBackupCountEnforced()
    {
        QString path = m_tempDir.filePath("rotate-limit.log");
        removeAllLogs(path);

        const int maxBackups = 3;
        const qint64 maxSize = 500;
        LogFormatter::enableFileOutput(path, maxSize, maxBackups);

        for (int round = 0; round < 20; ++round) {
            for (int i = 0; i < 20; ++i) {
                qCInfo(logTest()) << QString("round-%1-line-with-padding-%2").arg(round).arg(i);
            }
        }

        LogFormatter::disableFileOutput();

        int total = countLogFiles(path);
        QCOMPARE(total, maxBackups + 1);

        QVERIFY2(QFile::exists(path), "Current log must exist");
        QVERIFY2(QFile::exists(path + ".1"), "Backup .1 must exist");
        QVERIFY2(QFile::exists(path + ".2"), "Backup .2 must exist");
        QVERIFY2(QFile::exists(path + ".3"), "Backup .3 must exist");
        QVERIFY2(!QFile::exists(path + ".4"), "Backup .4 should NOT exist (exceeds maxBackups=3)");
    }

    // ==================================================================
    // 6. 归档文件命名顺序正确: .1 最新，数字越大越旧
    // ==================================================================
    void testRotate_NumberingOrderCorrect()
    {
        QString path = m_tempDir.filePath("order.log");
        removeAllLogs(path);

        const qint64 chunkSize = 5000;
        LogFormatter::enableFileOutput(path, chunkSize, 3);

        auto writeMarker = [&](const QString &marker) {
            qCInfo(logTest()) << marker.toUtf8().constData();
        };
        auto fillExactlyUntilAlmostFull = [&]() {
            const int approxLineLen = 80;
            const int linesNeeded = static_cast<int>(chunkSize / approxLineLen) - 5;
            for (int i = 0; i < linesNeeded; ++i) {
                QByteArray padding(40, 'X');
                qCInfo(logTest()) << padding.constData();
            }
        };

        writeMarker("FIRST-BATCH-OLDEST");
        fillExactlyUntilAlmostFull();

        writeMarker("SECOND-BATCH");
        fillExactlyUntilAlmostFull();

        writeMarker("THIRD-BATCH");
        fillExactlyUntilAlmostFull();

        writeMarker("FOURTH-BATCH-NEWEST");

        LogFormatter::disableFileOutput();

        QString c3 = readFile(path + ".3");
        QString c2 = readFile(path + ".2");
        QString c1 = readFile(path + ".1");
        QString cur = readFile(path);

        QVERIFY2(c3.contains("FIRST-BATCH-OLDEST"),
                 qPrintable(QString(".3 should contain oldest batch, got length: %1\nTail:\n%2").arg(c3.size()).arg(c3.right(1500))));
        QVERIFY2(c2.contains("SECOND-BATCH"),
                 qPrintable(QString(".2 should contain second batch, got length: %1\nTail:\n%2").arg(c2.size()).arg(c2.right(1500))));
        QVERIFY2(c1.contains("THIRD-BATCH"),
                 qPrintable(QString(".1 should contain third batch, got length: %1\nTail:\n%2").arg(c1.size()).arg(c1.right(1500))));
        QVERIFY2(cur.contains("FOURTH-BATCH-NEWEST"),
                 qPrintable(QString("current should contain newest batch, got:\n%1").arg(cur)));
    }

    // ==================================================================
    // 7. disableFileOutput() 后停止写入文件（但 stderr 不受影响）
    // ==================================================================
    void testDisableFile_StopsWriting()
    {
        QString path = m_tempDir.filePath("disable.log");
        removeAllLogs(path);

        LogFormatter::enableFileOutput(path, 10 * 1024 * 1024, 5);
        qCInfo(logTest()) << "before-disable";
        LogFormatter::disableFileOutput();

        QString before = readFile(path);

        qCInfo(logTest()) << "after-disable";

        QString after = readFile(path);

        QCOMPARE(before, after);
        QVERIFY2(!after.contains("after-disable"),
                 "Log written after disableFileOutput() must NOT appear in file");
    }

    // ==================================================================
    // 8. setFileConfig 结构体接口工作正常
    // ==================================================================
    void testSetFileConfig_StructInterface()
    {
        QString path = m_tempDir.filePath("struct-cfg.log");
        removeAllLogs(path);

        LogFormatter::FileConfig cfg;
        cfg.enabled = true;
        cfg.filePath = path;
        cfg.maxFileSize = 10 * 1024 * 1024;
        cfg.maxBackups = 3;
        LogFormatter::setFileConfig(cfg);

        qCInfo(logTest()) << "via-struct-config";

        LogFormatter::disableFileOutput();

        QString content = readFile(path);

        QVERIFY(content.contains("via-struct-config"));
    }

    // ==================================================================
    // 9. 多行日志全部写入，无丢失
    // ==================================================================
    void testMultiWrite_AllLinesPresent()
    {
        QString path = m_tempDir.filePath("interleave.log");
        removeAllLogs(path);

        LogFormatter::enableFileOutput(path, 10 * 1024 * 1024, 5);

        const int n = 100;
        for (int i = 0; i < n; ++i) {
            qCInfo(logTest()) << QString("SEQUENTIAL-LINE-%1").arg(i);
        }

        LogFormatter::disableFileOutput();

        QString content = readFile(path);

        int found = 0;
        for (int i = 0; i < n; ++i) {
            if (content.contains(QString("SEQUENTIAL-LINE-%1").arg(i)))
                ++found;
        }
        QCOMPARE(found, n);
    }

    // ==================================================================
    // 10. maxBackups=0 时滚动直接清空，不保留归档
    // ==================================================================
    void testZeroBackups_NoArchiveKept()
    {
        QString path = m_tempDir.filePath("zero.log");
        removeAllLogs(path);

        LogFormatter::enableFileOutput(path, 50, 0);

        for (int i = 0; i < 50; ++i) {
            qCInfo(logTest()) << QString("zero-test-line-with-padding-%1").arg(i);
        }

        LogFormatter::disableFileOutput();

        QVERIFY2(!QFile::exists(path + ".1"),
                 "With maxBackups=0, no .1 archive should exist");
    }
};

QTEST_GUILESS_MAIN(TestLogFormatter)
#include "tst_log_formatter.moc"
