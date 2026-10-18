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

#include <atomic>
#include <cmath>
#include <iterator>

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQuickWidget>
#include <QWebEngineCookieStore>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QWebEngineDownloadRequest>
#include <QWebEngineLoadingInfo>
#else
#include <QWebEngineDownloadItem>
#endif
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QRegularExpression>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>

#include "dtv/dtvcarouselserver.h"

#include "dvbdemodhbbtv.h"

namespace {

const QString m_messagePrefix = QStringLiteral("\u0001sdrangel:");
const QString m_scriptName = QStringLiteral("sdrangel-oipf");
constexpr int m_checkMs = 250;
constexpr qint64 m_repeatMs = 60000;   //!< A message like one logged (its numbers aside) held back for this long
constexpr int m_retransparentMax = 3;   //!< Times the view's made transparent again, for a video area
// setContent()'s content is percent-encoded to go to the renderer, which takes 2 MB less 30 bytes of it
constexpr int m_setContentLimit = 2 * 1024 * 1024 - 30;
constexpr int m_retryMs = 30000;        //!< Before a failed AUTOSTART application's tried again
constexpr int m_applicationTypeHbbTV = 0x10;
constexpr int m_autostart = 1;
constexpr int m_destroy = 3;
constexpr int m_kill = 4;

// Keyset bits (OIPF DAE 7.2.5)
constexpr int m_keysetRed = 0x1;
constexpr int m_keysetGreen = 0x2;
constexpr int m_keysetYellow = 0x4;
constexpr int m_keysetBlue = 0x8;
constexpr int m_keysetNavigation = 0x10;
constexpr int m_keysetVCR = 0x20;
constexpr int m_keysetScroll = 0x40;
constexpr int m_keysetInfo = 0x80;
constexpr int m_keysetNumeric = 0x100;

} // namespace

// Blocks requests to the internet, when applications aren't allowed it: those of the carousels (the loopback
// interface) and of the page itself are let through. A carousel's application's requests to the internet have the
// origin it has on a TV, its service's dvb: URL (dvb://onid.tsid.sid), rather than the server's, which broadcasters'
// servers refuse. Called in WebEngine's IO thread
class DVBDemodHbbTVInterceptor : public QWebEngineUrlRequestInterceptor
{
public:
    explicit DVBDemodHbbTVInterceptor(QObject *parent) : QWebEngineUrlRequestInterceptor(parent) {}

    void interceptRequest(QWebEngineUrlRequestInfo& info) override
    {
        const QUrl url = info.requestUrl();
        const QString scheme = url.scheme();
        const bool local = url.host().endsWith(".localhost") || (scheme == "data") || (scheme == "about") || (scheme == "blob");

        if (!m_networkAllowed && !local)
        {
            info.block(true);
            return;
        }

        const QString initiator = info.initiator().host();

        if (!local && initiator.startsWith("dvb-") && initiator.endsWith(".localhost"))
        {
            const QStringList parts = initiator.mid(4, initiator.size() - 4 - 10).split('-');

            if ((parts.size() == 4) && (parts[0] != "x") && (parts[1] != "x")) {
                info.setHttpHeader("Origin", QString("dvb://%1.%2.%3").arg(parts[0]).arg(parts[1]).arg(parts[2]).toLatin1());
            }
        }
    }

    std::atomic<bool> m_networkAllowed{true};
};

// The page: its console messages are the application's messages, and its own; no windows are opened
class DVBDemodHbbTVPage : public QWebEnginePage
{
public:
    DVBDemodHbbTVPage(QWebEngineProfile *profile, DVBDemodHbbTV *owner) :
        QWebEnginePage(profile, owner),
        m_owner(owner)
    {
    }

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int line, const QString& source) override
    {
        m_owner->consoleMessage((int) level, message, line, source);
    }

    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override
    {
        (void) type;

        // A dvb: link: the carousel's file, or an application of the AIT
        if (url.scheme() == "dvb")
        {
            if (isMainFrame) {
                QMetaObject::invokeMethod(m_owner, [owner = m_owner, uri = url.toString()]() { owner->createApplication(uri); }, Qt::QueuedConnection);
            }

            return false;
        }

        return true;
    }

    QWebEnginePage *createWindow(WebWindowType type) override
    {
        (void) type;
        return nullptr;
    }

private:
    DVBDemodHbbTV *m_owner;
};

DVBDemodHbbTV::DVBDemodHbbTV(DTVApps *apps, QObject *parent) :
    QObject(parent),
    m_apps(apps),
    m_server(new DTVCarouselServer(apps, this)),
    m_profile(acquireProfile()),
    m_page(nullptr),
    m_interceptor(new DVBDemodHbbTVInterceptor(this)),
    m_network(new QNetworkAccessManager(this)),
    m_idType(12),
    m_organisationId(0),
    m_applicationId(0),
    m_autostartApp(false),
    m_autostartDisabled(false),
    m_autostartRetryAt(0),
    m_visible(false),
    m_loaded(false),
    m_suppressed(false),
    m_keyset(0),
    m_pendingRed(false),
    m_mediaId(0),
    m_appCarouselPid(-1),
    m_loadsStarted(0),
    m_keysetSinceLoad(false)
{
    m_server->start();
    connect(m_server, &DTVCarouselServer::served, this, [this](const QString& path, int status) {
        logLimited(QString("Carousel: %1 %2").arg(path).arg(status), QString());
    });

    QFile script(":/demoddvb/hbbtv/oipf.js");

    if (script.open(QIODevice::ReadOnly)) {
        m_oipfScript = QString::fromUtf8(script.readAll());
    } else {
        qWarning("DVBDemodHbbTV::DVBDemodHbbTV: no oipf.js");
    }

    QString agent = qEnvironmentVariable("DTV_HBBTV_USER_AGENT");
    if (agent.isEmpty()) {
        agent = "Mozilla/5.0 (Web0S; Linux/SmartTV) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/38.0.2125.122 Safari/537.36 HbbTV/1.4.1 (+DRM; LGE; 55UJ670V-ZD; WEBOS3.5 06.10.50; W3_K3LP; W3_K3LP;)";
    }
    setUserAgent(agent);
    // Downloads: of this page's only, as the profile's shared
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadRequest *download) {
#else
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadItem *download) {
#endif
        if (download->page() == m_page)
        {
            const QUrl url = download->url();
            const QString mimeType = download->mimeType();
            download->cancel();
            downloadRequested(url, mimeType);
        }
    });

    m_page = new DVBDemodHbbTVPage(m_profile, this);
    m_page->setUrlRequestInterceptor(m_interceptor);
    m_page->setBackgroundColor(Qt::transparent);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_page, &QWebEnginePage::loadingChanged, this, [this](const QWebEngineLoadingInfo& info) {
        if (info.status() == QWebEngineLoadingInfo::LoadSucceededStatus) {
            loadFinished(true);
        } else if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus) {
            emit log(QString("Load failed: %1: %2").arg(info.url().toString()).arg(info.errorString()));
            loadFinished(false);
        }
    });
