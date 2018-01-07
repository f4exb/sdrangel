/*
 * loggerwithfile.h
 *
 *  Created on: Nov 11, 2017
 *      Author: f4exb
 */

#ifndef LOGGING_LOGGERWITHFILE_H_
#define LOGGING_LOGGERWITHFILE_H_

#include <QtGlobal>
#include "logger.h"
#include "filelogger.h"

namespace qtwebapp {

/**
  Logs messages to console and optionally to a file simultaneously.
  @see FileLogger for a description of the two underlying file logger.
  @see Logger for a description of the console loger.
*/

class DECLSPEC LoggerWithFile : public Logger {
    Q_OBJECT
    Q_DISABLE_COPY(LoggerWithFile)

public:
    LoggerWithFile(QObject *parent = 0);
    virtual ~LoggerWithFile();

    void createOrSetFileLogger(const FileLoggerSettings& settings, const int refreshInterval=10000);
    void destroyFileLogger();

    /**
      Decorate and log the message, if type>=minLevel.
      This method is thread safe.
      @param type Message type (level)
      @param message Message text
      @param file Name of the source file where the message was generated (usually filled with the macro __FILE__)
      @param function Name of the function where the message was generated (usually filled with the macro __LINE__)
      @param line Line Number of the source file, where the message was generated (usually filles with the macro __func__ or __FUNCTION__)
      @see LogMessage for a description of the message decoration.
    */
    virtual void log(const QtMsgType type, const QString& message, const QString &file="", const QString &function="", const int line=0);

    /**
      Clear the thread-local data of the current thread.
      This method is thread safe.
      @param buffer Whether to clear the backtrace buffer
      @param variables Whether to clear the log variables
    */
    virtual void clear(const bool buffer=true, const bool variables=true);

    bool getUseFileLogger() const { return useFileFlogger; }
    void setUseFileLogger(bool use) { useFileFlogger = use; }
    bool hasFileLogger() const { return fileLogger != 0; }

    /**
     * Get a file logger settings copy
     * @return The current file logger settings
     */
    FileLoggerSettings getFileLoggerSettings() const { return fileLogger->getFileLoggerSettings(); }

    /**
     * Set new file logger settings data
     * @param File logger settings to replace current data
     */
    void setFileLoggerSettings(const FileLoggerSettings& settings) { fileLogger->setFileLoggerSettings(settings); }

    void setConsoleMinMessageLevel(const QtMsgType& msgLevel);
    void setFileMinMessageLevel(const QtMsgType& msgLevel);

    void getConsoleMinMessageLevelStr(QString& levelStr);
    void getFileMinMessageLevelStr(QString& levelStr);
    void getLogFileName(QString& fileName);

    /** This will log to file only */
    void logToFile(const QtMsgType type, const QString& message, const QString &file="", const QString &function="", const int line=0);

private:
    /** First console logger */
    Logger* consoleLogger;

    /** Second file logger */
    FileLogger* fileLogger;

    /** Use file logger indicator */
    bool useFileFlogger;
};

} // end of namespace

#endif /* LOGGING_LOGGERWITHFILE_H_ */
