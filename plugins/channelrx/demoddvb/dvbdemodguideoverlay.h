///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_DVBDEMODGUIDEOVERLAY_H
#define INCLUDE_DVBDEMODGUIDEOVERLAY_H

#include <functional>
#include <vector>

#include <QColor>
#include <QFont>
#include <QImage>
#include <QRectF>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "dvbdemodepg.h"

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QPainter;
class GLVideoWidget;

// The programme guide over video (full screen, or in the GUI), as a TV shows it, drawn into an image the video widget
// shows over the picture. Two views:
//  - a now and next ribbon along the bottom, shown when the channel changes or on Info (I), which hides itself after
//    a few seconds: Up and Down browse other channels, Left and Right later programmes, OK (Enter) watches the
//    channel browsed
//  - the full guide (G): the programme selected in detail over the dimmed picture, and a grid of channels by time
//    below, with the time now marked: arrows move the selection, OK watches its channel, R records it
// With neither shown, Up and Down (or Page Up and Down) change channel, and digits enter a channel number. With the
// mouse: clicking the ribbon opens the guide; in the guide, clicking a programme or channel selects it,
// double-clicking watches it, buttons watch, record, close, page through the channels and step the time, and the
// wheel moves through the channels (with Shift, or sideways, the time). Right-clicking opens or closes the guide
class DVBDemodGuideOverlay : public QObject
{
    Q_OBJECT

public:
    typedef std::function<bool(int networkId, int serviceId, int eventId)> IsScheduled;
    typedef std::function<void(int networkId, int serviceId, int eventId)> ToggleRecording;

    explicit DVBDemodGuideOverlay(DVBDemodEPG *epg, QObject *parent = nullptr);

    //! To mark programmes to be recorded, and record them (R)
    void setRecordings(IsScheduled isScheduled, ToggleRecording toggle) { m_isScheduled = isScheduled; m_toggleRecording = toggle; }

    //! Where to draw, null for nowhere (hiding the guide)
    void setWidget(GLVideoWidget *widget);
    //! The service playing, in the multiplex being received
    void setPlaying(int serviceId) { m_playingService = serviceId; }
    void showRibbon();
    //! If the guide has the service playing, for the ribbon to show
    bool hasPlaying();
    void showGuide();
    void hide();
    bool isShown() const { return m_mode != Off; }
    bool isGuideShown() const { return m_mode == Guide; }
    //! True if the key was used
    bool handleKey(QKeyEvent *event);
    //! Press, double-click or move, at a position in the widget's coordinates. True if used
    bool handleMouse(QMouseEvent *event);
    bool handleWheel(QWheelEvent *event);
    //! Draw again if shown and the time has moved on (or the widget's size has changed), or if forced
    void refresh(bool force = false);
    //! What's shown, transparent where nothing is, as given to the widget
    QImage image(const QSize& size);

signals:
    void playService(int networkId, int serviceId);

private:
    enum Mode { Off, Ribbon, Guide };

    //! What's under the mouse, from the last image drawn
    struct HitArea
    {
        enum Kind { Event, Channel, Watch, Simulcast, Record, Close, PageUp, PageDown, Earlier, Later, RibbonBar, ChannelUp, ChannelDown };

        QRectF m_rect;          //!< In the image's pixels
        Kind m_kind;
        int m_row;
        int m_eventId;
    };

    //! Text with symbols, written as {dot}, {dash}, {up}, {down}, {left}, {right} and {rec}, so the source is ASCII
    static QString symbols(const char *text);
    //! Pixels per pixel of a 1920 x 1080 layout, no less than half
    static double scale(const QSize& size);
    static QFont font(double pixels, bool medium = false);
    static QString localTime(qint64 utc, const char *format = "HH:mm");
    static QString durationText(int seconds);
    //! Text, elided to fit width, with its top left at x, y
    static void drawText(QPainter& painter, int x, int y, int width, const QString& text, const QFont& f, const QColor& color);
    static void fillRounded(QPainter& painter, const QRectF& rect, double radius, const QColor& color);

