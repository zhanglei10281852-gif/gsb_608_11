// ============================================================================
// LogFormatter 文件输出 + 按大小滚动 单元测试
// ============================================================================

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QString>
#include <QStringList>
#include <QLoggingCategory>
#include <QThread>

#include <thread>
#include <vector>

#include "log_formatter.h"

namespace {
Q_LOGGING_CATEGORY(logTest, "test.fmt")

// 读取文件全部内容（UTF-8）
QString readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll());
}

// 列出目录下所有 baseName.*.bak 归档
QFileInfoList listBackups(const QString &mainPath)
{
    QFileInfo fi(mainPath);
    QDir dir = fi.absoluteDir();
    QStringList filters{ fi.fileName() + QStringLiteral(".*.bak") };
    return dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot,
                             QDir::Time | QDir::Reversed);
}

// 生成一段有效负载，用于快速触达滚动阈值
QString payload(int n, char ch = 'A')
{
    return QString(n, QLatin1Char(ch));
}
}

class TestLogFormatter : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    QString       m_logPath;

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        LogFormatter::install();
    }

    void cleanupTestCase()
    {
        LogFormatter::disableFileOutput();
        LogFormatter::uninstall();
    }

    void init()
    {
        // 每个测试一个独立日志路径，避免互相影响
        static int counter = 0;
        ++counter;
        m_logPath = m_tmp.filePath(QStringLiteral("app-%1.log").arg(counter));
        // 确保关闭可能残留的文件输出
        LogFormatter::disableFileOutput();
    }

    void cleanup()
    {
        LogFormatter::disableFileOutput();
    }

    // ------------------------------------------------------------------
    // 1. 默认未启用文件输出
    // ------------------------------------------------------------------
    void testDefault_FileOutputDisabled()
    {
        QVERIFY(!LogFormatter::isFileOutputEnabled());
        QVERIFY(LogFormatter::currentLogFilePath().isEmpty());

        // 即使打日志也不会创建任何文件
        qCInfo(logTest) << "no-file-please";
        QVERIFY(!QFile::exists(m_logPath));
    }

    // ------------------------------------------------------------------
    // 2. 启用文件输出 - 文件被创建且内容包含期望字段
    // ------------------------------------------------------------------
    void testEnable_FileCreatedAndContentCorrect()
    {
        QVERIFY(LogFormatter::enableFileOutput(m_logPath,
                                               1024 * 1024,
                                               3));
        QVERIFY(LogFormatter::isFileOutputEnabled());
        QCOMPARE(LogFormatter::currentLogFilePath(), m_logPath);

        qCInfo(logTest) << "hello-file-output-12345";

        QVERIFY(QFile::exists(m_logPath));
        const QString content = readAll(m_logPath);
        QVERIFY2(content.contains("hello-file-output-12345"),
                 qPrintable(QStringLiteral("file content: ") + content));
        // 格式: [时间] [级别] [模块名] 消息
        QVERIFY(content.contains("[INFO"));
        QVERIFY(content.contains("[test.fmt"));
        QVERIFY(content.endsWith("\n"));
    }

    // ------------------------------------------------------------------
    // 3. 禁用文件输出后 - 不再写入新内容
    // ------------------------------------------------------------------
    void testDisable_StopsWritingButKeepsFile()
    {
        QVERIFY(LogFormatter::enableFileOutput(m_logPath, 1024 * 1024, 3));
        qCInfo(logTest) << "first-line-keeps";
        const qint64 sizeBefore = QFileInfo(m_logPath).size();
        QVERIFY(sizeBefore > 0);

        LogFormatter::disableFileOutput();
        QVERIFY(!LogFormatter::isFileOutputEnabled());

        qCInfo(logTest) << "should-not-be-in-file";
        const qint64 sizeAfter = QFileInfo(m_logPath).size();
        QCOMPARE(sizeAfter, sizeBefore);

        const QString content = readAll(m_logPath);
        QVERIFY(!content.contains("should-not-be-in-file"));
        QVERIFY(content.contains("first-line-keeps"));
    }

    // ------------------------------------------------------------------
    // 4. 重新启用 - 同一文件路径采用追加模式（保留历史内容）
    // ------------------------------------------------------------------
    void testReEnable_AppendsRatherThanTruncates()
    {
        QVERIFY(LogFormatter::enableFileOutput(m_logPath, 1024 * 1024, 3));
        qCInfo(logTest) << "round-1-data";
        LogFormatter::disableFileOutput();

        QVERIFY(LogFormatter::enableFileOutput(m_logPath, 1024 * 1024, 3));
        qCInfo(logTest) << "round-2-data";
        LogFormatter::disableFileOutput();

        const QString content = readAll(m_logPath);
        QVERIFY(content.contains("round-1-data"));
        QVERIFY(content.contains("round-2-data"));
    }

    // ------------------------------------------------------------------
    // 5. 超过阈值触发滚动 - 主文件被归档，重新写入新主文件
    // ------------------------------------------------------------------
    void testRotation_TriggeredOnSizeExceeded()
    {
        // 阈值设为 1KB，方便快速触发
        const qint64 maxSize = 1024;
        QVERIFY(LogFormatter::enableFileOutput(m_logPath, maxSize, 5));

        // 写入一条远大于阈值的消息，必然触发一次滚动
        qCInfo(logTest) << payload(2000, 'X');

        const QFileInfoList backups = listBackups(m_logPath);
        QCOMPARE(backups.size(), 1);
        // 备份文件应该非空且名字符合 baseName.*.bak
        QVERIFY(backups.first().size() > 0);
        QVERIFY(backups.first().fileName().endsWith(".bak"));

        // 主文件存在（重新创建），但应该比阈值小很多（甚至为 0）
        QVERIFY(QFile::exists(m_logPath));
        QVERIFY(QFileInfo(m_logPath).size() < maxSize);
    }

    // ------------------------------------------------------------------
    // 6. 历史归档数量受 maxBackupFiles 限制
    // ------------------------------------------------------------------
    void testRotation_BackupCountCapped()
    {
        const qint64 maxSize = 256;
        const int    maxBackups = 3;
        QVERIFY(LogFormatter::enableFileOutput(m_logPath, maxSize, maxBackups));

        // 触发 N (>>3) 次滚动；每条消息都超过阈值
        for (int i = 0; i < 8; ++i) {
            qCInfo(logTest) << payload(400, 'A' + (i % 26));
            // 短暂 sleep 让备份文件 mtime 有差异，便于按时间排序裁剪
            QThread::msleep(5);
        }

        const QFileInfoList backups = listBackups(m_logPath);
        QVERIFY2(backups.size() <= maxBackups,
                 qPrintable(QStringLiteral("backups=%1 max=%2").arg(backups.size()).arg(maxBackups)));
        QCOMPARE(backups.size(), maxBackups);
    }

    // ------------------------------------------------------------------
    // 7. maxBackupFiles=0 时不保留任何归档
    // ------------------------------------------------------------------
    void testRotation_ZeroBackupFilesKeepsNothing()
    {
        const qint64 maxSize = 256;
        QVERIFY(LogFormatter::enableFileOutput(m_logPath, maxSize, 0));

        for (int i = 0; i < 4; ++i) {
            qCInfo(logTest) << payload(400, 'Z');
            QThread::msleep(2);
        }

        const QFileInfoList backups = listBackups(m_logPath);
        QCOMPARE(backups.size(), 0);
    }

    // ------------------------------------------------------------------
    // 8. 归档文件命名符合规则：以 .bak 结尾
    // ------------------------------------------------------------------
    void testRotation_BackupNamingPattern()
    {
        QVERIFY(LogFormatter::enableFileOutput(m_logPath, 256, 5));
        qCInfo(logTest) << payload(500, 'P');
        QThread::msleep(5);
        qCInfo(logTest) << payload(500, 'Q');

        const QFileInfoList backups = listBackups(m_logPath);
        QVERIFY(backups.size() >= 1);
        const QString mainName = QFileInfo(m_logPath).fileName();
        for (const QFileInfo &fi : backups) {
            QVERIFY2(fi.fileName().startsWith(mainName + "."),
                     qPrintable(fi.fileName()));
            QVERIFY2(fi.fileName().endsWith(".bak"),
                     qPrintable(fi.fileName()));
        }
    }

    // ------------------------------------------------------------------
    // 9. 多线程写入安全：内容不交错且总行数正确
    // ------------------------------------------------------------------
    void testThreadSafety_ConcurrentWrites()
    {
        // 不滚动，只校验并发写入不交错（maxFileSize 大到不会触发）
        QVERIFY(LogFormatter::enableFileOutput(m_logPath,
                                               64 * 1024 * 1024,
                                               5));

        constexpr int kThreads = 4;
        constexpr int kPerThread = 100;
        const QString tag = QStringLiteral("MTLINE");

        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            workers.emplace_back([t]() {
                for (int i = 0; i < kPerThread; ++i) {
                    qCInfo(logTest).noquote()
                        << QStringLiteral("MTLINE t=%1 i=%2").arg(t).arg(i);
                }
            });
        }
        for (auto &th : workers) th.join();
        LogFormatter::disableFileOutput();

        const QString content = readAll(m_logPath);
        const QStringList lines = content.split('\n', Qt::SkipEmptyParts);
        int matches = 0;
        for (const QString &line : lines) {
            if (line.contains(tag)) ++matches;
            // 每行必须以时间戳 '[' 起头，且包含级别/模块标记，行内不应出现重复 '\r'/'\n'
            QVERIFY2(line.startsWith('['), qPrintable(line));
            QVERIFY2(line.contains("[INFO"), qPrintable(line));
            QVERIFY2(line.contains("[test.fmt"), qPrintable(line));
        }
        QCOMPARE(matches, kThreads * kPerThread);
    }

    // ------------------------------------------------------------------
    // 10. enableFileOutput 路径为空 -> 失败且不影响 stderr
    // ------------------------------------------------------------------
    void testEnable_EmptyPathReturnsFalse()
    {
        QVERIFY(!LogFormatter::enableFileOutput(QString(), 1024, 1));
        QVERIFY(!LogFormatter::isFileOutputEnabled());
    }

    // ------------------------------------------------------------------
    // 11. 自动创建父目录
    // ------------------------------------------------------------------
    void testEnable_CreatesParentDirectory()
    {
        const QString nestedPath = m_tmp.filePath("nested/deep/dir/app.log");
        QVERIFY(LogFormatter::enableFileOutput(nestedPath, 1024 * 1024, 3));
        qCInfo(logTest) << "nested-dir-write";
        LogFormatter::disableFileOutput();
        QVERIFY(QFile::exists(nestedPath));
        const QString content = readAll(nestedPath);
        QVERIFY(content.contains("nested-dir-write"));
    }
};

QTEST_GUILESS_MAIN(TestLogFormatter)
#include "tst_log_formatter.moc"
