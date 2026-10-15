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

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QStyleHints>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QPainterPath>

#include "gui/glvideowidget.h"

#include "dvbdemodguideoverlay.h"

// Light text on translucent dark panels, so the picture shows through, and a colour per kind of programme
const QColor DVBDemodGuideOverlay::m_textColor(0xF1, 0xEF, 0xE8);
const QColor DVBDemodGuideOverlay::m_bodyColor(0xD3, 0xD1, 0xC7);
const QColor DVBDemodGuideOverlay::m_secondaryColor(0xB4, 0xB2, 0xA9);
const QColor DVBDemodGuideOverlay::m_mutedColor(0x88, 0x87, 0x80);
const QColor DVBDemodGuideOverlay::m_accentColor(0x5D, 0xCA, 0xA5);
const QColor DVBDemodGuideOverlay::m_warningColor(0xEF, 0x9F, 0x27);
const QColor DVBDemodGuideOverlay::m_recordColor(0xE2, 0x4B, 0x4A);
const QColor DVBDemodGuideOverlay::m_ageColor(0xF0, 0x99, 0x7B);
const QColor DVBDemodGuideOverlay::m_panelColor(14, 14, 14, 205);
const QColor DVBDemodGuideOverlay::m_dividerColor(241, 239, 232, 50);

// Text with symbols, written as {dot}, {dash}, {up}, {down}, {left}, {right} and {rec}, so the source is ASCII
QString DVBDemodGuideOverlay::symbols(const char *text)
{
    QString s(text);
    s.replace("{dot}", QChar(0x00B7));
    s.replace("{dash}", QChar(0x2013));
    s.replace("{up}", QChar(0x25B2));
    s.replace("{down}", QChar(0x25BC));
    s.replace("{left}", QChar(0x25C0));
    s.replace("{right}", QChar(0x25B6));
    s.replace("{rec}", QChar(0x25CF));
    return s;
}

// Pixels per pixel of a 1920 x 1080 layout, no less than half, so text is readable in a small window, where the
// layout is more compact instead
double DVBDemodGuideOverlay::scale(const QSize& size)
{
    return std::max(0.5, std::min(size.height() / 1080.0, size.width() / 1920.0));
}

QFont DVBDemodGuideOverlay::font(double pixels, bool medium)
{
    QFont f;
    f.setPixelSize(std::max(8, (int) std::lround(pixels)));
    f.setWeight(medium ? QFont::Medium : QFont::Normal);
    return f;
}

QString DVBDemodGuideOverlay::localTime(qint64 utc, const char *format)
{
    return QDateTime::fromSecsSinceEpoch(utc).toLocalTime().toString(format);
}

QString DVBDemodGuideOverlay::durationText(int seconds)
{
    const int minutes = (seconds + 30) / 60;

    if (minutes < 60) {
        return QString("%1 min").arg(minutes);
    }

    return minutes % 60 == 0 ? QString("%1 hr").arg(minutes / 60) : QString("%1 hr %2").arg(minutes / 60).arg(minutes % 60);
}

// Text, elided to fit width, with its top left at x, y
void DVBDemodGuideOverlay::drawText(QPainter& painter, int x, int y, int width, const QString& text, const QFont& f, const QColor& color)
{
    const QFontMetrics metrics(f);
    painter.setFont(f);
    painter.setPen(color);
    painter.drawText(x, y + metrics.ascent(), metrics.elidedText(text, Qt::ElideRight, std::max(0, width)));
}

void DVBDemodGuideOverlay::fillRounded(QPainter& painter, const QRectF& rect, double radius, const QColor& color)
{
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    painter.fillPath(path, color);
}

DVBDemodGuideOverlay::DVBDemodGuideOverlay(DVBDemodEPG *epg, QObject *parent) :
    QObject(parent),
    m_epg(epg),
    m_mode(Off),
    m_playingService(0),
    m_stepping(false),
    m_playedNetworkId(-1),
    m_playedServiceId(-1),
    m_row(0),
    m_ribbonEvent(0),
    m_guideEventId(-1),
    m_guideFocus(0),
    m_windowStart(0),
    m_firstRow(0),
    m_visibleRows(6),
    m_lastRender(0),
    m_pressMode(Off)
{
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, &DVBDemodGuideOverlay::hide);
    m_digitTimer.setSingleShot(true);
    connect(&m_digitTimer, &QTimer::timeout, this, [this]() { digit(-1); });
    m_clickTimer.setSingleShot(true);
    connect(&m_clickTimer, &QTimer::timeout, this, [this]() {
        if (m_mode == Off) {
            showRibbon();
        } else {
            hide();
        }
    });
}

void DVBDemodGuideOverlay::setWidget(GLVideoWidget *widget)
{
    if (m_widget && (m_widget != widget)) {
        m_widget->setGuide(QImage());
    }

    m_widget = widget;

    if (!m_widget) {
        hide();
    } else {
        render(); // What's shown, there
    }
}

qint64 DVBDemodGuideOverlay::now() const
{
    const qint64 t = m_epg->getTime();
    return t > 0 ? t : QDateTime::currentSecsSinceEpoch();
}

// TV and radio services that can be played (received, or on a multiplex of the area, retuned to or searched for if its
// frequency isn't known), and not hidden by their channel's visible_service_flag, by channel number. The one browsed or
// selected, and the one at the top of the guide's grid, stay so
void DVBDemodGuideOverlay::loadServices()
{
    auto ids = [this](int row) {
        return (row >= 0) && (row < (int) m_services.size())
            ? std::make_pair(m_services[row].m_networkId, m_services[row].m_serviceId) : std::make_pair(-1, -1);
    };
    const std::pair<int, int> selected = ids(m_row), first = ids(m_firstRow);

    m_services = m_epg->getServices();
    m_services.erase(std::remove_if(m_services.begin(), m_services.end(), [](const DVBDemodEPG::Service& s) {
        return s.m_name.isEmpty() || !s.m_visible || (s.m_type == 0x0C) || (s.m_type == 0x10) || (!s.m_actual && !s.m_multiplexKnown);
    }), m_services.end());
    std::sort(m_services.begin(), m_services.end(), [](const DVBDemodEPG::Service& a, const DVBDemodEPG::Service& b) {
        const int ca = a.m_channel >= 0 ? a.m_channel : 0x7FFFFFFF, cb = b.m_channel >= 0 ? b.m_channel : 0x7FFFFFFF;
        return ca != cb ? ca < cb : a.m_name < b.m_name;
    });
    m_row = 0;
    m_firstRow = 0;

    for (int i = 0; i < (int) m_services.size(); i++)
    {
        const std::pair<int, int> s = ids(i);

        if (s == selected) {
            m_row = i;
        }

        if (s == first) {
            m_firstRow = i;
        }

        if (m_services[i].m_actual && (m_services[i].m_serviceId == m_playingService))
        {
            m_playedNetworkId = s.first;
            m_playedServiceId = s.second;
        }
    }
}