#else
    // Qt 5 says a load's failed when it's replaced by another (as when a page goes to another as it loads): only a
    // load not followed by another is taken to have failed
    connect(m_page, &QWebEnginePage::loadStarted, this, [this]() { m_loadsStarted++; });
    connect(m_page, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        if (ok)
        {
            loadFinished(true);
            return;
        }

        const int loads = m_loadsStarted;
        QTimer::singleShot(500, this, [this, loads]() {
            if (loads == m_loadsStarted)
            {
                emit log(QString("Load failed: %1").arg(m_page->url().toString()));
                loadFinished(false);
            }
        });
    });
#endif

    connect(&m_timer, &QTimer::timeout, this, &DVBDemodHbbTV::check);
    m_timer.start(m_checkMs);
}

DVBDemodHbbTV::~DVBDemodHbbTV()
{
    // The page before its profile
    delete m_view;
    delete m_page;
    releaseProfile();
}

namespace {
QWebEngineProfile *s_profile = nullptr;
int s_profileUsers = 0;
}

QWebEngineProfile *DVBDemodHbbTV::acquireProfile()
{
    if (!s_profile)
    {
        // In the application's data directory, as QtWebEngine/sdrangel-hbbtv
        s_profile = new QWebEngineProfile("sdrangel-hbbtv");
        s_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
        QWebEngineSettings *settings = s_profile->settings();
        settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
        settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
        settings->setAttribute(QWebEngineSettings::PluginsEnabled, false);
        settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
        settings->setAttribute(QWebEngineSettings::ShowScrollBars, false);
        settings->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);
        settings->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, false);
    }

    s_profileUsers++;
    return s_profile;
}

void DVBDemodHbbTV::releaseProfile()
{
    if ((--s_profileUsers == 0) && s_profile)
    {
        delete s_profile;
        s_profile = nullptr;
    }
}

// The profile's kept until its cache is cleared: deleted while that's in progress (as when no application's running, so
// this is its only user), it corrupts the heap
void DVBDemodHbbTV::clearData()
{
    QWebEngineProfile *profile = acquireProfile();
    profile->cookieStore()->deleteAllCookies();
    profile->clearAllVisitedLinks();
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    QObject::connect(profile, &QWebEngineProfile::clearHttpCacheCompleted, profile, []() {
        // Not in the profile's own signal, as it may be deleted
        QTimer::singleShot(0, []() { releaseProfile(); });
    }, Qt::SingleShotConnection);
    profile->clearHttpCache();
#else
    // No signal when it's done: a while later
    profile->clearHttpCache();
    QTimer::singleShot(10000, []() { releaseProfile(); });
#endif
}

QWebEngineView *DVBDemodHbbTV::createView(QWidget *parent)
{
    if (!m_view)
    {
        m_view = new QWebEngineView(parent);
        m_view->setPage(m_page);
        m_view->setAttribute(Qt::WA_TranslucentBackground);
        m_view->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_view->setFocusPolicy(Qt::NoFocus);
        m_view->setContextMenuPolicy(Qt::NoContextMenu);
        makeTransparent();
        showView();
    }

    return m_view;
}

// Composited over the video (a QOpenGLWidget) with its transparency: the view draws with a QQuickWidget, which is
// otherwise composited under other widgets' contents
void DVBDemodHbbTV::makeTransparent()
{
    if (!m_view) {
        return;
    }

    for (QQuickWidget *widget : m_view->findChildren<QQuickWidget*>())
    {
        widget->setAttribute(Qt::WA_AlwaysStackOnTop);
        widget->setAttribute(Qt::WA_TransparentForMouseEvents);
        widget->setFocusPolicy(Qt::NoFocus);
        widget->setClearColor(Qt::transparent);
        widget->removeEventFilter(this);
        widget->installEventFilter(this);
    }

    m_view->removeEventFilter(this);
    m_view->installEventFilter(this);

    if (m_view->focusProxy())
    {
        m_view->focusProxy()->removeEventFilter(this);
        m_view->focusProxy()->installEventFilter(this);
    }
}

// While the video's shown in the page, the view's kept transparent. Qt makes it opaque (its QQuickWidget's clear colour,
// and not stacked on top, so composited without blending: black over the video) when Chromium reports the page's
// background opaque, as TVP VOD's black, while it's browsed. When it's cleared for the video, Chromium doesn't always
// report it's transparent again (until the view's resized, as for full screen): it does once the background SDRangel
// gives the page changes, so it's made black then transparent again
void DVBDemodHbbTV::keepTransparent()
{
    if (!m_view || m_videoArea.isNull() || m_retransparent || (m_retransparentCount >= m_retransparentMax)) {
        return;
    }

    bool opaque = false;

    for (QQuickWidget *widget : m_view->findChildren<QQuickWidget*>()) {
        opaque = opaque || !widget->testAttribute(Qt::WA_AlwaysStackOnTop);
    }

    if (!opaque) {
        return;
    }

    // Not over and over (each time the page's drawn again, perhaps a flash): if it's still opaque, it's as the page is
    // (its background opaque, as an image the video's area can't be cut from), until the video's area changes
    if (++m_retransparentCount >= m_retransparentMax) {
        qDebug("DVBDemodHbbTV::keepTransparent: the page's drawn opaque over its video: made transparent again, not again");
    } else {
        qDebug("DVBDemodHbbTV::keepTransparent: the page's drawn opaque over its video: made transparent again");
    }

    m_retransparent = true;
    m_page->setBackgroundColor(Qt::black);
    QTimer::singleShot(0, this, [this]() {
        m_page->setBackgroundColor(Qt::transparent);
        makeTransparent();
        // Not again until Chromium's had a while to say what the page's background is
        QTimer::singleShot(1000, this, [this]() { m_retransparent = false; });
    });
}

void DVBDemodHbbTV::takeFocus()
{
    if (!m_view) {
        return;
    }

    if (m_view->isVisible())
    {
        QWidget *widget = m_view->focusProxy() ? m_view->focusProxy() : m_view.data();
        widget->setFocus(Qt::OtherFocusReason);
    }
    else if (m_view->parentWidget())
    {
        m_view->parentWidget()->setFocus(Qt::OtherFocusReason);
    }
}

