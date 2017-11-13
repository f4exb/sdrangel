/**
  @file
  @author Stefan Frings
*/

#ifndef HTTPSESSIONSTORE_H
#define HTTPSESSIONSTORE_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QMutex>
#include "httpglobal.h"
#include "httpsession.h"
#include "httpresponse.h"
#include "httprequest.h"
#include "httpsessionssettings.h"

namespace qtwebapp {

/**
  Stores HTTP sessions and deletes them when they have expired.
  The following configuration settings are required in the config file:
  <code><pre>
  expirationTime=3600000
  cookieName=sessionid
  </pre></code>
  The following additional configurations settings are optionally:
  <code><pre>
  cookiePath=/
  cookieComment=Session ID
  ;cookieDomain=stefanfrings.de
  </pre></code>
*/

class DECLSPEC HttpSessionStore : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(HttpSessionStore)
public:

    /** Constructor with Qt settings. */
    HttpSessionStore(QSettings* settings, QObject* parent=NULL);

    /** Constructor with settings structure. */
    HttpSessionStore(const HttpSessionsSettings& settings, QObject* parent=NULL);

    /** Destructor */
    virtual ~HttpSessionStore();

    /**
       Get the ID of the current HTTP session, if it is valid.
       This method is thread safe.
       @warning Sessions may expire at any time, so subsequent calls of
       getSession() might return a new session with a different ID.
       @param request Used to get the session cookie
       @param response Used to get and set the new session cookie
       @return Empty string, if there is no valid session.
    */
    QByteArray getSessionId(HttpRequest& request, HttpResponse& response);

    /**
       Get the session of a HTTP request, eventually create a new one.
       This method is thread safe. New sessions can only be created before
       the first byte has been written to the HTTP response.
       @param request Used to get the session cookie
       @param response Used to get and set the new session cookie
       @param allowCreate can be set to false, to disable the automatic creation of a new session.
       @return If autoCreate is disabled, the function returns a null session if there is no session.
       @see HttpSession::isNull()
    */
    HttpSession getSession(HttpRequest& request, HttpResponse& response, bool allowCreate=true);

    /**
       Get a HTTP session by it's ID number.
       This method is thread safe.
       @return If there is no such session, the function returns a null session.
       @param id ID number of the session
       @see HttpSession::isNull()
    */
    HttpSession getSession(const QByteArray id);

    /** Delete a session */
    void removeSession(HttpSession session);

    /**
     * Get a sessions settings copy
     * @return The current sessions settings
     */
    HttpSessionsSettings getListenerSettings() const { return sessionsSettings; }

    /**
     * Set new sessions settings data
     * @param sessions settings to replace current data
     */
    void setListenerSettings(const HttpSessionsSettings& settings) { sessionsSettings = settings; }

protected:
    /** Storage for the sessions */
    QMap<QByteArray,HttpSession> sessions;

private:

    /** Configuration settings as Qt settings*/
    QSettings* settings;

    /** Configuration settings as a structure*/
    HttpSessionsSettings sessionsSettings;

    /** Timer to remove expired sessions */
    QTimer cleanupTimer;

    /** Name of the session cookie */
    QByteArray cookieName;

    /** Time when sessions expire (in ms)*/
    int expirationTime;

    /** Used to synchronize threads */
    QMutex mutex;

    /** Settings flag */
    bool useQtSettings;

private slots:

    /** Called every minute to cleanup expired sessions. */
    void sessionTimerEvent();
};

} // end of namespace

#endif // HTTPSESSIONSTORE_H
