/*
 * fileloggersettings.h
 *
 *  Created on: Nov 11, 2017
 *      Author: f4exb
 */

#ifndef LOGGING_FILELOGGERSETTINGS_H_
#define LOGGING_FILELOGGERSETTINGS_H_

#include <QtGlobal>

namespace qtwebapp {

struct FileLoggerSettings
{
    QString   fileName;
    long      maxSize;
    int       maxBackups;
    QString   msgFormat;
    QString   timestampFormat;
    QtMsgType minLevel;
    int       bufferSize;

    FileLoggerSettings() {
        resetToDefaults();
    }

    void resetToDefaults() {
        fileName = "logging.log";
        maxSize = 1000000;
        maxBackups = 2;
        msgFormat = "{timestamp} {type} {msg}";
        timestampFormat = "dd.MM.yyyy hh:mm:ss.zzz";
        minLevel = QtDebugMsg;
        bufferSize = 100;
    }
};

} // end of namespace

#endif /* LOGGING_FILELOGGERSETTINGS_H_ */
