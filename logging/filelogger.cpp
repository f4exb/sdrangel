/**
 @file
 @author Stefan Frings
 */

#include "filelogger.h"
#include <QTime>
#include <QStringList>
#include <QThread>
#include <QtGlobal>
#include <QFile>
#include <QTimerEvent>
#include <QDir>
#include <QFileInfo>
#include <stdio.h>

using namespace qtwebapp;

void FileLogger::refreshSettings()
{
    mutex.lock();

    if (useQtSettings) {
        refreshQtSettings();
    } else {
        refreshFileLogSettings();
    }

    mutex.unlock();
}

void FileLogger::refreshQtSettings()
{
    // Save old file name for later comparision with new settings
    QString oldFileName = fileName;

    // Load new config settings
    settings->sync();
    fileName = settings->value("fileName").toString();

    // Convert relative fileName to absolute, based on the directory of the config file.
#ifdef Q_OS_WIN32
    if (QDir::isRelativePath(fileName) && settings->format()!=QSettings::NativeFormat)
#else
    if (QDir::isRelativePath(fileName))
#endif
    {
        QFileInfo configFile(settings->fileName());
        fileName = QFileInfo(configFile.absolutePath(), fileName).absoluteFilePath();
    }

    maxSize = settings->value("maxSize", 0).toLongLong();
    maxBackups = settings->value("maxBackups", 0).toInt();
    msgFormat = settings->value("msgFormat", "{timestamp} {type} {msg}").toString();
    timestampFormat = settings->value("timestampFormat", "yyyy-MM-dd hh:mm:ss.zzz").toString();
    minLevel = static_cast<QtMsgType>(settings->value("minLevel", 0).toInt());
    bufferSize = settings->value("bufferSize", 0).toInt();

    // Create new file if the filename has been changed
    if (oldFileName != fileName)
    {
        fprintf(stderr, "FileLogger::refreshQtSettings: Logging to %s\n", qPrintable(fileName));
        close();
        open();
    }
}

void FileLogger::refreshFileLogSettings()
{
    // Save old file name for later comparision with new settings
    QString oldFileName = fileName;

    // Load new config settings
    fileName = fileLoggerSettings.fileName;

    // Convert relative fileName to absolute, based on the current working directory
    if (QDir::isRelativePath(fileName)) {
        fileName = QFileInfo(QDir::currentPath(), fileName).absoluteFilePath();
    }

    maxSize = fileLoggerSettings.maxSize;
    maxBackups = fileLoggerSettings.maxBackups;
    msgFormat = fileLoggerSettings.msgFormat;
    timestampFormat = fileLoggerSettings.timestampFormat;
    minLevel = fileLoggerSettings.minLevel;
    bufferSize = fileLoggerSettings.bufferSize;

    // Create new file if the filename has been changed
    if (oldFileName != fileName)
    {
        fprintf(stderr, "FileLogger::refreshQtSettings: Logging to new file %s\n", qPrintable(fileName));
        close();
        open();
    }
}

FileLogger::FileLogger(QSettings* settings, const int refreshInterval, QObject* parent) :
        Logger(parent), fileName(""), useQtSettings(true)
{
    Q_ASSERT(settings != 0);
    Q_ASSERT(refreshInterval >= 0);

    this->settings = settings;
    file = 0;

    if (refreshInterval > 0) {
        refreshTimer.start(refreshInterval, this);
    }

    flushTimer.start(1000, this);

    refreshSettings();
}

FileLogger::FileLogger(const FileLoggerSettings& settings, const int refreshInterval, QObject* parent) :
        Logger(parent), fileName(""), useQtSettings(false)
{
    Q_ASSERT(refreshInterval >= 0);

    fileLoggerSettings = settings;
    file = 0;

    if (refreshInterval > 0) {
        refreshTimer.start(refreshInterval, this);
    }

    flushTimer.start(1000, this);

    refreshSettings();
}

FileLogger::~FileLogger()
{
    close();
}

void FileLogger::write(const LogMessage* logMessage)
{
    // Try to write to the file
    if (file)
    {
        // Write the message
        file->write(qPrintable(logMessage->toString(msgFormat, timestampFormat)));

        // Flush error messages immediately, to ensure that no important message
        // gets lost when the program terinates abnormally.
        if (logMessage->getType() >= QtCriticalMsg) {
            file->flush();
        }

        // Check for success
        if (file->error())
        {
            close();
            fprintf(stderr, "FileLogger::write: Cannot write to log file %s: %s\n", qPrintable(fileName), qPrintable(file->errorString()));
        }
    }

    // Fall-back to the super class method, if writing failed
    if (!file && useQtSettings) {
        Logger::write(logMessage);
    }

}

void FileLogger::open()
{
    if (fileName.isEmpty())
    {
        fprintf(stderr, "FileLogger::open: Name of logFile is empty\n");
    }
    else
    {
        file = new QFile(fileName);

        if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            fprintf(stderr, "FileLogger::open: Cannot open log file %s: %s\n", qPrintable(fileName), qPrintable(file->errorString()));
            file = 0;
        }
        else
        {
            fprintf(stderr, "FileLogger::open: Opened log file %s\n", qPrintable(fileName));
        }
    }
}

void FileLogger::close()
{
    if (file)
    {
        file->close();
        delete file;
        file = 0;
    }
}

void FileLogger::rotate()
{
    fprintf(stderr, "FileLogger::rotate\n");
    // count current number of existing backup files
    int count = 0;

    forever
    {
        QFile bakFile(QString("%1.%2").arg(fileName).arg(count + 1));

        if (bakFile.exists()) {
            ++count;
        } else {
            break;
        }
    }

    // Remove all old backup files that exceed the maximum number
    while (maxBackups > 0 && count >= maxBackups)
    {
        QFile::remove(QString("%1.%2").arg(fileName).arg(count));
        --count;
    }

    // Rotate backup files
    for (int i = count; i > 0; --i)
    {
        QFile::rename(QString("%1.%2").arg(fileName).arg(i), QString("%1.%2").arg(fileName).arg(i + 1));
    }

    // Backup the current logfile
    QFile::rename(fileName, fileName + ".1");
}

void FileLogger::timerEvent(QTimerEvent* event)
{
    if (!event) {
        return;
    } else if (event->timerId() == refreshTimer.timerId()) {
        refreshSettings();
    }
    else if (event->timerId() == flushTimer.timerId() && file)
    {
        mutex.lock();

        // Flush the I/O buffer
        file->flush();

        // Rotate the file if it is too large
        if (maxSize > 0 && file->size() >= maxSize)
        {
            close();
            rotate();
            open();
        }

        mutex.unlock();
    }
}
