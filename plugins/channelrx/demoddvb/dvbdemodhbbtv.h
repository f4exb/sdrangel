///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DVBDEMODHBBTV_H
#define INCLUDE_DVBDEMODHBBTV_H

#include <vector>

#include <QElapsedTimer>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QTimer>

#include "dtv/dtvapps.h"

class DTVCarouselServer;
class QKeyEvent;
class QNetworkAccessManager;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;
class QWidget;
class DVBDemodHbbTVInterceptor;

// Runs the HbbTV applications (TS 102 796) of the service played, in a browser (Qt WebEngine) over the video: the
// AUTOSTART one of the service's AIT when it's played, and those it starts (as from the red button), from the
// service's carousels (served by a DTVCarouselServer) or from the internet. The OIPF objects applications use (the
// application manager, video/broadcast, configuration...) are oipf.js's. Keys in the application's keyset are given to
// it; the video is drawn where its video/broadcast object is
class DVBDemodHbbTV : public QObject
{
    Q_OBJECT

public:
    struct Channel {
        int m_networkId = -1;           //!< original_network_id
        int m_transportStreamId = -1;
        int m_serviceId = 0;
        int m_lcn = -1;
        QString m_name;
        bool m_radio = false;
        bool m_hd = false;
    };

    struct Programme {
        int m_eventId = 0;
        QString m_name;
        QString m_description;
        qint64 m_start = 0;             //!< UTC, seconds since 1970
        int m_duration = 0;             //!< Seconds
    };

    DVBDemodHbbTV(DTVApps *apps, QObject *parent = nullptr);
    ~DVBDemodHbbTV();

    //! The browser, as a child of the video widget: transparent, not taking the mouse or focus (keys are given by
    //! keyPress()), hidden while no application's visible. Its geometry is set with setViewGeometry()
    QWebEngineView *createView(QWidget *parent);
    void setViewGeometry(const QRect& geometry);
    //! The view moved to another widget, as the full screen video's
    void setViewParent(QWidget *parent);
    //! The view hidden while something's shown over the video instead (as the programme guide)
    void setSuppressed(bool suppressed);
    //! The keyboard focus given to the page while it's shown (so the application's focused items show as focused),
    //! else to the video widget. The page's keys are caught before the browser has them: those in the application's
    //! keyset given to it as keyPress() gives them, others to the video widget
    void takeFocus();
    QWebEnginePage *getPage() { return m_page; }
    //! Whether applications may use the internet: if not, only the carousels are loaded from
    void setNetworkAllowed(bool allowed);
    //! The user agent sent, for testing as a particular TV: empty for SDRangel's own. The environment variable
    //! DTV_HBBTV_USER_AGENT sets it too
    void setUserAgent(const QString& userAgent);
    QString getUserAgent() const;
    //! The ids of DVB channels: ID_DVB_T 12, ID_DVB_T2 16, ID_DVB_S 11, ID_DVB_S2 15
    void setIdType(int idType) { m_idType = idType; }

    //! The service played (its m_serviceId 0 if none), and the multiplex's others. A new service stops the application
    void setService(const Channel& channel, const std::vector<Channel>& channels, const std::vector<Programme>& programmes);
    //! After channelChangeRequested: whether the service's playing
    void channelChangeResult(bool ok);
    void stop();                        //!< Stops the application, until the service changes

    //! A key, given to the application if it's in its keyset: true if it was. Red with no application running
    //! starts the AUTOSTART one again (as when it's exited), and is given to it when it asks for it
    bool keyPress(QKeyEvent *event);
    bool keyRelease(QKeyEvent *event);
    //! The red button, as pressed and released: false if there's no application for it
    bool pressRed();
    //! The service has an AUTOSTART HbbTV application that can be loaded (whether it's running or not)
    bool hasAutostartApp() const;

    bool isRunning() const { return !m_appUrl.isEmpty(); }
    //! What's being waited for, after the user's pressed red or a key the application has: the application starting,
    //! or loading (with how much of its carousel's been received). Empty when there's nothing to say
    QString getStatus() const;
    //! The page's showing the status itself (over the application): it needn't be shown over the video
    bool isStatusInPage() const { return !m_pageStatus.isEmpty(); }
    bool isVisible() const { return m_visible; }
    int getKeyset() const { return m_keyset; }
    QString getAppUrl() const { return m_appUrl; }
    QString getAppName() const { return m_appName; }

