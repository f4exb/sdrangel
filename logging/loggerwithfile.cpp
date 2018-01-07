/*
 * loggerwithfile.cpp
 *
 *  Created on: Nov 11, 2017
 *      Author: f4exb
 */

#include "loggerwithfile.h"

using namespace qtwebapp;

LoggerWithFile::LoggerWithFile(QObject* parent)
    :Logger(parent), fileLogger(0), useFileFlogger(false)
{
     consoleLogger = new Logger(this);
}

LoggerWithFile::~LoggerWithFile()
{
    destroyFileLogger();
    delete consoleLogger;
}

void LoggerWithFile::createOrSetFileLogger(const FileLoggerSettings& settings, const int refreshInterval)
{
    if (!fileLogger) {
        fileLogger = new FileLogger(settings, refreshInterval, this);
    } else {
        fileLogger->setFileLoggerSettings(settings);
    }
}

void LoggerWithFile::destroyFileLogger()
{
    if (fileLogger)
    {
        delete fileLogger;
        fileLogger = 0;
    }
}

void LoggerWithFile::log(const QtMsgType type, const QString& message, const QString &file, const QString &function, const int line)
{
    consoleLogger->log(type,message,file,function,line);

    if (fileLogger && useFileFlogger) {
        fileLogger->log(type,message,file,function,line);
    }
}

void LoggerWithFile::logToFile(const QtMsgType type, const QString& message, const QString &file, const QString &function, const int line)
{
    if (fileLogger && useFileFlogger) {
        fileLogger->log(type,message,file,function,line);
    }
}

void LoggerWithFile::clear(const bool buffer, const bool variables)
{
    consoleLogger->clear(buffer,variables);

    if (fileLogger && useFileFlogger) {
        fileLogger->clear(buffer,variables);
    }
}

void LoggerWithFile::setConsoleMinMessageLevel(const QtMsgType& msgLevel)
{
    consoleLogger->setMinMessageLevel(msgLevel);
}

void LoggerWithFile::setFileMinMessageLevel(const QtMsgType& msgLevel)
{
    if (fileLogger) {
        fileLogger->setMinMessageLevel(msgLevel);
    }
}

void LoggerWithFile::getConsoleMinMessageLevelStr(QString& levelStr)
{
    switch (consoleLogger->getMinMessageLevel())
    {
    case QtDebugMsg:
        levelStr = "debug";
        break;
    case QtInfoMsg:
        levelStr = "info";
        break;
    case QtWarningMsg:
        levelStr = "warning";
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        levelStr = "error";
        break;
    default:
        levelStr = "debug";
        break;
    }
}

void LoggerWithFile::getFileMinMessageLevelStr(QString& levelStr)
{
    switch (fileLogger->getMinMessageLevel())
    {
    case QtDebugMsg:
        levelStr = "debug";
        break;
    case QtInfoMsg:
        levelStr = "info";
        break;
    case QtWarningMsg:
        levelStr = "warning";
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        levelStr = "error";
        break;
    default:
        levelStr = "debug";
        break;
    }
}

void LoggerWithFile::getLogFileName(QString& fileName)
{
    fileName = fileLogger->getFileLoggerSettings().fileName;
}
