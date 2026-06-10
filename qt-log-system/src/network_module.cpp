#include "network_module.h"
#include "log_categories.h"

void NetworkModule::sendRequest(const char *url)
{
    qCDebug(logNetwork) << "Preparing HTTP request to:" << url;
    qCInfo(logNetwork) << "Sending GET request to:" << url;
    // 模拟业务逻辑...
    qCDebug(logNetwork) << "Request headers set, awaiting response...";
}

void NetworkModule::handleResponse(int statusCode)
{
    qCDebug(logNetwork) << "Raw response received, status:" << statusCode;

    if (statusCode >= 200 && statusCode < 300) {
        qCInfo(logNetwork) << "Request successful, status:" << statusCode;
    } else if (statusCode >= 400 && statusCode < 500) {
        qCWarning(logNetwork) << "Client error, status:" << statusCode;
    } else if (statusCode >= 500) {
        qCCritical(logNetwork) << "Server error, status:" << statusCode;
    }
}

void NetworkModule::simulateTimeout()
{
    qCDebug(logNetwork) << "Connection attempt started...";
    qCWarning(logNetwork) << "Connection timeout after 30s";
    qCCritical(logNetwork) << "Failed to establish connection, all retries exhausted";
}