    //! An event of the player playing the application's media (mediaRequest's id): metadata (duration, width,
    //! height), playing, time (position), waiting, ended, error (code, message)
    void mediaEvent(int id, const QString& type, const QJsonObject& data = QJsonObject());
    int getMediaId() const { return m_mediaId; }

    //! Map a Qt key to an HbbTV key code and the keyset bit it needs: false if it isn't one
    static bool keyCode(int qtKey, int& code, int& keysetBit);
    //! What applications have stored (cookies, as sign-ins, and the cache) forgotten
    static void clearData();

signals:
    void visibleChanged(bool visible);
    //! Where the application wants the video, as fractions of the page: null for all of it
    void videoAreaChanged(const QRectF& area);
    void channelChangeRequested(int networkId, int transportStreamId, int serviceId);
    void channelStepRequested(int direction);
    void appStarted(const QString& url, const QString& name);
    //! The application's media (as iPlayer's video): command load (url, with FFmpeg's options), play, pause, seek
    //! (position, seconds) or stop. Media's played by the GUI's player, under the page
    void mediaRequest(const QString& command, int id, const QString& url, const QMap<QString, QString>& options, double position);
    void appStopped();
    //! The application's console messages, and what it does, for the log
    void log(const QString& message);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    friend class DVBDemodHbbTVPage;

    //! Applications' profile: kept (cookies, local storage, cache), so sign-ins last from one run to the next. Shared
    //! by the demodulators running applications (a profile's storage can only be open once), and deleted with the last
    static QWebEngineProfile *acquireProfile();
    static void releaseProfile();

    void check();
    void launch(const DTVApps::App& app, const QString& suffix);
    void navigate(const QString& url, const QString& name, uint32_t organisationId, int applicationId, bool autostart);
    void message(const QJsonObject& message);
    void consoleMessage(int level, const QString& message, int line, const QString& source);
    void createApplication(const QString& uri);
    void destroyApplication();
    void setVisible(bool visible);
    void showView();
    void loadFinished(bool ok);
    void updateScript();
    QString scriptSource() const;
    static QByteArray withScript(const QByteArray& page, const QString& script);
    void showStatus();                  //!< getStatus() in the page, over what the application shows
    void updateProxy();
    QJsonObject configuration() const;
    QJsonObject channelJson(const Channel& channel) const;
    QString appUrl(const DTVApps::App& app) const;
    void makeTransparent();
    void downloadRequested(const QUrl& url, const QString& mimeType);

    DTVApps *m_apps;
    DTVCarouselServer *m_server;
    QWebEngineProfile *m_profile;
    QWebEnginePage *m_page;
    QPointer<QWebEngineView> m_view;
    DVBDemodHbbTVInterceptor *m_interceptor;
    QNetworkAccessManager *m_network;
    QString m_oipfScript;
    QTimer m_timer;
    int m_idType;

    Channel m_channel;
    std::vector<Channel> m_channels;
    std::vector<Programme> m_programmes;

    QString m_appUrl;                   //!< Running, empty if none
    QString m_appName;
    uint32_t m_organisationId;
    int m_applicationId;
    bool m_autostartApp;                //!< The running one's the service's AUTOSTART application
    bool m_autostartDisabled;           //!< Until the service changes (it destroyed itself, or failed to load)
    qint64 m_autostartRetryAt;          //!< When a failed AUTOSTART application's tried again (ms since epoch), 0 none
    bool m_visible;
    bool m_loaded;
    bool m_suppressed;
    int m_keyset;
    QRectF m_videoArea;
    bool m_pendingRed;                  //!< Red's to be given to the application started for it, once it asks for it
    int m_mediaId;                      //!< Of the media being played, 0 if none
    QElapsedTimer m_userRequest;        //!< Since the user last pressed red or gave the application a key
    QElapsedTimer m_failed;             //!< Since an application last failed to load
    int m_appCarouselPid;               //!< The carousel the running application's loaded from, -1 if none
    int m_loadsStarted;                 //!< Qt 5: page loads started, to tell a failed load from one replaced by another
    QElapsedTimer m_loadedAt;           //!< Since the running application's page loaded
    bool m_keysetSinceLoad;             //!< It's set its keyset since: it's ready
    QString m_pageStatus;               //!< Status last shown in the page
    QElapsedTimer m_redDelivered;       //!< Since red was last given to the application
    QElapsedTimer m_pendingRedTimer;
};

#endif // INCLUDE_DVBDEMODHBBTV_H