// The page's keys: the application's, given to it as keyPress() does (as a TV's remote's, with their codes), the
// others to the video widget, as when it has the focus
bool DVBDemodHbbTV::eventFilter(QObject *object, QEvent *event)
{
    const QEvent::Type type = event->type();

    // A widget the view's made, as WebEngine does for a page's drawing when the view's moved to another window (full
    // screen video's): made transparent, and not taking the mouse, as the first were, else it hides the video
    if ((type == QEvent::ChildAdded) && (object == m_view.data()))
    {
        QObject *child = static_cast<QChildEvent*>(event)->child();

        if (child && child->isWidgetType())
        {
            qDebug("DVBDemodHbbTV::eventFilter: view widget added: %s", child->metaObject()->className());
            QTimer::singleShot(0, this, &DVBDemodHbbTV::makeTransparent);
        }
    }

    if ((type == QEvent::KeyPress) || (type == QEvent::KeyRelease) || (type == QEvent::ShortcutOverride))
    {
        QKeyEvent *key = static_cast<QKeyEvent*>(event);

        // Not shortcuts of the window's: the keys are the video's
        if (type == QEvent::ShortcutOverride)
        {
            event->accept();
            return true;
        }

        const bool taken = type == QEvent::KeyPress ? keyPress(key) : keyRelease(key);

        if (!taken && m_view && m_view->parentWidget())
        {
            QKeyEvent copy(type, key->key(), key->modifiers(), key->text(), key->isAutoRepeat(), key->count());
            QCoreApplication::sendEvent(m_view->parentWidget(), &copy);
        }

        return true;
    }

    return QObject::eventFilter(object, event);
}

void DVBDemodHbbTV::setViewGeometry(const QRect& geometry)
{
    if (!m_view) {
        return;
    }

    m_view->setGeometry(geometry);
    m_viewWidth = geometry.width();
    applyZoom();
}

// Applications are laid out for 1280 by 720 pixels, or some for 1920 by 1080: zoomed to fill the view. The view's
// device pixels rounded up, so the page's no wider than that (with display scaling the view's a fraction of a device
// pixel wider, which would make it 1281: an application choosing its layout by media queries would take a wider one)
void DVBDemodHbbTV::applyZoom()
{
    if (m_viewWidth > 0)
    {
        const double ratio = m_view ? m_view->devicePixelRatioF() : 1.0;
        const double devicePixels = std::ceil(m_viewWidth * ratio);
        const double zoom = devicePixels / (ratio * m_layoutWidth) * (1.0 + 1e-6);

        m_page->setZoomFactor(std::max(0.25, std::min(5.0, zoom)));
        qDebug("DVBDemodHbbTV::applyZoom: view %d wide, device pixel ratio %.3f, laid out for %d: zoom %.5f",
            m_viewWidth, ratio, m_layoutWidth, zoom);
    }
}

void DVBDemodHbbTV::setViewParent(QWidget *parent)
{
    if (m_view && (m_view->parentWidget() != parent))
    {
        m_view->setParent(parent);
        makeTransparent();
        showView();
    }
}

void DVBDemodHbbTV::setSuppressed(bool suppressed)
{
    if (suppressed != m_suppressed)
    {
        m_suppressed = suppressed;
        showView();
    }
}

void DVBDemodHbbTV::showView()
{
    if (m_view)
    {
        const bool show = isRunning() && m_visible && m_loaded && !m_suppressed;

        if (show != m_view->isVisible())
        {
            QWidget *parent = m_view->parentWidget();
            QWidget *focus = QApplication::focusWidget();
            const bool hadFocus = focus && ((focus == m_view) || m_view->isAncestorOf(focus));
            const bool parentHadFocus = parent && (focus == parent);
            m_view->setVisible(show);

            if (show)
            {
                m_view->raise();
                makeTransparent();

                if (parentHadFocus) {
                    takeFocus();
                }
            }
            else if (hadFocus && parent)
            {
                parent->setFocus(Qt::OtherFocusReason);
            }
        }
    }
}

void DVBDemodHbbTV::setUserAgent(const QString& userAgent)
{
    if (!userAgent.isEmpty())
    {
        m_profile->setHttpUserAgent(userAgent);
        emit log(QString("User agent: %1").arg(userAgent));
        updateProxy();
        return;
    }

    // As a TV's: HbbTV/1.5.1 is HbbTV 2.0.2 (TS 102 796 7.3.2.4)
    QWebEngineProfile defaultProfile;
    m_profile->setHttpUserAgent(defaultProfile.httpUserAgent()
        + QString(" HbbTV/1.5.1 (; SDRangel; DVBDemod; %1; ; SDRangel; )").arg(QCoreApplication::applicationVersion()));
    updateProxy();
}

QString DVBDemodHbbTV::getUserAgent() const
{
    return m_profile->httpUserAgent();
}

void DVBDemodHbbTV::setNetworkAllowed(bool allowed)
{
    m_interceptor->m_networkAllowed = allowed;
    updateProxy();
    updateScript();     // Its configuration has the proxy if it's allowed (Qt 5)
}

// Qt 5's WebEngine sends the page's origin whatever the interceptor says, which broadcasters' servers refuse: a
// carousel's application's requests to other sites go by the carousel server, which makes them with its TV origin
void DVBDemodHbbTV::updateProxy()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_server->setProxy(m_interceptor->m_networkAllowed, m_profile->httpUserAgent());
#endif
}

bool DVBDemodHbbTV::keyCode(int qtKey, int& code, int& keysetBit)
{
    switch (qtKey)
    {
    case Qt::Key_R: case Qt::Key_F1: code = 403; keysetBit = m_keysetRed; return true;
    case Qt::Key_G: case Qt::Key_F2: code = 404; keysetBit = m_keysetGreen; return true;
    case Qt::Key_Y: case Qt::Key_F3: code = 405; keysetBit = m_keysetYellow; return true;
    case Qt::Key_B: case Qt::Key_F4: code = 406; keysetBit = m_keysetBlue; return true;
    case Qt::Key_Left: code = 37; keysetBit = m_keysetNavigation; return true;
    case Qt::Key_Up: code = 38; keysetBit = m_keysetNavigation; return true;
    case Qt::Key_Right: code = 39; keysetBit = m_keysetNavigation; return true;
    case Qt::Key_Down: code = 40; keysetBit = m_keysetNavigation; return true;
    case Qt::Key_Return: case Qt::Key_Enter: code = 13; keysetBit = m_keysetNavigation; return true;
    case Qt::Key_Backspace: case Qt::Key_Back: code = 461; keysetBit = m_keysetNavigation; return true;
    case Qt::Key_PageUp: code = 33; keysetBit = m_keysetScroll; return true;
    case Qt::Key_PageDown: code = 34; keysetBit = m_keysetScroll; return true;
    case Qt::Key_MediaPlay: code = 415; keysetBit = m_keysetVCR; return true;
    case Qt::Key_MediaPause: code = 19; keysetBit = m_keysetVCR; return true;
    case Qt::Key_MediaTogglePlayPause: code = 402; keysetBit = m_keysetVCR; return true;
    case Qt::Key_MediaStop: code = 413; keysetBit = m_keysetVCR; return true;
    case Qt::Key_MediaNext: code = 425; keysetBit = m_keysetVCR; return true;
    case Qt::Key_MediaPrevious: code = 424; keysetBit = m_keysetVCR; return true;
    case Qt::Key_Info: code = 457; keysetBit = m_keysetInfo; return true;
    default:
        if ((qtKey >= Qt::Key_0) && (qtKey <= Qt::Key_9))
        {
            code = 48 + (qtKey - Qt::Key_0);
            keysetBit = m_keysetNumeric;
            return true;
        }

        return false;
    }
}

