/**
  @file
  @author Stefan Frings
*/

#ifndef FILELOGGER_H
#define FILELOGGER_H

#include <QtGlobal>
#include <QSettings>
#include <QFile>
#include <QMutex>
#include <QBasicTimer>
#include "logglobal.h"
#include "logger.h"
#include "fileloggersettings.h"

namespace qtwebapp {

/**
  Logger that uses a text file for output. Settings are read from a
  config file using a QSettings object. Config settings can be changed at runtime.
  <p>
  Example for the configuration settings:
  <code><pre>
  fileName=logs/QtWebApp.log
  maxSize=1000000
  maxBackups=2
  minLevel=0
  msgformat={timestamp} {typeNr} {type} thread={thread}: {msg}
  timestampFormat=dd.MM.yyyy hh:mm:ss.zzz
  bufferSize=0
  </pre></code>

  - fileName is the name of the log file, relative to the directory of the settings file.
    In case of windows, if the settings are in the registry, the path is relative to the current
    working directory.
  - maxSize is the maximum size of that file in bytes. The file will be backed up and
    replaced by a new file if it becomes larger than this limit. Please note that
    the actual file size may become a little bit larger than this limit. Default is 0=unlimited.
  - maxBackups defines the number of backup files to keep. Default is 0=unlimited.
  - minLevel defines the minimum type of messages that are written (together with buffered messages) into the file. Defaults is 0=debug.
  - msgFormat defines the decoration of log messages, see LogMessage class. Default is "{timestamp} {type} {msg}".
  - timestampFormat defines the format of timestamps, see QDateTime::toString(). Default is "yyyy-MM-dd hh:mm:ss.zzz".
  - bufferSize defines the size of the buffer. Default is 0=disabled.

  @see set() describes how to set logger variables
  @see LogMessage for a description of the message decoration.
  @see Logger for a descrition of the buffer.
*/

class DECLSPEC FileLogger : public Logger {
    Q_OBJECT
    Q_DISABLE_COPY(FileLogger)
public:

    /**
      Constructor.
      @param settings Configuration settings, usually stored in an INI file. Must not be 0.
      Settings are read from the current group, so the caller must have called settings->beginGroup().
      Because the group must not change during runtime, it is recommended to provide a
      separate QSettings instance to the logger that is not used by other parts of the program.
      @param refreshInterval Interval of checking for changed config settings in msec, or 0=disabled
      @param parent Parent object
    */
    FileLogger(QSettings* settings, const int refreshInterval=10000, QObject* parent = 0);

    /**
      Constructor.
      @param settings Configuration settings as a simple structure (see: FileLoggerSettings).
      @param refreshInterval Interval of checking for changed config settings in msec, or 0=disabled
      @param parent Parent object
    */
    FileLogger(const FileLoggerSettings& settings, const int refreshInterval=10000, QObject* parent = 0);

    /**
      Destructor. Closes the file.
    */
    virtual ~FileLogger();

    /**
     * Get a file logger settings copy
     * @return The current file logger settings
     */
    FileLoggerSettings getFileLoggerSettings() const { return fileLoggerSettings; }

    /**
     * Set new file logger settings data
     * @param File logger settings to replace current data
     */
    void setFileLoggerSettings(const FileLoggerSettings& settings) { fileLoggerSettings = settings; }

    /** Write a message to the log file */
    virtual void write(const LogMessage* logMessage);

protected:

    /**
      Handler for timer events.
      Refreshes config settings or synchronizes I/O buffer, depending on the event.
      This method is thread-safe.
      @param event used to distinguish between the two timers.
    */
    void timerEvent(QTimerEvent* event);

private:

    /** Configured name of the log file */
    QString fileName;

    /** Configured  maximum size of the file in bytes, or 0=unlimited */
    long maxSize;

    /** Configured maximum number of backup files, or 0=unlimited */
    int maxBackups;

    /** Pointer to the configuration settings */
    QSettings* settings;

    /** Pointer to the configuration settings */
    FileLoggerSettings fileLoggerSettings;

    /** Output file, or 0=disabled */
    QFile* file;

    /** Timer for refreshing configuration settings */
    QBasicTimer refreshTimer;

    /** Timer for flushing the file I/O buffer */
    QBasicTimer flushTimer;

    /** Use of Qt settings or simple structure settings */
    bool useQtSettings;

    /** Open the output file */
    void open();

    /** Close the output file */
    void close();

    /** Rotate files and delete some backups if there are too many */
    void rotate();

    /**
      Refreshes the configuration settings.
      This method is thread-safe.
    */
    void refreshSettings();

    /**
      Refreshes the configuration settings with Qt settings.
    */
    void refreshQtSettings();

    /**
      Refreshes the configuration settings with file log settings structure.
    */
    void refreshFileLogSettings();

};

} // end of namespace

#endif // FILELOGGER_H
