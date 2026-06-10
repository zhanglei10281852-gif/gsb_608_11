#include "database_module.h"
#include "log_categories.h"

void DatabaseModule::connect(const char *host, int port)
{
    qCDebug(logDatabase) << "Resolving host:" << host << "port:" << port;
    qCInfo(logDatabase) << "Database connection established:" << host << ":" << port;
}

void DatabaseModule::executeQuery(const char *sql)
{
    qCDebug(logDatabase) << "Preparing SQL statement:" << sql;
    qCInfo(logDatabase) << "Query executed successfully, affected rows: 42";
    qCDebug(logDatabase) << "Query execution time: 12ms";
}

void DatabaseModule::simulateConnectionLost()
{
    qCWarning(logDatabase) << "Database connection unstable, attempting reconnect...";
    qCCritical(logDatabase) << "Database connection lost, reconnect failed after 3 attempts";
}