bool DVBDemodHbbTV::keyPress(QKeyEvent *event)
{
    int code, bit;

    if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) || !keyCode(event->key(), code, bit)) {
        return false;
    }

    // Red with no application running starts the service's again
    if ((bit == m_keysetRed) && !isRunning() && !event->isAutoRepeat()) {
        return pressRed();
    }

    if (!isRunning() || !m_loaded || !(m_keyset & bit)) {
        return false;
    }

    m_userRequest.start();

    if (bit == m_keysetRed) {
        m_redDelivered.start();
    }

    // Which keys the application's given, for the log (as what it does may follow from one)
    if (!event->isAutoRepeat()) {
        qDebug("DVBDemodHbbTV::keyPress: key %d given to the application", code);
    }

    m_page->runJavaScript(QString("window.__sdrangelHbbTV && window.__sdrangelHbbTV.key(%1, true)").arg(code));
    return true;
}

bool DVBDemodHbbTV::keyRelease(QKeyEvent *event)
{
    int code, bit;

    if (!isRunning() || !m_loaded || !keyCode(event->key(), code, bit) || !(m_keyset & bit) || event->isAutoRepeat()) {
        return false;
    }

    m_page->runJavaScript(QString("window.__sdrangelHbbTV && window.__sdrangelHbbTV.key(%1, false)").arg(code));
    return true;
}

bool DVBDemodHbbTV::pressRed()
{
    m_userRequest.start();

    if (isRunning() && m_loaded && (m_keyset & m_keysetRed))
    {
        qDebug("DVBDemodHbbTV::pressRed: red given to the application");
        m_page->runJavaScript("window.__sdrangelHbbTV && (window.__sdrangelHbbTV.key(403, true), window.__sdrangelHbbTV.key(403, false))");
        m_redDelivered.start();
        return true;
    }

    if (!isRunning() && hasAutostartApp())
    {
        emit log("Red: starting the AUTOSTART application");
        m_autostartDisabled = false;
        m_autostartRetryAt = 0;
        m_pendingRed = true;
        m_pendingRedTimer.start();
        check();
        return true;
    }

    // Loading, or it's not asked for red yet: given to it when it does
    if (isRunning())
    {
        m_pendingRed = true;
        m_pendingRedTimer.start();
        return true;
    }

    return false;
}

QString DVBDemodHbbTV::getStatus() const
{
    // The application's asked for a DRM licence for its video, which can't be played: for a while, over what it shows
    // (as it may only say it's loading, or that something went wrong)
    if (m_drmRequest.isValid() && (m_drmRequest.elapsed() < 8000)) {
        return QString("Protected by %1 DRM: can't be played").arg(m_drmSystem);
    }

    if (!m_userRequest.isValid() || (m_userRequest.elapsed() > 60000)) {
        return QString();
    }

    if (m_failed.isValid() && (m_failed.elapsed() < 5000)) {
        return "Red button: not available";
    }

    if (!isRunning()) {
        return m_pendingRed && (m_userRequest.elapsed() < 30000) ? QString("Red button: starting...") : QString();
    }

    if (m_loaded)
    {
        // Loaded, but getting what it shows (as from the internet) until it's asked for keys; and just after red's
        // given to it, before it starts what it's for
        if ((!m_keysetSinceLoad && m_loadedAt.isValid() && (m_loadedAt.elapsed() < 8000) && m_visible)
            || (m_redDelivered.isValid() && (m_redDelivered.elapsed() < 1500))) {
            return "Red button: loading...";
        }

        return QString();
    }

    // From a carousel: as far as it's been received
    if (m_appCarouselPid >= 0)
    {
        for (const DTVApps::Carousel& carousel : m_apps->getCarousels(m_channel.m_serviceId))
        {
            if ((carousel.m_pid == m_appCarouselPid) && !carousel.m_complete)
            {
                const int percent = carousel.m_progress.m_bytes > 0 ? (int) (100 * carousel.m_progress.m_bytesComplete / carousel.m_progress.m_bytes) : 0;
                return QString("Red button: loading from the broadcast (%1%)").arg(percent);
            }
        }
    }

    return "Red button: loading...";
}

void DVBDemodHbbTV::mediaEvent(int id, const QString& type, const QJsonObject& data)
{
    if (id == 0) {
        return;
    }

    const QString json = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
    m_page->runJavaScript(QString("window.__sdrangelHbbTV && window.__sdrangelHbbTV.media(%1, '%2', %3)").arg(id).arg(type).arg(json));
}

bool DVBDemodHbbTV::hasAutostartApp() const
{
    if (m_channel.m_serviceId == 0) {
        return false;
    }

    for (const DTVApps::App& app : m_apps->getApps(m_channel.m_serviceId))
    {
        if ((app.m_applicationType == m_applicationTypeHbbTV) && (app.m_controlCode == m_autostart) && !appUrl(app).isEmpty()) {
            return true;
        }
    }

    return false;
}

void DVBDemodHbbTV::setService(const Channel& channel, const std::vector<Channel>& channels, const std::vector<Programme>& programmes)
{
    const bool changed = (channel.m_serviceId != m_channel.m_serviceId) || (channel.m_transportStreamId != m_channel.m_transportStreamId);
    m_channel = channel;
    m_channels = channels;
    m_programmes = programmes;

    if (changed)
    {
        // The application keeps running if the new service signals it too and it isn't bound to the old one
        // (TS 102 796 6.2.2.2)
        bool keep = false;

        if (isRunning() && (channel.m_serviceId != 0))
        {
            for (const DTVApps::App& app : m_apps->getApps(channel.m_serviceId))
            {
                keep |= (app.m_organisationId == m_organisationId) && (app.m_applicationId == m_applicationId)
                    && (m_applicationId != 0) && !app.m_serviceBound && (app.m_controlCode != m_kill) && (app.m_controlCode != m_destroy);
            }
        }

        if (!keep && isRunning())
        {
            emit log("Service changed: application stopped");
            destroyApplication();
        }

        m_autostartDisabled = false;
        m_autostartRetryAt = 0;
        m_pendingRed = false;
    }

    updateScript();

    if (isRunning())
    {
        const QString json = QString::fromUtf8(QJsonDocument(configuration()).toJson(QJsonDocument::Compact));
        m_page->runJavaScript(QString("window.__sdrangelHbbTV && window.__sdrangelHbbTV.update(%1)").arg(json));
    }

    check();
}