int DVBDemodGuideOverlay::playingRow() const
{
    int remembered = -1;

    for (int i = 0; i < (int) m_services.size(); i++)
    {
        const DVBDemodEPG::Service& s = m_services[i];

        if (s.m_actual && (s.m_serviceId == m_playingService)) {
            return i;
        }

        if ((s.m_networkId == m_playedNetworkId) && (s.m_serviceId == m_playedServiceId)) {
            remembered = i;
        }
    }

    return remembered;
}

// A service's programmes, in order: the schedule, and for the multiplex being received, now and next too, which
// may be all there is
std::vector<DVBDemodEPG::Event> DVBDemodGuideOverlay::events(int row) const
{
    if ((row < 0) || (row >= (int) m_services.size())) {
        return {};
    }

    const DVBDemodEPG::Service& s = m_services[row];
    std::vector<DVBDemodEPG::Event> events = m_epg->getEvents(s.m_networkId, s.m_serviceId);
    DVBDemodEPG::Event present, following;

    if (s.m_actual && m_epg->getNowNext(s.m_serviceId, present, following))
    {
        // Instead of the schedule's copies, which may have the times as planned rather than as broadcast
        for (const DVBDemodEPG::Event *e : {&present, &following})
        {
            if (e->isValid())
            {
                events.erase(std::remove_if(events.begin(), events.end(), [e](const DVBDemodEPG::Event& x) {
                    return (x.m_eventId == e->m_eventId) || (x.m_start == e->m_start);
                }), events.end());
                events.push_back(*e);
            }
        }

        std::sort(events.begin(), events.end(), [](const DVBDemodEPG::Event& a, const DVBDemodEPG::Event& b) { return a.m_start < b.m_start; });
    }

    return events;
}

bool DVBDemodGuideOverlay::hasPlaying()
{
    loadServices();

    // The rows may have moved, so what's shown and its hit areas are made again
    if (m_mode != Off) {
        render();
    }

    return playingRow() >= 0;
}

void DVBDemodGuideOverlay::showRibbon()
{
    // Shown again as each service starts playing: while stepping with the arrows, on the last asked for, not the
    // one that's caught up
    const bool stepping = (m_mode == Ribbon) && m_stepping;
    loadServices();

    if (!stepping) {
        m_row = std::max(0, playingRow());
    }

    m_ribbonEvent = 0;
    m_message.clear();
    m_mode = Ribbon;
    restartHideTimer();
    render();
}

void DVBDemodGuideOverlay::showGuide()
{
    if (m_mode != Ribbon)
    {
        loadServices();
        m_row = std::max(0, playingRow());
    }

    const qint64 t = now();
    m_windowStart = t - t % 1800;
    m_guideFocus = t;
    m_firstRow = std::max(0, m_row - m_visibleRows / 2);
    m_message.clear();
    m_mode = Guide;
    selectGuideEventAt(t);
    keepGuideSelectionVisible();
    restartHideTimer();
    render();
}

void DVBDemodGuideOverlay::hide()
{
    m_mode = Off;
    m_stepping = false;
    m_hideTimer.stop();
    render(); // The channel number being entered, if any
}

void DVBDemodGuideOverlay::restartHideTimer()
{
    m_hideTimer.start((m_mode == Guide ? m_guideSeconds : m_ribbonSeconds) * 1000);
}

// On the guide if it's shown, else on the ribbon
void DVBDemodGuideOverlay::showMessage(const QString& message)
{
    if (m_mode == Off) {
        showRibbon();
    }

    m_message = message;
    restartHideTimer();
    render();
}

void DVBDemodGuideOverlay::refresh(bool force)
{
    if (((m_mode == Off) && m_digits.isEmpty()) || !m_widget) {
        return;
    }

    if (force || (now() - m_lastRender >= 30) || (m_widget->pixelSize() != m_renderSize)) {
        render();
    }
}

bool DVBDemodGuideOverlay::handleKey(QKeyEvent *event)
{
    const int key = event->key();
    const bool ok = (key == Qt::Key_Return) || (key == Qt::Key_Enter) || (key == Qt::Key_Select);
    const bool back = (key == Qt::Key_Escape) || (key == Qt::Key_Back) || (key == Qt::Key_Backspace);

    if ((key >= Qt::Key_0) && (key <= Qt::Key_9) && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)))
    {
        digit(key - Qt::Key_0);
        return true;
    }

    if (!m_digits.isEmpty() && ok)
    {
        digit(-1);
        return true;
    }

    switch (m_mode)
    {
    case Off:
        if (ok || (key == Qt::Key_I) || (key == Qt::Key_Info)) {
            showRibbon();
        } else if ((key == Qt::Key_G) || (key == Qt::Key_Guide)) {
            showGuide();
        } else if (key == Qt::Key_H) {
            if (!playSimulcast(m_epg->getNetworkId(), m_playingService)) {
                showMessage("Not available in HD");
            }
        } else if ((key == Qt::Key_Up) || (key == Qt::Key_PageUp) || (key == Qt::Key_ChannelUp)) {
            channelStep(1);
        } else if ((key == Qt::Key_Down) || (key == Qt::Key_PageDown) || (key == Qt::Key_ChannelDown)) {
            channelStep(-1);
        } else {
            return false;
        }

        return true;

    case Ribbon:
    {
        const int n = (int) m_services.size();

        if (back || (key == Qt::Key_I) || (key == Qt::Key_Info))
        {
            hide();
            return true;
        }

        if ((key == Qt::Key_G) || (key == Qt::Key_Guide))
        {
            showGuide();
            return true;
        }

        if (ok)
        {
            play(m_row);
            return true;
        }

        if ((key == Qt::Key_H) && (m_row >= 0) && (m_row < n))
        {
            if (!playSimulcast(m_services[m_row].m_networkId, m_services[m_row].m_serviceId)) {
                showMessage("Not available in HD");
            }

            return true;
        }

        if ((key == Qt::Key_PageUp) || (key == Qt::Key_ChannelUp)) {
            channelStep(1);
            return true;
        }

        if ((key == Qt::Key_PageDown) || (key == Qt::Key_ChannelDown)) {
            channelStep(-1);
            return true;
        }

        if ((key == Qt::Key_Up) && (n > 0)) {
            m_row = (m_row + 1) % n;
            m_ribbonEvent = 0;
        } else if ((key == Qt::Key_Down) && (n > 0)) {
            m_row = (m_row + n - 1) % n;
            m_ribbonEvent = 0;
        } else if (key == Qt::Key_Left) {
            m_ribbonEvent = std::max(0, m_ribbonEvent - 1);
        } else if (key == Qt::Key_Right) {
            m_ribbonEvent++; // Limited when drawn
        } else {
            return false;
        }

        m_message.clear();
        restartHideTimer();
        render();
        return true;
    }

    case Guide:
        if (back || (key == Qt::Key_G) || (key == Qt::Key_Guide))
        {
            hide();
            return true;
        }

        if (ok)
        {
            play(m_row);
            return true;
        }

        if (key == Qt::Key_H)
        {
            if ((m_row < 0) || (m_row >= (int) m_services.size())
                || !playSimulcast(m_services[m_row].m_networkId, m_services[m_row].m_serviceId)) {
                showMessage("Not available in HD");
            }

            return true;
        }

        m_message.clear();

        if (key == Qt::Key_Up) {
            moveGuideRow(-1);
        } else if (key == Qt::Key_Down) {
            moveGuideRow(1);
        } else if (key == Qt::Key_PageUp) {
            moveGuideRow(-m_visibleRows);
        } else if (key == Qt::Key_PageDown) {
            moveGuideRow(m_visibleRows);
        } else if (key == Qt::Key_Left) {
            moveGuideEvent(-1);
        } else if (key == Qt::Key_Right) {
            moveGuideEvent(1);
        } else if ((key == Qt::Key_R) || (key == Qt::Key_MediaRecord)) {
            toggleRecording();
        } else {
            return false;
        }

        restartHideTimer();
        render();
        return true;
    }

    return false;
}