    void loadServices();
    //! The service playing, in the multiplex being received or, until its SI arrives after retuning, the one played
    //! from here or last seen playing. -1 if not known
    int playingRow() const;
    std::vector<DVBDemodEPG::Event> events(int row) const;
    qint64 now() const;
    void render();
    void paintRibbon(QPainter& painter, const QSize& size, qint64 t);
    void paintGuide(QPainter& painter, const QSize& size, qint64 t);
    void paintClock(QPainter& painter, const QSize& size, qint64 t, double u, bool date);
    //! The logo, right-aligned to right and centred on centreY, at most width by height. Returns the width drawn, 0 if none
    double paintLogo(QPainter& painter, double right, double centreY, double width, double height);
    void paintBadges(QPainter& painter, int x, int y, double u, const DVBDemodEPG::Event& event, const QString& extra);
    //! A button, outlined, with a key's name before its label, right-aligned to right. Returns its width
    double paintButton(QPainter& painter, double right, double y, double u, const QString& key, const QString& label, HitArea::Kind kind);
    void addHit(const QRectF& rect, HitArea::Kind kind, int row = -1, int eventId = -1) { m_hits.push_back({rect, kind, row, eventId}); }
    const HitArea *hit(const QPointF& position) const;
    void stepWindow(int halfHours);
    QColor genreColor(int genre, int networkId) const;
    void moveGuideRow(int delta);
    void moveGuideEvent(int delta);
    void selectGuideEventAt(qint64 utc);
    void keepGuideSelectionVisible();
    void keepGuideEventVisible(qint64 start);
    void play(int row);
    //! The same programme on another service, as in HD (H), from the guide's event links. False if there's none
    bool playSimulcast(int networkId, int serviceId);
    //! Name of the service with a row's programme in HD (or another simulcast), empty if none
    QString simulcastName(int row) const;
    void channelStep(int delta);
    //! The ribbon's arrows: play the channel above or below the one it shows, keeping it up to step again
    void stepRibbon(int delta);
    void digit(int value);
    void toggleRecording();
    void restartHideTimer();
    void showMessage(const QString& message);

    DVBDemodEPG *m_epg;
    IsScheduled m_isScheduled;
    ToggleRecording m_toggleRecording;
    QPointer<GLVideoWidget> m_widget;
    Mode m_mode;
    int m_playingService;
    bool m_stepping;            //!< Stepping with the ribbon's arrows: it stays on the channel asked for, until hidden
    int m_playedNetworkId;      //!< The service last seen playing, or played from here, -1 if none
    int m_playedServiceId;
    std::vector<DVBDemodEPG::Service> m_services;  //!< By channel number
    int m_row;                  //!< Service browsed or selected
    int m_ribbonEvent;          //!< Programme on the ribbon: 0 for on now, 1 next...
    int m_guideEventId;         //!< Programme selected in the guide, -1 if none
    qint64 m_guideFocus;        //!< Time (UTC) the selection follows between rows
    qint64 m_windowStart;       //!< Of the guide's grid
    int m_firstRow;             //!< Shown at the top of the grid
    int m_visibleRows;
    QString m_digits;           //!< Channel number being entered
    QTimer m_hideTimer;
    QTimer m_digitTimer;
    //! A click on the video shows the ribbon, or off the ribbon or guide hides it, after the double-click interval,
    //! so a double-click (full screen) does neither
    QTimer m_clickTimer;
    std::vector<QRectF> m_drawn;    //!< Where the ribbon or guide was drawn, from the last image
    QImage m_logo;
    QImage m_logoScaled;            //!< At the height last drawn
    qint64 m_lastRender;        //!< Time drawn, to draw again as it moves on
    QSize m_renderSize;
    QString m_message;          //!< Shown on the ribbon or the guide, as why a channel can't be played
    Mode m_pressMode;           //!< Before the last mouse press, to ignore a double-click after one that opened or closed a view
    std::vector<HitArea> m_hits;

    static const int m_windowSeconds = 2 * 3600;
    static constexpr int m_ribbonSeconds = 6;
    static constexpr int m_guideSeconds = 120;
    static constexpr int m_digitMilliseconds = 1500;

    // Light text on translucent dark panels, so the picture shows through, and a colour per kind of programme
    static const QColor m_textColor;
    static const QColor m_bodyColor;
    static const QColor m_secondaryColor;
    static const QColor m_mutedColor;
    static const QColor m_accentColor;
    static const QColor m_warningColor;
    static const QColor m_recordColor;
    static const QColor m_ageColor;
    static const QColor m_panelColor;
    static const QColor m_dividerColor;
};

#endif // INCLUDE_DVBDEMODGUIDEOVERLAY_H