void DVBDemodHbbTV::channelChangeResult(bool ok)
{
    // OIPF's errorState 5: "cannot tune" (as no transport stream's been found)
    m_page->runJavaScript(QString("window.__sdrangelHbbTV && window.__sdrangelHbbTV.channelChanged(%1, %2)").arg(ok ? "true" : "false").arg(ok ? 0 : 5));
}

void DVBDemodHbbTV::stop()
{
    if (isRunning()) {
        destroyApplication();
    }

    m_autostartDisabled = true;
}

// The service's AUTOSTART application started when there's none running; one the AIT kills, stopped
void DVBDemodHbbTV::showStatus()
{
    const QString status = isRunning() && m_loaded ? getStatus() : QString();

    if (status != m_pageStatus)
    {
        m_pageStatus = status;
        QString quoted = status;
        quoted.replace('\\', "\\\\").replace('\'', "\\'");
        m_page->runJavaScript(QString("window.__sdrangelHbbTV && window.__sdrangelHbbTV.status('%1')").arg(quoted));
    }
}

void DVBDemodHbbTV::check()
{
    showStatus();
    keepTransparent();

    if (m_channel.m_serviceId == 0) {
        return;
    }

    const std::vector<DTVApps::App> apps = m_apps->getApps(m_channel.m_serviceId);

    if (isRunning())
    {
        for (const DTVApps::App& app : apps)
        {
            if ((app.m_organisationId == m_organisationId) && (app.m_applicationId == m_applicationId) && (m_applicationId != 0)
                && ((app.m_controlCode == m_kill) || (app.m_controlCode == m_destroy)))
            {
                emit log(QString("The AIT stops application %1").arg(app.idText()));
                destroyApplication();
                return;
            }
        }

        return;
    }

    if (m_autostartDisabled || ((m_autostartRetryAt > 0) && (QDateTime::currentMSecsSinceEpoch() < m_autostartRetryAt))) {
        return;
    }

    // Not until the service's network and transport stream are known: a carousel's application's origin is of them
    // (dvb://onid.tsid.sid), and broadcasters' servers refuse others
    const int networkId = m_channel.m_networkId >= 0 ? m_channel.m_networkId : m_apps->getOriginalNetworkId();
    const int transportStreamId = m_channel.m_transportStreamId >= 0 ? m_channel.m_transportStreamId : m_apps->getTransportStreamId();

    if ((networkId < 0) || (transportStreamId < 0)) {
        return;
    }

    const DTVApps::App *best = nullptr;

    for (const DTVApps::App& app : apps)
    {
        if ((app.m_applicationType == m_applicationTypeHbbTV) && (app.m_controlCode == m_autostart) && !appUrl(app).isEmpty()
            && (!best || (app.m_priority > best->m_priority))) {
            best = &app;
        }
    }

    if (best) {
        launch(*best, QString());
    }
}

// Where an application's loaded from: its carousel, if it's in one of the service's (being received), else its URL
QString DVBDemodHbbTV::appUrl(const DTVApps::App& app) const
{
    const int networkId = m_channel.m_networkId >= 0 ? m_channel.m_networkId : m_apps->getOriginalNetworkId();
    const int transportStreamId = m_channel.m_transportStreamId >= 0 ? m_channel.m_transportStreamId : m_apps->getTransportStreamId();

    if ((app.m_componentTag >= 0) && (app.m_carouselPid >= 0)) {
        return m_server->httpUrl(app.url(networkId, transportStreamId));
    }

    if (!app.m_urlBases.isEmpty())
    {
        DTVApps::App broadband = app;
        broadband.m_componentTag = -1;
        return broadband.url(networkId, transportStreamId);
    }

    return QString();
}

void DVBDemodHbbTV::launch(const DTVApps::App& app, const QString& suffix)
{
    QString url = appUrl(app);

    if (url.isEmpty())
    {
        emit log(QString("Application %1 can't be loaded: it's in another service's carousel").arg(app.idText()));
        return;
    }

    // Parameters given to it, after its location's own
    if (suffix.startsWith('?') && url.contains('?') && !url.contains('#')) {
        url += "&" + suffix.mid(1);
    } else {
        url += suffix;
    }

    navigate(url, app.m_name, app.m_organisationId, app.m_applicationId, app.m_controlCode == m_autostart);
    m_appCarouselPid = (app.m_componentTag >= 0) && m_server->isServerUrl(url) ? app.m_carouselPid : -1;
}

void DVBDemodHbbTV::navigate(const QString& url, const QString& name, uint32_t organisationId, int applicationId, bool autostart)
{
    m_appUrl = url;
    m_appName = name;
    m_organisationId = organisationId;
    m_applicationId = applicationId;
    m_autostartApp = autostart;
    m_appCarouselPid = -1;
    m_keyset = 0;
    m_loaded = false;
    m_keysetSinceLoad = false;
    m_pageStatus.clear();

    if (m_layoutWidth != m_defaultLayoutWidth)
    {
        m_layoutWidth = m_defaultLayoutWidth;
        applyZoom();
    }

    if (m_mediaId != 0)
    {
        emit mediaRequest("stop", m_mediaId, QString(), QMap<QString, QString>(), 0.0);
        m_mediaId = 0;
    }

    m_visible = true;   // Until it hides itself
    updateScript();

    showView();
    emit log(QString("Starting \"%1\": %2%3").arg(name).arg(url)
        .arg(m_server->isServerUrl(url) ? QString(" (%1)").arg(m_server->dvbUrl(url)) : QString()));
    emit appStarted(url, name);
    m_page->load(QUrl(url));
}

void DVBDemodHbbTV::loadFinished(bool ok)
{
    if (!isRunning()) {
        return;
    }

    if (!ok)
    {
        m_failed.start();

        // An AUTOSTART application that can't be loaded is tried again later, not over and over
        if (m_autostartApp) {
            m_autostartRetryAt = QDateTime::currentMSecsSinceEpoch() + m_retryMs;
        }

        const bool autostart = m_autostartApp;
        destroyApplication();
        m_autostartDisabled = false;

        if (!autostart) {
            check();
        }

        return;
    }

    m_loaded = true;
    m_loadedAt.start();
    makeTransparent();
    showView();
}