// Up to three digits of a channel number, played when complete, after a pause, or on OK (value < 0)
void DVBDemodGuideOverlay::digit(int value)
{
    if (value >= 0)
    {
        m_digits += QString::number(value);
        m_digitTimer.start(m_digitMilliseconds);

        if (m_digits.size() < 3)
        {
            render();
            return;
        }
    }

    m_digitTimer.stop();
    const int channel = m_digits.toInt();
    m_digits.clear();
    loadServices();

    for (int i = 0; i < (int) m_services.size(); i++)
    {
        if (m_services[i].m_channel == channel)
        {
            play(i);
            return;
        }
    }

    showRibbon();
    m_message = QString("No channel %1").arg(channel);
    render();
}

void DVBDemodGuideOverlay::channelStep(int delta)
{
    loadServices();
    const int n = (int) m_services.size();

    if (n == 0) {
        return;
    }

    // Not until the service playing is known, as just after retuning to another multiplex
    const int row = playingRow();

    if (row >= 0) {
        play((row + delta + n) % n);
    }
}

void DVBDemodGuideOverlay::stepRibbon(int delta)
{
    const int n = (int) m_services.size();

    if (n == 0) {
        return;
    }

    m_row = (std::max(0, std::min(m_row, n - 1)) + delta + n) % n;
    m_ribbonEvent = 0;
    m_stepping = true;
    const DVBDemodEPG::Service& s = m_services[m_row];

    if (!(s.m_actual && (s.m_serviceId == m_playingService)))
    {
        m_playedNetworkId = s.m_networkId;
        m_playedServiceId = s.m_serviceId;
        emit playService(s.m_networkId, s.m_serviceId);
    }
}

// The ribbon is shown again once the service changes
void DVBDemodGuideOverlay::play(int row)
{
    if ((row < 0) || (row >= (int) m_services.size()))
    {
        hide();
        return;
    }

    const DVBDemodEPG::Service& s = m_services[row];
    const bool playing = s.m_actual && (s.m_serviceId == m_playingService);
    hide();

    if (!playing)
    {
        // The service playing until the transport stream it's on is received
        m_playedNetworkId = s.m_networkId;
        m_playedServiceId = s.m_serviceId;
        emit playService(s.m_networkId, s.m_serviceId);
    }
}

bool DVBDemodGuideOverlay::playSimulcast(int networkId, int serviceId)
{
    DVBDemodEPG::Service simulcast;

    if (!m_epg->getSimulcast(networkId, serviceId, simulcast) || (simulcast.m_actual && (simulcast.m_serviceId == m_playingService))) {
        return false;
    }

    hide();
    m_playedNetworkId = simulcast.m_networkId;
    m_playedServiceId = simulcast.m_serviceId;
    emit playService(simulcast.m_networkId, simulcast.m_serviceId);
    return true;
}

QString DVBDemodGuideOverlay::simulcastName(int row) const
{
    DVBDemodEPG::Service simulcast;

    if ((row < 0) || (row >= (int) m_services.size())
        || !m_epg->getSimulcast(m_services[row].m_networkId, m_services[row].m_serviceId, simulcast)) {
        return QString();
    }

    return simulcast.m_name;
}

void DVBDemodGuideOverlay::toggleRecording()
{
    if ((m_row < 0) || (m_row >= (int) m_services.size()) || (m_guideEventId < 0)) {
        return;
    }

    const DVBDemodEPG::Service& s = m_services[m_row];

    if (m_toggleRecording) {
        m_toggleRecording(s.m_networkId, s.m_serviceId, m_guideEventId);
    }
}

// The programme on at a time, or the next after it, or the last before it
void DVBDemodGuideOverlay::selectGuideEventAt(qint64 utc)
{
    const std::vector<DVBDemodEPG::Event> evs = events(m_row);
    m_guideEventId = -1;

    for (const auto& e : evs)
    {
        if (e.end() > utc)
        {
            m_guideEventId = e.m_eventId;
            return;
        }
    }

    if (!evs.empty()) {
        m_guideEventId = evs.back().m_eventId;
    }
}

void DVBDemodGuideOverlay::moveGuideRow(int delta)
{
    const int n = (int) m_services.size();

    if (n == 0) {
        return;
    }

    m_row = std::max(0, std::min(n - 1, m_row + delta));
    selectGuideEventAt(std::max(m_guideFocus, now()));
    keepGuideSelectionVisible();

    for (const auto& e : events(m_row))
    {
        if (e.m_eventId == m_guideEventId) {
            keepGuideEventVisible(e.m_start);
        }
    }
}

// Along the row, scrolling the grid by half hours to keep the programme in view. Not back before the half hour now
void DVBDemodGuideOverlay::moveGuideEvent(int delta)
{
    const std::vector<DVBDemodEPG::Event> evs = events(m_row);
    const qint64 t = now();
    const qint64 earliest = t - t % 1800;
    int index = -1;

    for (int i = 0; i < (int) evs.size(); i++)
    {
        if (evs[i].m_eventId == m_guideEventId) {
            index = i;
        }
    }

    if (index < 0)
    {
        selectGuideEventAt(t);
        return;
    }

    index = std::max(0, std::min((int) evs.size() - 1, index + delta));

    while ((index + 1 < (int) evs.size()) && (evs[index].end() <= earliest)) {
        index++;
    }

    const DVBDemodEPG::Event& e = evs[index];
    m_guideEventId = e.m_eventId;
    m_guideFocus = std::max(e.m_start, t);
    keepGuideEventVisible(e.m_start);
}

void DVBDemodGuideOverlay::keepGuideEventVisible(qint64 start)
{
    const qint64 t = now();
    const qint64 earliest = t - t % 1800;

    while (start >= m_windowStart + m_windowSeconds - 1800) {
        m_windowStart += 1800;
    }

    while ((start < m_windowStart) && (m_windowStart > earliest)) {
        m_windowStart -= 1800;
    }
}

void DVBDemodGuideOverlay::keepGuideSelectionVisible()
{
    if (m_row < m_firstRow) {
        m_firstRow = m_row;
    } else if (m_row >= m_firstRow + m_visibleRows) {
        m_firstRow = m_row - m_visibleRows + 1;
    }

    m_firstRow = std::max(0, std::min(m_firstRow, (int) m_services.size() - m_visibleRows));
}

