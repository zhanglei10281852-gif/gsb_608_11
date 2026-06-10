#ifndef DATABASE_MODULE_H
#define DATABASE_MODULE_H

// ============================================================================
// 数据库模块 - 业务模块使用日志的示例
// ============================================================================

class DatabaseModule
{
public:
    // 模拟数据库连接
    void connect(const char *host, int port);

    // 模拟执行查询
    void executeQuery(const char *sql);

    // 模拟连接丢失
    void simulateConnectionLost();
};

#endif // DATABASE_MODULE_H
