#ifndef NETWORK_MODULE_H
#define NETWORK_MODULE_H

// ============================================================================
// 网络模块 - 业务模块使用日志的示例
// ============================================================================

class NetworkModule
{
public:
    // 模拟发送 HTTP 请求
    void sendRequest(const char *url);

    // 模拟处理响应
    void handleResponse(int statusCode);

    // 模拟连接超时
    void simulateTimeout();
};

#endif // NETWORK_MODULE_H