void DVBDemodHbbTV::destroyApplication()
{
    if (m_mediaId != 0)
    {
        emit mediaRequest("stop", m_mediaId, QString(), QMap<QString, QString>(), 0.0);
        m_mediaId = 0;
    }

    const bool wasRunning = isRunning();
    const bool wasAutostart = m_autostartApp;
    m_appUrl.clear();
    m_appName.clear();
    m_organisationId = 0;
    m_applicationId = 0;
    m_autostartApp = false;
    m_keyset = 0;
    m_loaded = false;
    setVisible(false);
    m_page->setUrl(QUrl("about:blank"));

    if (!m_videoArea.isNull())
    {
        m_videoArea = QRectF();
        emit videoAreaChanged(m_videoArea);
    }

    if (wasRunning) {
        emit appStopped();
    }

    // An application other than the AUTOSTART one exiting starts that again (TS 102 796 6.2.2.6); the AUTOSTART one
    // exiting itself isn't started again until the service changes
    m_autostartDisabled = wasAutostart;
}

void DVBDemodHbbTV::setVisible(bool visible)
{
    if (visible == m_visible) {
        return;
    }

    m_visible = visible;
    showView();
    emit visibleChanged(visible);
}

void DVBDemodHbbTV::consoleMessage(int level, const QString& message, int line, const QString& source)
{
    if (message.startsWith(m_messagePrefix))
    {
        const QJsonDocument document = QJsonDocument::fromJson(message.mid(m_messagePrefix.size()).toUtf8());

        if (document.isObject()) {
            this->message(document.object());
        }

        return;
    }

    logLimited(QString("Console%1: %2 (%3:%4)").arg(level >= 2 ? " error" : "").arg(message).arg(source).arg(line), source);
}

// Logged, unless one like it was a short while ago: applications log what they do often (as a player its position, a few
// times a second) or poll (as a carousel's file, every few seconds), which would fill the log. Like it: the same but
// for its numbers, from the same script. The next logged says how many weren't
void DVBDemodHbbTV::logLimited(const QString& text, const QString& from)
{
    if (!m_logLimited)
    {
        emit log(text);
        return;
    }

    static const QRegularExpression numbers("\\d+(\\.\\d+)?");
    const QString key = from + '\n' + QString(text).replace(numbers, "#");
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_repeated.find(key);

    if ((it != m_repeated.end()) && (now - it->m_loggedAt < m_repeatMs))
    {
        it->m_held++;
        return;
    }

    const int held = it != m_repeated.end() ? it->m_held : 0;
    emit log(held > 0 ? QString("%1 (and %2 like it in the last minute, not logged)").arg(text).arg(held) : text);

    // The old forgotten, now and then
    if (m_repeated.size() > 500)
    {
        for (auto old = m_repeated.begin(); old != m_repeated.end();) {
            old = now - old->m_loggedAt >= m_repeatMs ? m_repeated.erase(old) : std::next(old);
        }
    }

    m_repeated.insert(key, Repeated{now, 0});
}

// From oipf.js
void DVBDemodHbbTV::message(const QJsonObject& message)
{
    const QString type = message.value("t").toString();

    if (type == "keyset")
    {
        m_keyset = message.value("v").toInt();

        if (m_loaded) {
            m_keysetSinceLoad = true;
        }

        emit log(QString("Keyset 0x%1").arg(m_keyset, 0, 16));

        // Red pressed to start it: given now it's asked for, if that's soon after
        if (m_pendingRed && (m_keyset & m_keysetRed))
        {
            m_pendingRed = false;

            if (m_pendingRedTimer.elapsed() < 30000) {
                QTimer::singleShot(300, this, [this]() { pressRed(); });
            }
        }
    }
    else if (type == "show")
    {
        setVisible(true);
    }
    else if (type == "hide")
    {
        setVisible(false);
    }
    else if (type == "destroy")
    {
        emit log("Application destroyed itself");
        // Not from within the page's own call
        QTimer::singleShot(0, this, [this]() {
            const bool autostart = m_autostartApp;
            destroyApplication();

            if (!autostart) {
                check();
            }
        });
    }
    else if (type == "create")
    {
        const QString uri = message.value("uri").toString();
        QTimer::singleShot(0, this, [this, uri]() { createApplication(uri); });
    }
    else if (type == "setChannel")
    {
        emit log(QString("setChannel %1.%2.%3").arg(message.value("onid").toInt()).arg(message.value("tsid").toInt()).arg(message.value("sid").toInt()));
        emit channelChangeRequested(message.value("onid").toInt(-1), message.value("tsid").toInt(-1), message.value("sid").toInt());
    }
    else if (type == "drm")
    {
        // DRM system ids (DVB CA system ids, as urn:dvb:casystemid:19219): the ones applications use, by name
        static const QMap<QString, QString> names = {
            {"19219", "PlayReady"}, {"19156", "Widevine"}, {"19188", "Marlin"}};
        const QString id = message.value("system").toString();
        const QString number = id.section(':', -1);
        const QString system = names.value(number, id.isEmpty() ? QString("DRM") : id);
        emit log(QString("DRM message (%1) for %2: no DRM system, so protected video can't be played").arg(message.value("type").toString(), system));
        m_drmSystem = system;
        m_drmRequest.start();
        showStatus();
        emit drmRequested(system);
    }
    else if (type == "step")
    {
        emit channelStepRequested(message.value("d").toInt());
    }
    else if (type == "layout")
    {
        // Its html or body 1920 pixels wide: made for a 1920 by 1080 browser. Only as laid out in the viewport it's
        // zoomed to: while the view's resized it's briefly wider or narrower, and an application choosing its layout
        // by media queries (as TVP VOD's) would take the wider one, then keep it if it were zoomed for that
        const int width = message.value("w").toInt();
        const int viewport = message.value("vw").toInt();
        const int layoutWidth = (width >= 1900) && (width <= 1940) ? 1920 : m_defaultLayoutWidth;

        qDebug("DVBDemodHbbTV::message: layout %d in a viewport %d wide, zoomed for %d", width, viewport, m_layoutWidth);

        if ((viewport <= m_layoutWidth) && (viewport >= m_layoutWidth - 2) && (layoutWidth != m_layoutWidth))
        {
            m_layoutWidth = layoutWidth;
            applyZoom();
            emit log(QString("Laid out for %1 pixels wide: zoomed to fit").arg(layoutWidth));
        }
    }
    else if (type == "video")
    {
        QRectF area;

        if (!message.value("full").toBool() && !message.value("hidden").toBool())
        {
            area = QRectF(message.value("x").toDouble(), message.value("y").toDouble(), message.value("w").toDouble(), message.value("h").toDouble())
                .intersected(QRectF(0.0, 0.0, 1.0, 1.0));
        }

        if (area != m_videoArea)
        {
            m_videoArea = area;
            m_retransparentCount = 0;
            // Once the page's cleared its backgrounds over it, and Chromium's said what that makes the page's
            QTimer::singleShot(300, this, &DVBDemodHbbTV::keepTransparent);
            qDebug("DVBDemodHbbTV::message: video %s: %s", qPrintable(message.value("what").toString()),
                message.value("full").toBool() ? "full screen" : message.value("hidden").toBool() ? "hidden (full screen)"
                : qPrintable(QString("%1,%2 %3x%4").arg(area.x(), 0, 'f', 3).arg(area.y(), 0, 'f', 3).arg(area.width(), 0, 'f', 3).arg(area.height(), 0, 'f', 3)));
            emit videoAreaChanged(area);
        }
    }
    else if (type == "media")
    {
        const QString command = message.value("cmd").toString();
        const int id = message.value("id").toInt();
        QString url = message.value("url").toString();
        QMap<QString, QString> options;

        if (command == "load")
        {
            // Only by HTTP. SDRangel's player (FFmpeg) fetches it, not the browser, so the interceptor doesn't see it:
            // refused here as the browser's requests are when the internet's not allowed (but a carousel's file), and
            // other protocols (as file:, concat:) never, as the page could ask for anything
            const QUrl requested(url);
            const bool http = (requested.scheme() == "http") || (requested.scheme() == "https");
            const bool allowed = http && (m_interceptor->m_networkAllowed || m_server->isServerUrl(url));

            if (!allowed)
            {
                emit log(QString("Media %1 refused: %2").arg(id).arg(url));
                QJsonObject data;
                data.insert("code", http ? 2 : 4);     // MEDIA_ERR_NETWORK, MEDIA_ERR_SRC_NOT_SUPPORTED
                data.insert("message", http ? "The internet isn't allowed" : "Not an HTTP URL");
                mediaEvent(id, "error", data);
                return;
            }

            // And the protocols its demuxers open (as a manifest's segments) only those of HTTP
            options["protocol_whitelist"] = "http,https,tcp,tls,crypto,httpproxy";

            // FFmpeg fetches it as the page would: with its user agent, and a carousel's file from the server by the
            // loopback address (FFmpeg's resolver doesn't know *.localhost), its host given in the header
            options["user_agent"] = m_profile->httpUserAgent();
            QString headers = QString("Referer: %1\r\n").arg(m_page->url().toString());

            if (m_server->isServerUrl(url))
            {
                QUrl u(url);
                headers += QString("Host: %1:%2\r\n").arg(u.host()).arg(u.port());
                u.setHost("127.0.0.1");
                url = u.toString();
            }

            options["headers"] = headers;

            // One at a time: another element's stopped
            if ((m_mediaId != 0) && (m_mediaId != id)) {
                emit mediaRequest("stop", m_mediaId, QString(), QMap<QString, QString>(), 0.0);
            }

            m_mediaId = id;
        }
        else if (id != m_mediaId)
        {
            return;     // Of media no longer played
        }

        emit log(QString("Media %1 %2%3").arg(id).arg(command).arg(command == "load" ? " " + url
            : command == "seek" ? QString(" %1 s").arg(message.value("position").toDouble()) : QString()));
        emit mediaRequest(command, id, url, options, message.value("position").toDouble());

        if (command == "stop") {
            m_mediaId = 0;
        }
    }
    else if (type == "loaded")
    {
        emit log(QString("Loaded %1").arg(message.value("url").toString()));
    }
    else if (type == "error")
    {
        emit log(QString("Script error: %1 (%2:%3)").arg(message.value("message").toString())
            .arg(message.value("source").toString()).arg(message.value("line").toInt()));
    }
}

