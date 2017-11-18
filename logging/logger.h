/**
  @file
  @author Stefan Frings
*/

#ifndef LOGGER_H
#define LOGGER_H

#include <QtGlobal>
#include <QThreadStorage>
#include <QHash>
#include <QStringList>
#include <QMutex>
#include <QObject>
#include "logglobal.h"
#include "logmessage.h"

namespace qtwebapp {

/**
  Decorates and writes log messages to the console, stderr.
  <p>
  The decorator uses a predefined msgFormat string to enrich log messages
  with additional information (e.g. timestamp).
  <p>
  The msgFormat string and also the message text may contain additional
  variable names in the form  <i>{name}</i> that are filled by values
  taken from a static thread local dictionary.
  <p>
  The logger keeps a configurable number of messages in a ring-buffer.
  A log message with a severity >= minLevel flushes the buffer,
  so the stored messages get written out. If the buffer is disabled, then
  only messages with severity >= minLevel are written out.
  <p>
  If you enable the buffer and use minLevel=2, then the application logs
  only errors together with some buffered debug messages. But as long no
  error occurs, nothing gets written out.
  <p>
  Each thread has it's own buffer.
  <p>
  The logger can be registered to handle messages from
  the static global functions qDebug(), qWarning(), qCritical() and qFatal().

  @see set() describes how to set logger variables
  @see LogMessage for a description of the message decoration.
  @warning You should prefer a derived class, for example FileLogger,
  because logging to the console is less useful.
*/

class DECLSPEC Logger : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(Logger)
public:

    /**
      Constructor.
      Uses the same defaults as the other constructor.
      @param parent Parent object
    */
    Logger(QObject* parent);


    /**
      Constructor.
      @param msgFormat Format of the decoration, e.g. "{timestamp} {type} thread={thread}: {msg}"
      @param timestampFormat Format of timestamp, e.g. "dd.MM.yyyy hh:mm:ss.zzz"
      @param minLevel Minimum severity that genertes an output (0=debug, 1=warning, 2=critical, 3=fatal).
      @param bufferSize Size of the backtrace buffer, number of messages per thread. 0=disabled.
      @param parent Parent object
      @see LogMessage for a description of the message decoration.
    */
    Logger(const QString msgFormat="{timestamp} {type} {msg}", const QString timestampFormat="dd.MM.yyyy hh:mm:ss.zzz", const QtMsgType minLevel=QtDebugMsg, const int bufferSize=0, QObject* parent = 0);

    /** Destructor */
    virtual ~Logger();

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
      Installs this logger as the default message handler, so it
      can be used through the global static logging functions (e.g. qDebug()).
    */
    void installMsgHandler();

    /**
     * Sets the minimum message level on the fly
     */
    void setMinMessageLevel(const QtMsgType& minMsgLevel) {
        minLevel = minMsgLevel;
    }

    /**
     * Get the current message level
     */
    QtMsgType getMinMessageLevel() const {
        return minLevel;
    }

    /**
      Sets a thread-local variable that may be used to decorate log messages.
      This method is thread safe.
      @param name Name of the variable
      @param value Value of the variable
    */
    static void set(const QString& name, const QString& value);

    /**
      Clear the thread-local data of the current thread.
      This method is thread safe.
      @param buffer Whether to clear the backtrace buffer
      @param variables Whether to clear the log variables
    */
    virtual void clear(const bool buffer=true, const bool variables=true);

protected:

    /** Format string for message decoration */
    QString msgFormat;

    /** Format string of timestamps */
    QString timestampFormat;

    /** Minimum level of message types that are written out */
    QtMsgType minLevel;

    /** Size of backtrace buffer, number of messages per thread. 0=disabled */
    int bufferSize;

    /** Used to synchronize access of concurrent threads */
    static QMutex mutex;

    /**
      Decorate and write a log message to stderr. Override this method
      to provide a different output medium.
    */
    virtual void write(const LogMessage* logMessage);

private:

    /** Pointer to the default logger, used by msgHandler() */
    static Logger* defaultLogger;

    /**
      Message Handler for the global static logging functions (e.g. qDebug()).
      Forward calls to the default logger.
      <p>
      In case of a fatal message, the program will abort.
      Variables in the in the message are replaced by their values.
      This method is thread safe.
      @param type Message type (level)
      @param message Message text
      @param file Name of the source file where the message was generated (usually filled with the macro __FILE__)
      @param function Name of the function where the message was generated (usually filled with the macro __LINE__)
      @param line Line Number of the source file, where the message was generated (usually filles with the macro __func__ or __FUNCTION__)
    */
    static void msgHandler(const QtMsgType type, const QString &message, const QString &file="", const QString &function="", const int line=0);


#if QT_VERSION >= 0x050000

    /**
      Wrapper for QT version 5.
      @param type Message type (level)
      @param context Message context
      @param message Message text
      @see msgHandler()
    */
    static void msgHandler5(const QtMsgType type, const QMessageLogContext& context, const QString &message);

#else

    /**
      Wrapper for QT version 4.
      @param type Message type (level)
      @param message Message text
      @see msgHandler()
    */
    static void msgHandler4(const QtMsgType type, const char * message);

#endif

    /** Thread local variables to be used in log messages */
    static QThreadStorage<QHash<QString,QString>*> logVars;

    /** Thread local backtrace buffers */
    QThreadStorage<QList<LogMessage*>*> buffers;

};

} // end of namespace

#endif // LOGGER_H
