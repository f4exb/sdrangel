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

void LoggerWithFile::createFileLogger(const FileLoggerSettings& settings, const int refreshInterval)
{
     fileLogger = new FileLogger(settings, refreshInterval, this);
}

void LoggerWithFile::log(const QtMsgType type, const QString& message, const QString &file, const QString &function, const int line)
{
    consoleLogger->log(type,message,file,function,line);

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

void LoggerWithFile::setMinMessageLevel(QtMsgType& msgLevel)
{
    consoleLogger->setMinMessageLevel(msgLevel);

    if (fileLogger) {
        fileLogger->setMinMessageLevel(msgLevel);
    }
}