double DVBDemodGuideOverlay::paintButton(QPainter& painter, double right, double y, double u, const QString& key,
    const QString& label, HitArea::Kind kind)
{
    const QFont keyFont = font(19 * u, true);
    const QFont labelFont = font(21 * u);
    const double keyWidth = QFontMetrics(keyFont).horizontalAdvance(key);
    const double labelWidth = QFontMetrics(labelFont).horizontalAdvance(label);
    const double width = keyWidth + labelWidth + 44 * u;
    const QRectF r(right - width, y, width, 38 * u);
    fillRounded(painter, r, 19 * u, QColor(14, 14, 14, 190));
    painter.setPen(QPen(QColor(241, 239, 232, 70), std::max(1.0, 1.5 * u)));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(r, 19 * u, 19 * u);
    painter.setFont(keyFont);
    painter.setPen(m_accentColor);
    painter.drawText(QRectF(r.x() + 16 * u, r.y(), keyWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter, key);
    painter.setFont(labelFont);
    painter.setPen(m_textColor);
    painter.drawText(QRectF(r.x() + 28 * u + keyWidth, r.y(), labelWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter, label);
    addHit(r, kind);
    return width;
}

// The topmost: buttons are drawn after the grid they may overlap
const DVBDemodGuideOverlay::HitArea *DVBDemodGuideOverlay::hit(const QPointF& position) const
{
    for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it)
    {
        if (it->m_rect.contains(position)) {
            return &*it;
        }
    }

    return nullptr;
}

// Scroll the grid by half hours (not before the half hour now), selecting the programme at its start, or now
void DVBDemodGuideOverlay::stepWindow(int halfHours)
{
    const qint64 t = now();
    m_windowStart = std::max(t - t % 1800, m_windowStart + halfHours * 1800);
    m_guideFocus = std::max(m_windowStart, t);
    selectGuideEventAt(m_guideFocus);
}

bool DVBDemodGuideOverlay::handleMouse(QMouseEvent *event)
{
    const double ratio = m_widget ? m_widget->devicePixelRatioF() : 1.0; // None when testing
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF position = event->position() * ratio;
#else
    const QPointF position = event->localPos() * ratio;
#endif
    const QEvent::Type type = event->type();

    // Keep it open while the mouse is being used
    if (type == QEvent::MouseMove)
    {
        if (m_mode != Off) {
            restartHideTimer();
        }

        return false;
    }

    // A double-click's second click comes as MouseButtonDblClick, after its first as a press. If that opened or closed
    // the guide or ribbon (or played a channel), what the second hits is something else: it's ignored, rather than
    // playing whatever's now under the pointer, or leaving full screen
    if (type == QEvent::MouseButtonPress) {
        m_pressMode = m_mode;
    } else if ((type == QEvent::MouseButtonDblClick) && (m_mode != m_pressMode)) {
        return true;
    }

    if ((type == QEvent::MouseButtonPress) && (event->button() == Qt::RightButton))
    {
        if (m_mode == Guide) {
            hide();
        } else {
            showGuide();
        }

        return true;
    }

    if (event->button() != Qt::LeftButton) {
        return false;
    }

    const HitArea *area = (m_mode != Off) ? hit(position) : nullptr;

    if (!area)
    {
        // A click on the video shows the ribbon (now and next), and one off the ribbon or guide hides it, once it's
        // clear it isn't the first of a double-click, which is left for full screen
        if (type == QEvent::MouseButtonDblClick)
        {
            m_clickTimer.stop();
            return false;
        }

        const bool onDrawn = std::any_of(m_drawn.begin(), m_drawn.end(), [&position](const QRectF& r) { return r.contains(position); });

        if ((type == QEvent::MouseButtonPress) && ((m_mode == Off) || !onDrawn)) {
            m_clickTimer.start(QGuiApplication::styleHints()->mouseDoubleClickInterval());
        }

        return false;
    }

    const HitArea h = *area; // Drawing again replaces the hit areas
    const bool doubleClick = type == QEvent::MouseButtonDblClick;
    m_message.clear();

    switch (h.m_kind)
    {
    case HitArea::RibbonBar:
        if (!doubleClick) {
            showGuide();
        }
        return true;

    case HitArea::ChannelUp: // A quick second click (a double-click) steps again
        stepRibbon(1);
        break;

    case HitArea::ChannelDown:
        stepRibbon(-1);
        break;

    case HitArea::Event:
        if (doubleClick)
        {
            play(h.m_row);
            return true;
        }

        m_row = h.m_row;
        m_guideEventId = h.m_eventId;

        for (const auto& e : events(m_row))
        {
            if (e.m_eventId == h.m_eventId) {
                m_guideFocus = std::max(e.m_start, now());
            }
        }

        keepGuideSelectionVisible();
        break;

    case HitArea::Channel:
        if (doubleClick)
        {
            play(h.m_row);
            return true;
        }

        moveGuideRow(h.m_row - m_row);
        break;

    case HitArea::Watch:
        play(m_row);
        return true;

    case HitArea::Simulcast:
        if ((m_row < 0) || (m_row >= (int) m_services.size())
            || !playSimulcast(m_services[m_row].m_networkId, m_services[m_row].m_serviceId)) {
            showMessage("Not available in HD");
        }
        return true;

    case HitArea::Record:
        toggleRecording();
        break;

    case HitArea::Close:
        hide();
        return true;

    case HitArea::PageUp:
        moveGuideRow(-m_visibleRows);
        break;

    case HitArea::PageDown:
        moveGuideRow(m_visibleRows);
        break;

    case HitArea::Earlier:
        stepWindow(-1);
        break;

    case HitArea::Later:
        stepWindow(1);
        break;
    }

    restartHideTimer();
    render();
    return true;
}

// A row a notch, or with Shift or sideways, half an hour
bool DVBDemodGuideOverlay::handleWheel(QWheelEvent *event)
{
    if (m_mode != Guide) {
        return false;
    }

    const QPoint delta = event->angleDelta();
    const bool sideways = (delta.x() != 0) || (event->modifiers() & Qt::ShiftModifier);
    const int amount = delta.x() != 0 ? delta.x() : delta.y();
    const int notches = amount > 0 ? -std::max(1, amount / 120) : std::max(1, -amount / 120);

    if (amount == 0) {
        return true;
    }

    if (sideways) {
        stepWindow(notches);
    } else {
        moveGuideRow(notches);
    }

    restartHideTimer();
    render();
    return true;
}

// By the content's level 1 nibble: the UK's D-Book differs from EN 300 468 from 6 on
QColor DVBDemodGuideOverlay::genreColor(int genre, int networkId) const
{
    static const QColor gray(0x88, 0x87, 0x80);
    static const QColor drama(0x7F, 0x77, 0xDD);
    static const QColor news(0x37, 0x8A, 0xDD);
    static const QColor entertainment(0xBA, 0x75, 0x17);
    static const QColor sport(0xD8, 0x5A, 0x30);
    static const QColor children(0xD4, 0x53, 0x7E);
    static const QColor factual(0x1D, 0x9E, 0x75);
    static const QColor leisure(0x63, 0x99, 0x22);

    if (genre < 0) {
        return gray;
    }

    const int level1 = (genre >> 4) & 0x0F;
    const bool uk = networkId == 0x233A;

    switch (level1)
    {
    case 1: return drama;
    case 2: return news;
    case 3: return entertainment;
    case 4: return sport;
    case 5: return children;
    case 6: return uk ? factual : children;    // Education, or music
    case 7: return uk ? leisure : factual;     // Lifestyle, or arts
    case 8: return uk ? drama : news;          // Drama, or social and political
    case 9: return factual;
    case 10: return leisure;
    default: return gray;
    }
}

// Into an image the size of the widget, which shows it over the video
void DVBDemodGuideOverlay::render()
{
    if (!m_widget) {
        return;
    }

    const QSize size = m_widget->pixelSize();
    m_renderSize = size;
    m_lastRender = now();
    m_widget->setGuide(((m_mode == Off) && m_digits.isEmpty()) || size.isEmpty() ? QImage() : image(size));
}

QImage DVBDemodGuideOverlay::image(const QSize& size)
{
    m_hits.clear();
    m_drawn.clear();
    QImage image(size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const qint64 t = now();

    if (m_mode == Ribbon) {
        paintRibbon(painter, size, t);
    } else if (m_mode == Guide) {
        paintGuide(painter, size, t);
    }

    // The channel number being entered, top left
    if (!m_digits.isEmpty())
    {
        const double u = scale(size);
        const QFont f = font(72 * u, true);
        const QFontMetrics metrics(f);
        const QString text = m_digits + QString(3 - m_digits.size(), QChar('-'));
        const QRectF box(48 * u, 40 * u, metrics.horizontalAdvance(text) + 48 * u, metrics.height() + 24 * u);
        fillRounded(painter, box, 16 * u, m_panelColor);
        m_drawn.push_back(box);
        painter.setFont(f);
        painter.setPen(m_textColor);
        painter.drawText(box, Qt::AlignCenter, text);
    }

    return image;
}

void DVBDemodGuideOverlay::paintClock(QPainter& painter, const QSize& size, qint64 t, double u, bool date)
{
    const QString text = date ? localTime(t, "ddd d MMM  HH:mm") : localTime(t);
    const QFont f = font(28 * u, date);
    const QFontMetrics metrics(f);
    const double width = metrics.horizontalAdvance(text) + 32 * u;
    const QRectF box(size.width() - 48 * u - width, 40 * u, width, metrics.height() + 14 * u);
    fillRounded(painter, box, 10 * u, QColor(12, 12, 12, 150));
    painter.setFont(f);
    painter.setPen(m_textColor);
    painter.drawText(box, Qt::AlignCenter, text);
}

double DVBDemodGuideOverlay::paintLogo(QPainter& painter, double right, double centreY, double width, double height)
{
    if (m_logo.isNull()) {
        m_logo = QImage(":/demoddvb/images/angeltv.png");
    }

    if (m_logo.isNull() || (width < 1.0) || (height < 1.0)) {
        return 0.0;
    }

    // Scaled smoothly once for the size, rather than by the painter each time
    const double scale = std::min(width / m_logo.width(), height / m_logo.height());
    const QSize size((int) std::lround(m_logo.width() * scale), (int) std::lround(m_logo.height() * scale));

    if (m_logoScaled.size() != size) {
        m_logoScaled = m_logo.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    painter.drawImage(QPointF(right - m_logoScaled.width(), centreY - m_logoScaled.height() / 2.0), m_logoScaled);
    return m_logoScaled.width();
}

// HD, subtitles, audio description and age as outlined badges, then extra text
void DVBDemodGuideOverlay::paintBadges(QPainter& painter, int x, int y, double u, const DVBDemodEPG::Event& event, const QString& extra)
{
    const QFont f = font(19 * u);
    const QFontMetrics metrics(f);
    const double height = metrics.height() + 4 * u;
    painter.setFont(f);
    QStringList badges;

    if (event.m_hd) {
        badges.append("HD");
    }

    if (event.m_subtitles) {
        badges.append("S");
    }

    if (event.m_audioDescription) {
        badges.append("AD");
    }

    double px = x;
    auto badge = [&](const QString& text, const QColor& border, const QColor& color) {
        const QRectF r(px, y, metrics.horizontalAdvance(text) + 14 * u, height);
        painter.setPen(QPen(border, std::max(1.0, 1.5 * u)));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(r, 5 * u, 5 * u);
        painter.setPen(color);
        painter.drawText(r, Qt::AlignCenter, text);
        px += r.width() + 8 * u;
    };

    for (const QString& b : badges) {
        badge(b, m_mutedColor, m_bodyColor);
    }

    if (event.m_minimumAge > 0) {
        badge(QString::number(event.m_minimumAge), m_ageColor, QColor(0xF5, 0xC4, 0xB3));
    }

    if (!extra.isEmpty())
    {
        painter.setPen(m_secondaryColor);
        painter.drawText(QRectF(px + 4 * u, y, 2000 * u, height), Qt::AlignLeft | Qt::AlignVCenter, extra);
    }
}

// Channel, programme on now (or browsed) with its progress, and the next, along the bottom
void DVBDemodGuideOverlay::paintRibbon(QPainter& painter, const QSize& size, qint64 t)
{
    const double u = scale(size);
    const double margin = 48 * u;
    const double height = 186 * u;
    const double pad = 26 * u;
    const QRectF bar(margin, size.height() - margin - height, size.width() - 2 * margin, height);
    fillRounded(painter, bar, 18 * u, m_panelColor);
    addHit(bar, HitArea::RibbonBar);
    m_drawn.push_back(bar);
    paintClock(painter, size, t, u, false);

    if (m_services.empty())
    {
        drawText(painter, bar.x() + pad, bar.y() + pad, bar.width() - 2 * pad, "No programme guide yet", font(32 * u, true), m_textColor);
        return;
    }

    const DVBDemodEPG::Service& s = m_services[std::min(m_row, (int) m_services.size() - 1)];

    // Channel
    const double channelWidth = 210 * u;
    const QRectF channelRect(bar.x() + pad, bar.y() + pad, channelWidth - pad, height - 2 * pad);
    painter.setPen(m_textColor);
    painter.setFont(font(58 * u, true));
    const QFontMetrics numberMetrics(painter.font());
    painter.drawText(QRectF(channelRect.x(), channelRect.y() - 10 * u, channelRect.width(), numberMetrics.height()),
        Qt::AlignHCenter | Qt::AlignTop, s.m_channel >= 0 ? QString::number(s.m_channel) : QString());
    const QFont nameFont = font(24 * u);
    const QFontMetrics nameMetrics(nameFont);
    painter.setFont(nameFont);
    painter.setPen(m_secondaryColor);
    painter.drawText(QRectF(channelRect.x(), channelRect.y() + numberMetrics.height() - 12 * u, channelRect.width(), nameMetrics.height()),
        Qt::AlignHCenter | Qt::AlignTop, nameMetrics.elidedText(s.m_name, Qt::ElideRight, (int) channelRect.width()));

    // Up and down a channel, as the keys, and between them, whether the channel shown is on another multiplex
    const double arrowWidth = 44 * u, arrowHeight = 32 * u;
    const QRectF upRect(channelRect.x(), channelRect.bottom() - arrowHeight, arrowWidth, arrowHeight);
    const QRectF downRect(channelRect.right() - arrowWidth, channelRect.bottom() - arrowHeight, arrowWidth, arrowHeight);
    painter.setFont(font(20 * u));

    for (const auto& arrow : {std::make_pair(upRect, HitArea::ChannelUp), std::make_pair(downRect, HitArea::ChannelDown)})
    {
        fillRounded(painter, arrow.first, 8 * u, QColor(241, 239, 232, 22));
        painter.setPen(m_secondaryColor);
        painter.drawText(arrow.first, Qt::AlignCenter, symbols(arrow.second == HitArea::ChannelUp ? "{up}" : "{down}"));
        addHit(arrow.first, arrow.second);
    }

    if (!s.m_actual)
    {
        painter.setFont(font(17 * u));
        painter.setPen(m_warningColor);
        painter.drawText(QRectF(upRect.right(), upRect.y(), downRect.x() - upRect.right(), arrowHeight), Qt::AlignCenter,
            s.m_frequency > 0 ? QString("Retunes") : QString("Searches"));
    }

    const double divider1 = bar.x() + channelWidth + 4 * u;
    painter.fillRect(QRectF(divider1, bar.y() + pad, std::max(1.0, u), height - 2 * pad), m_dividerColor);

    // Programmes that haven't ended
    std::vector<DVBDemodEPG::Event> evs = events(m_row);
    evs.erase(std::remove_if(evs.begin(), evs.end(), [t](const DVBDemodEPG::Event& e) { return e.end() <= t; }), evs.end());
    m_ribbonEvent = std::max(0, std::min(m_ribbonEvent, (int) evs.size() - 1));

    // The logo at the right, the programmes before it, unless the bar is too narrow to spare the room
    const bool logo = bar.right() - pad - (divider1 + 28 * u) >= 800 * u;
    const double logoWidth = logo ? paintLogo(painter, bar.right() - pad, bar.center().y(), 255 * u, 90 * u) : 0.0;
    const double x0 = divider1 + 28 * u;
    const double contentWidth = bar.right() - pad - (logoWidth > 0.0 ? logoWidth + 32 * u : 0.0) - x0;
    const double mainWidth = contentWidth * 0.58;
    const double x1 = x0 + mainWidth + 28 * u;
    const double sideWidth = x0 + contentWidth - x1;
    const int top = (int) (bar.y() + pad);

    if (evs.empty())
    {
        drawText(painter, (int) x0, top + (int) (30 * u), (int) mainWidth, "No programme information", font(30 * u), m_secondaryColor);
    }
    else
    {
        const DVBDemodEPG::Event& e = evs[m_ribbonEvent];
        const bool onNow = e.m_start <= t;
        const bool next = (m_ribbonEvent == 0) || ((m_ribbonEvent == 1) && (evs[0].m_start <= t));
        const QString when = onNow ? "Now" : next ? "Next" : "Later";
        QString label = symbols("%1  {dot}  %2 {dash} %3").arg(when).arg(localTime(e.m_start)).arg(localTime(e.end()));
        const QString genre = DVBDemodEPG::genreText(e.m_genre, s.m_networkId);

        if (!genre.isEmpty()) {
            label += symbols("  {dot}  %1").arg(genre);
        }

        if (!onNow && (QDateTime::fromSecsSinceEpoch(e.m_start).toLocalTime().date() != QDateTime::fromSecsSinceEpoch(t).toLocalTime().date())) {
            label += symbols("  {dot}  %1").arg(localTime(e.m_start, "ddd"));
        }

        drawText(painter, (int) x0, top, (int) mainWidth, label, font(21 * u), onNow ? m_accentColor : m_secondaryColor);
        drawText(painter, (int) x0, top + (int) (30 * u), (int) mainWidth, e.m_title, font(38 * u, true), m_textColor);
        int y = top + (int) (94 * u);

        if (onNow)
        {
            const double fraction = std::max(0.0, std::min(1.0, (double) (t - e.m_start) / std::max(1, e.m_duration)));
            fillRounded(painter, QRectF(x0, y, mainWidth, 7 * u), 3.5 * u, QColor(241, 239, 232, 40));
            fillRounded(painter, QRectF(x0, y, mainWidth * fraction, 7 * u), 3.5 * u, m_accentColor);
        }

        y += (int) (24 * u);
        QString extra = onNow ? QString("%1 left").arg(durationText((int) (e.end() - t))) : durationText(e.m_duration);
        const QString simulcast = onNow ? simulcastName(m_row) : QString();

        if (!simulcast.isEmpty()) {
            extra += symbols("  {dot}  H: watch on %1").arg(simulcast);
        }

        paintBadges(painter, (int) x0, y, u, e, extra);

        // Then the next, and the one after
        painter.fillRect(QRectF(x1 - 14 * u, bar.y() + pad, std::max(1.0, u), height - 2 * pad), m_dividerColor);

        if (m_ribbonEvent + 1 < (int) evs.size())
        {
            const DVBDemodEPG::Event& n = evs[m_ribbonEvent + 1];
            drawText(painter, (int) x1, top, (int) sideWidth, symbols("%1  {dot}  %2").arg(onNow ? "Next" : "Then").arg(localTime(n.m_start)),
                font(21 * u), m_secondaryColor);
            drawText(painter, (int) x1, top + (int) (30 * u), (int) sideWidth, n.m_title, font(30 * u, true), m_textColor);
        }

        if (m_message.isEmpty() && (m_ribbonEvent + 2 < (int) evs.size()))
        {
            const DVBDemodEPG::Event& n = evs[m_ribbonEvent + 2];
            drawText(painter, (int) x1, top + (int) (78 * u), (int) sideWidth, QString("Then %1  %2").arg(localTime(n.m_start)).arg(n.m_title),
                font(23 * u), m_mutedColor);
        }
    }

    if (!m_message.isEmpty()) {
        drawText(painter, (int) x1, top + (int) (78 * u), (int) sideWidth, m_message, font(23 * u), m_warningColor);
    }
}

// The programme selected in detail over the dimmed picture, and channels by time below
void DVBDemodGuideOverlay::paintGuide(QPainter& painter, const QSize& size, qint64 t)
{
    const double u = scale(size);
    painter.fillRect(QRect(QPoint(0, 0), size), QColor(8, 8, 8, 150));

    // The selection
    const DVBDemodEPG::Service *service = m_services.empty() ? nullptr : &m_services[std::min(m_row, (int) m_services.size() - 1)];
    const std::vector<DVBDemodEPG::Event> selectedRow = events(m_row);
    const DVBDemodEPG::Event *selected = nullptr;

    for (const auto& e : selectedRow)
    {
        if (e.m_eventId == m_guideEventId) {
            selected = &e;
        }
    }

    // The grid's panel, larger in a small window, as the scale is limited. In the smallest, the selection is just a
    // line above the buttons, without the clock
    const bool compact = size.height() / u < 900;
    const bool tiny = size.height() / u < 600;
    const double panelMargin = 32 * u;
    const double panelHeight = tiny ? size.height() - 150 * u : size.height() * (compact ? 0.56 : 0.47);
    const QRectF panel(panelMargin, size.height() - panelMargin - panelHeight, size.width() - 2 * panelMargin, panelHeight);
    const double buttonsY = panel.y() - 50 * u;
    const double badgesHeight = 34 * u;

    const double heroX = (tiny ? 36 : 64) * u;
    const double heroWidth = size.width() * 0.6;
    // The guide, for clicks, is the grid and the buttons above it: a click elsewhere, as on the dimmed picture with the
    // selection's details, closes it
    m_drawn.push_back(QRectF(panel.x(), buttonsY, panel.width(), panel.y() - buttonsY));
    m_drawn.push_back(panel);
    int y = (int) (48 * u);

    if (!tiny)
    {
        // The clock at the top right, and the logo under it, smaller if the buttons are close, beside the selection's
        // details, which take the left 60 %
        paintClock(painter, size, t, u, true);
        const double logoTop = 112 * u;
        const double logoHeight = std::min(84 * u, buttonsY - 16 * u - logoTop);

        if (logoHeight >= 32 * u) {
            paintLogo(painter, size.width() - 48 * u, logoTop + logoHeight / 2.0, 260 * u, logoHeight);
        }
    }

    if (tiny)
    {
        QString line = service ? service->m_name : QString("No programme guide yet");

        if (selected) {
            line = symbols("%1  {dot}  %2 {dash} %3").arg(selected->m_title).arg(localTime(selected->m_start)).arg(localTime(selected->end()));
        }

        drawText(painter, (int) heroX, (int) (22 * u), (int) (size.width() - 2 * heroX), line, font(30 * u, true), m_textColor);
    }
    else if (!service)
    {
        drawText(painter, (int) heroX, y + (int) (28 * u), (int) heroWidth, "No programme guide yet", font(54 * u, true), m_textColor);
    }
    else if (!selected)
    {
        drawText(painter, (int) heroX, y, (int) heroWidth, service->m_name, font(23 * u), m_secondaryColor);
        drawText(painter, (int) heroX, y + (int) (32 * u), (int) heroWidth, "No programme information", font(54 * u, true), m_textColor);
    }
    else
    {
        const DVBDemodEPG::Event& e = *selected;
        QStringList parts;
        const QString genre = DVBDemodEPG::genreText(e.m_genre, service->m_networkId);

        if (!genre.isEmpty()) {
            parts.append(genre);
        }

        parts.append(service->m_name);
        const bool today = QDateTime::fromSecsSinceEpoch(e.m_start).toLocalTime().date() == QDateTime::fromSecsSinceEpoch(t).toLocalTime().date();
        parts.append(symbols("%1%2 {dash} %3").arg(today ? QString() : localTime(e.m_start, "ddd ")).arg(localTime(e.m_start)).arg(localTime(e.end())));
        drawText(painter, (int) heroX, y, (int) heroWidth, parts.join(symbols("  {dot}  ")), font(23 * u),
            e.m_genre >= 0 ? genreColor(e.m_genre, service->m_networkId).lighter(165) : m_secondaryColor);
        y += (int) (32 * u);
        drawText(painter, (int) heroX, y, (int) heroWidth, e.m_title, font((compact ? 46 : 56) * u, true), m_textColor);
        y += (int) ((compact ? 64 : 80) * u);
        const QFont f = font(25 * u);
        const QFontMetrics metrics(f);
        // Up to 4 lines of the description, as many as fit above the buttons with the badges
        const int lines = std::max(0, std::min(4, (int) ((buttonsY - 12 * u - badgesHeight - y) / metrics.lineSpacing())));

        if (!e.m_text.isEmpty() && (lines > 0))
        {
            painter.setFont(f);
            painter.setPen(m_bodyColor);
            painter.drawText(QRectF(heroX, y, heroWidth, metrics.lineSpacing() * lines), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, e.m_text);
            const QRect used = metrics.boundingRect(QRect(0, 0, (int) heroWidth, 100000), Qt::TextWordWrap, e.m_text);
            y += std::min(used.height(), metrics.lineSpacing() * lines) + (int) (14 * u);
        }

        QString extra = e.m_guidance;
        const bool scheduled = (m_isScheduled && m_isScheduled(service->m_networkId, service->m_serviceId, e.m_eventId));

        if (scheduled) {
            extra = symbols("{rec} Recording") + (extra.isEmpty() ? QString() : QString("   ") + extra);
        }

        if (y + badgesHeight <= buttonsY - 4 * u) {
            paintBadges(painter, (int) heroX, y, u, e, extra);
        }
    }

    // The grid
    double buttonsRight = panel.right() - 4 * u;
    buttonsRight -= paintButton(painter, buttonsRight, buttonsY, u, "G", "Close", HitArea::Close) + 14 * u;
    buttonsRight -= paintButton(painter, buttonsRight, buttonsY, u, "R", "Record", HitArea::Record) + 14 * u;
    buttonsRight -= paintButton(painter, buttonsRight, buttonsY, u, "Enter", "Watch", HitArea::Watch) + 14 * u;

    // The channel's programme on now in HD, as on the ribbon, whichever of its programmes is selected
    const QString simulcast = simulcastName(m_row);

    if (!simulcast.isEmpty()) {
        buttonsRight -= paintButton(painter, buttonsRight, buttonsY, u, "H", QString("Now on %1").arg(simulcast), HitArea::Simulcast) + 14 * u;
    }

    // A message, as why H did nothing, instead of the keys' hint
    buttonsRight -= 10 * u;
    painter.setFont(font(20 * u));
    painter.setPen(m_message.isEmpty() ? m_mutedColor : m_warningColor);
    painter.drawText(QRectF(panel.x(), buttonsY, buttonsRight - panel.x(), 38 * u), Qt::AlignRight | Qt::AlignVCenter,
        m_message.isEmpty() ? symbols("{left} {right} {up} {down}  Move") : m_message);
    fillRounded(painter, panel, 20 * u, QColor(14, 14, 14, 215));

    const double pad = 18 * u;
    const double labelWidth = 310 * u;
    const double headerHeight = 40 * u;
    const double gridX = panel.x() + pad + labelWidth;
    const double gridWidth = panel.right() - pad - gridX;
    const double rowsTop = panel.y() + pad + headerHeight;
    const double rowHeight = 64 * u;
    m_visibleRows = std::max(1, (int) ((panel.bottom() - pad - rowsTop) / rowHeight));
    keepGuideSelectionVisible();
    const double perSecond = gridWidth / m_windowSeconds;
    const qint64 windowEnd = m_windowStart + m_windowSeconds;

    painter.setFont(font(20 * u));
    painter.setPen(m_mutedColor);

    for (int k = 0; k < m_windowSeconds / 1800; k++)
    {
        const double x = gridX + k * 1800 * perSecond;
        painter.drawText(QRectF(x + 6 * u, panel.y() + pad, 1800 * perSecond, headerHeight - 8 * u), Qt::AlignLeft | Qt::AlignVCenter,
            localTime(m_windowStart + k * 1800));
        painter.fillRect(QRectF(x, panel.y() + pad + 6 * u, std::max(1.0, u), headerHeight - 14 * u), m_dividerColor);
    }

    const QFont numberFont = font(23 * u);
    const QFont nameFont = font(24 * u);
    const QFont titleFont = font(22 * u);
    const QFontMetrics titleMetrics(titleFont);

    for (int i = 0; i < m_visibleRows; i++)
    {
        const int row = m_firstRow + i;

        if (row >= (int) m_services.size()) {
            break;
        }

        const DVBDemodEPG::Service& s = m_services[row];
        const double ry = rowsTop + i * rowHeight;
        const double blockHeight = rowHeight - 8 * u;
        const bool current = row == m_row;

        // Channel
        if (current) {
            painter.fillRect(QRectF(panel.x() + pad - 8 * u, ry, 5 * u, blockHeight), m_accentColor);
        }

        addHit(QRectF(panel.x() + pad, ry, labelWidth - 8 * u, blockHeight), HitArea::Channel, row);

        painter.setFont(numberFont);
        painter.setPen(current ? m_textColor : m_mutedColor);
        painter.drawText(QRectF(panel.x() + pad + 4 * u, ry, 62 * u, blockHeight), Qt::AlignLeft | Qt::AlignVCenter,
            s.m_channel >= 0 ? QString::number(s.m_channel) : QString());
        QFont nf = nameFont;
        nf.setItalic(!s.m_actual); // Retunes, as in the guide window
        const QFontMetrics nameMetrics(nf);
        painter.setFont(nf);
        painter.setPen(current ? m_textColor : m_secondaryColor);
        painter.drawText(QRectF(panel.x() + pad + 70 * u, ry, labelWidth - 84 * u, blockHeight), Qt::AlignLeft | Qt::AlignVCenter,
            nameMetrics.elidedText(s.m_name, Qt::ElideRight, (int) (labelWidth - 84 * u)));

        // Programmes
        const std::vector<DVBDemodEPG::Event> evs = row == m_row ? selectedRow : events(row);
        bool any = false;

        for (const auto& e : evs)
        {
            if ((e.end() <= m_windowStart) || (e.m_start >= windowEnd)) {
                continue;
            }

            any = true;
            const double x0 = gridX + (std::max(e.m_start, m_windowStart) - m_windowStart) * perSecond;
            const double x1 = gridX + (std::min(e.end(), windowEnd) - m_windowStart) * perSecond;
            const QRectF block(x0 + 2 * u, ry, std::max(2.0, x1 - x0 - 4 * u), blockHeight);
            const bool isSelected = current && (e.m_eventId == m_guideEventId);
            addHit(block, HitArea::Event, row, e.m_eventId);
            const bool past = e.end() <= t;
            QColor c = genreColor(e.m_genre, s.m_networkId);

            if (isSelected)
            {
                c.setAlpha(225);
                fillRounded(painter, block, 6 * u, c);
                painter.setPen(QPen(m_textColor, std::max(1.0, 2 * u)));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(block, 6 * u, 6 * u);
            }
            else
            {
                QColor fill = c;
                fill.setAlpha(past ? 40 : 85);
                painter.fillRect(block, fill);
                c.setAlpha(past ? 110 : 255);
                painter.fillRect(QRectF(block.x(), block.y(), 5 * u, block.height()), c);
            }

            const double textX = block.x() + (isSelected ? 12 : 14) * u;
            const double textWidth = block.right() - 10 * u - textX;

            if (textWidth > 20 * u)
            {
                painter.setFont(titleFont);
                painter.setPen(past ? m_mutedColor : m_textColor);
                painter.drawText(QRectF(textX, block.y(), textWidth, block.height()), Qt::AlignLeft | Qt::AlignVCenter,
                    titleMetrics.elidedText(e.m_title, Qt::ElideRight, (int) textWidth));
            }

            if ((m_isScheduled && m_isScheduled(s.m_networkId, s.m_serviceId, e.m_eventId)))
            {
                painter.setPen(Qt::NoPen);
                painter.setBrush(m_recordColor);
                painter.drawEllipse(QPointF(block.right() - 12 * u, block.y() + 12 * u), 6 * u, 6 * u);
            }
        }

        if (!any)
        {
            painter.fillRect(QRectF(gridX + 2 * u, ry, gridWidth - 4 * u, blockHeight), QColor(241, 239, 232, 18));
            painter.setFont(titleFont);
            painter.setPen(m_mutedColor);
            painter.drawText(QRectF(gridX + 14 * u, ry, gridWidth, blockHeight), Qt::AlignLeft | Qt::AlignVCenter, "No information");
        }
    }

    // More channels above or below, and earlier or later, as buttons
    painter.setFont(font(22 * u));
    const double arrow = 40 * u;
    auto arrowButton = [&](const QRectF& r, const QString& text, HitArea::Kind kind, bool enabled) {
        painter.setPen(enabled ? m_secondaryColor : QColor(136, 135, 128, 70));
        painter.drawText(r, Qt::AlignCenter, text);

        if (enabled) {
            addHit(r, kind);
        }
    };
    const double headerY = panel.y() + pad;
    const qint64 earliest = t - t % 1800;
    arrowButton(QRectF(panel.x() + pad, headerY, arrow, headerHeight - 6 * u), symbols("{up}"), HitArea::PageUp, m_firstRow > 0);
    arrowButton(QRectF(panel.x() + pad + arrow, headerY, arrow, headerHeight - 6 * u), symbols("{down}"), HitArea::PageDown,
        m_firstRow + m_visibleRows < (int) m_services.size());
    arrowButton(QRectF(gridX - 2 * arrow - 8 * u, headerY, arrow, headerHeight - 6 * u), symbols("{left}"), HitArea::Earlier, m_windowStart > earliest);
    arrowButton(QRectF(gridX - arrow - 8 * u, headerY, arrow, headerHeight - 6 * u), symbols("{right}"), HitArea::Later, true);

    // Now
    if ((t >= m_windowStart) && (t < windowEnd))
    {
        const double x = gridX + (t - m_windowStart) * perSecond;
        const double bottom = rowsTop + std::min(m_visibleRows, (int) m_services.size() - m_firstRow) * rowHeight - 8 * u;
        painter.fillRect(QRectF(x - 1.5 * u, panel.y() + pad + headerHeight - 10 * u, 3 * u, bottom - (panel.y() + pad + headerHeight - 10 * u)), m_accentColor);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_accentColor);
        painter.drawEllipse(QPointF(x, panel.y() + pad + headerHeight - 10 * u), 6 * u, 6 * u);
    }
}