// Application.createApplication(): an application of the AIT, by its ids (dvb://current.ait/org.app, TS 102 851),
// a file of a carousel, or a URL
void DVBDemodHbbTV::createApplication(const QString& uri)
{
    emit log(QString("createApplication %1").arg(uri));
    const QString aitPrefix = "dvb://current.ait/";

    if (uri.startsWith(aitPrefix, Qt::CaseInsensitive))
    {
        const QString rest = uri.mid(aitPrefix.size());
        int end = rest.size();

        for (QChar c : QString("?#/"))
        {
            const int i = rest.indexOf(c);

            if (i >= 0) {
                end = std::min(end, i);
            }
        }

        const QStringList ids = rest.left(end).split('.');
        bool orgOK = false, appOK = false;
        const uint32_t organisationId = ids.size() == 2 ? ids[0].toUInt(&orgOK, 16) : 0;
        const int applicationId = ids.size() == 2 ? ids[1].toInt(&appOK, 16) : 0;

        if (orgOK && appOK)
        {
            for (const DTVApps::App& app : m_apps->getApps(m_channel.m_serviceId))
            {
                if ((app.m_organisationId == organisationId) && (app.m_applicationId == applicationId))
                {
                    launch(app, rest.mid(end).startsWith('/') ? QString() : rest.mid(end));
                    return;
                }
            }
        }

        emit log(QString("createApplication: %1 isn't in the AIT").arg(uri));
        return;
    }

    if (uri.startsWith("dvb://", Qt::CaseInsensitive))
    {
        const QString url = m_server->httpUrl(uri);

        if (!url.isEmpty()) {
            navigate(url, QString(), 0, 0, false);
        } else {
            emit log(QString("createApplication: can't load %1").arg(uri));
        }

        return;
    }

    const QUrl url(uri);

    if ((url.scheme() == "http") || (url.scheme() == "https")) {
        navigate(uri, QString(), 0, 0, false);
    } else if (url.isRelative() && isRunning()) {
        navigate(QUrl(m_appUrl).resolved(url).toString(), QString(), 0, 0, false);
    } else {
        emit log(QString("createApplication: can't load %1").arg(uri));
    }
}

