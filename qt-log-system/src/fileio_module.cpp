#include "fileio_module.h"
#include "log_categories.h"

void FileIOModule::readFile(const char *path)
{
    qCDebug(logFileIO) << "Opening file for reading:" << path;
    qCInfo(logFileIO) << "File read complete:" << path << "(2048 bytes)";
}

void FileIOModule::writeFile(const char *path)
{
    qCDebug(logFileIO) << "Opening file for writing:" << path;
    qCInfo(logFileIO) << "File write complete:" << path << "(1024 bytes)";
    qCDebug(logFileIO) << "File handle closed:" << path;
}

void FileIOModule::simulateDiskFull()
{
    qCWarning(logFileIO) << "Disk space low, remaining: 50MB";
    qCCritical(logFileIO) << "Disk full, write operation aborted";
}
