#ifndef FILEIO_MODULE_H
#define FILEIO_MODULE_H

// ============================================================================
// 文件IO模块 - 业务模块使用日志的示例
// ============================================================================

class FileIOModule
{
public:
    // 模拟读取文件
    void readFile(const char *path);

    // 模拟写入文件
    void writeFile(const char *path);

    // 模拟磁盘空间不足
    void simulateDiskFull();
};

#endif // FILEIO_MODULE_H