// A page of the running application served as application/vnd.hbbtv.xhtml+xml, which the browser would download rather
// than show: fetched, and given to it as HTML. Its first page, or one it goes to, or one it's redirected to (as
// index.php to index.html)
void DVBDemodHbbTV::downloadRequested(const QUrl& url, const QString& mimeType)
{
    // Fetched without the interceptor, so the internet's only used if allowed
    const bool allowed = m_interceptor->m_networkAllowed || m_server->isServerUrl(url.toString());

    if (!isRunning() || !allowed || !(mimeType.contains("hbbtv") || mimeType.contains("xhtml") || mimeType.contains("cehtml")))
    {
        emit log(QString("Download of %1 (%2) refused").arg(url.toString()).arg(mimeType));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_profile->httpUserAgent());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy); // Qt 5's default is not to follow them
    QNetworkReply *reply = m_network->get(request);
    const QString appUrl = m_appUrl;

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, appUrl]() {
        reply->deleteLater();

        if ((reply->error() != QNetworkReply::NoError) || !isRunning() || (m_appUrl != appUrl))
        {
            emit log(QString("Can't load %1: %2").arg(url.toString()).arg(reply->errorString()));
            return;
        }


        // With oipf.js in it as well: given so, a page's scripts aren't always injected (Qt WebEngine doesn't inject
        // them in one given after another, as when the service changes from one application served so to another)
        const QByteArray page = withScript(reply->readAll(), scriptSource());

        if (page.toPercentEncoding().size() < m_setContentLimit)
        {
            m_page->setContent(page, "text/html", url);
            return;
        }

        // Too large to be given: a page that loads it (from where it is, so of its origin) and writes it into itself
        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        const QRegularExpressionMatch charset = QRegularExpression("charset\\s*=\\s*\"?([^\";\\s]+)", QRegularExpression::CaseInsensitiveOption).match(contentType);
        emit log(QString("%1 is too large to be given to the page (%2 bytes): loaded by a script").arg(url.toString()).arg(page.size()));
        m_page->setContent(loaderPage(url, charset.hasMatch() ? charset.captured(1) : QString("utf-8")), "text/html", url);
    });
}

QJsonObject DVBDemodHbbTV::channelJson(const Channel& channel) const
{
    QJsonObject json;
    json.insert("onid", channel.m_networkId >= 0 ? channel.m_networkId : m_apps->getOriginalNetworkId());
    json.insert("tsid", channel.m_transportStreamId >= 0 ? channel.m_transportStreamId : m_apps->getTransportStreamId());
    json.insert("sid", channel.m_serviceId);
    json.insert("name", channel.m_name);
    json.insert("lcn", channel.m_lcn);
    json.insert("radio", channel.m_radio);
    json.insert("hd", channel.m_hd);
    json.insert("idType", m_idType);
    return json;
}

QJsonObject DVBDemodHbbTV::configuration() const
{
    QJsonObject config;
    config.insert("channel", m_channel.m_serviceId != 0 ? QJsonValue(channelJson(m_channel)) : QJsonValue());
    QJsonArray channels;

    for (const Channel& channel : m_channels) {
        channels.append(channelJson(channel));
    }

    config.insert("channels", channels);
    QJsonArray programmes;

    for (const Programme& programme : m_programmes)
    {
        QJsonObject p;
        p.insert("id", programme.m_eventId);
        p.insert("name", programme.m_name);
        p.insert("description", programme.m_description);
        p.insert("start", (double) programme.m_start);
        p.insert("duration", programme.m_duration);
        programmes.append(p);
    }

    config.insert("programmes", programmes);
    config.insert("language", "eng");
    config.insert("country", "GBR");
    config.insert("version", QCoreApplication::applicationVersion());
    config.insert("serverPort", m_server->getPort());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Only if the internet's allowed: else requests to it fail as the browser's own do (not as the proxy's 404)
    if (m_interceptor->m_networkAllowed) {
        config.insert("proxy", m_server->getProxyPath());
    }
#endif
    return config;
}

// oipf.js, with the TV's state, as a script: escaped to ASCII (as a page's encoding may not be UTF-8), and without
// </ (so it can be in a page's <script> element)
QString DVBDemodHbbTV::scriptSource() const
{
    // Not by arg(): the script has %s of its own. The configuration's < escaped (only in its strings, as JSON has no
    // other): its programmes' names and descriptions are the broadcaster's, and <!-- with <script in a page's script
    // element would make the rest of the page part of it
    const QString config = QString::fromUtf8(QJsonDocument(configuration()).toJson(QJsonDocument::Compact)).replace("<", "\\u003c");
    const QString source = "window.__sdrangelHbbTVConfig = " + config + ";\n" + m_oipfScript;
    QString ascii;
    ascii.reserve(source.size());

    for (const QChar c : source)
    {
        if (c.unicode() > 0x7e) {
            ascii += QString("\\u%1").arg((uint) c.unicode(), 4, 16, QChar('0'));
        } else {
            ascii += c;
        }
    }

    return ascii.replace("</", "<\\/");
}

// A page with a script as its first: in its head, or after its html element or doctype if it has none
QByteArray DVBDemodHbbTV::withScript(const QByteArray& page, const QString& script)
{
    const QByteArray element = "<script>" + script.toLatin1() + "</script>";
    const QString text = QString::fromLatin1(page); // Only to find the tags, which are ASCII
    int at = -1;

    for (const QString& tag : {QString("<head"), QString("<html"), QString("<!doctype")})
    {
        const QRegularExpressionMatch match = QRegularExpression(tag + "(\\s[^>]*)?>", QRegularExpression::CaseInsensitiveOption).match(text);

        if (match.hasMatch())
        {
            at = match.capturedEnd();
            break;
        }
    }

    QByteArray result = page;
    result.insert(std::max(0, at), element);
    return result;
}

// A page that loads one too large to be given by setContent(): oipf.js, then its own script fetching the page (from
// where it is, its origin and URL the page's) and writing it into itself, as it would have been
QByteArray DVBDemodHbbTV::loaderPage(const QUrl& url, const QString& charset) const
{
    const QString quotedUrl = QString::fromUtf8(QJsonDocument(QJsonArray{url.toString(QUrl::FullyEncoded)}).toJson(QJsonDocument::Compact)).replace("<", "\\u003c");
    const QString quotedType = QString::fromUtf8(QJsonDocument(QJsonArray{QString("text/html; charset=%1").arg(charset)}).toJson(QJsonDocument::Compact))
        .replace("<", "\\u003c");
    const QString loader = QString(
        "(function () {"
        " var x = new XMLHttpRequest();"
        " x.open('GET', %1[0]);"
        " x.overrideMimeType(%2[0]);"
        " x.onload = function () { document.open(); document.write(x.responseText); document.close(); };"
        " x.onerror = function () { console.log('[sdrangel] the page could not be loaded'); };"
        " x.send();"
        " })();").arg(quotedUrl, quotedType);
    return "<!DOCTYPE html><html><head><script>" + scriptSource().toLatin1() + "</script><script>" + loader.toLatin1()
        + "</script></head><body></body></html>";
}

// oipf.js, with the TV's state, in each page before its scripts
void DVBDemodHbbTV::updateScript()
{
    QWebEngineScriptCollection& scripts = m_page->scripts();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QList<QWebEngineScript> oldScripts = scripts.find(m_scriptName);
#else
    const QList<QWebEngineScript> oldScripts = scripts.findScripts(m_scriptName);
#endif

    for (const QWebEngineScript& old : oldScripts) {
        scripts.remove(old);
    }

    QWebEngineScript script;
    script.setName(m_scriptName);
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(true);
    script.setSourceCode(scriptSource());
    scripts.insert(script);
}
