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

#include <cmath>

#include <QDateTime>

#include "dtv/dtvtsfifo.h"
#include "dtv/dtvstreamremuxer.h"
#include "dtv/dtvstreamplayback.h"

namespace {

constexpr int m_fifoSize = 8 * 1024 * 1024;
constexpr int m_checkMs = 250;
constexpr qint64 m_endedMs = 1500;      //!< No pictures for this long, with the stream all read: it's ended

} // namespace

DTVStreamPlayback::DTVStreamPlayback(AudioFifo *audioFifo, DTVVideoPlayer::FrameCallback frameCallback,
    DTVVideoPlayer::CanShowCallback canShowCallback, std::function<void(DTVVideoPlayer *)> configure, QObject *parent) :
    QObject(parent),
    m_audioFifo(audioFifo),
    m_frameCallback(frameCallback),
    m_canShowCallback(canShowCallback),
    m_configure(configure),
    m_position(0.0),
    m_playing(false),
    m_fifo(new DTVTSFifo(m_fifoSize)),
    m_remuxer(nullptr),
    m_player(nullptr),
    m_frameTime(std::nan("")),
    m_frameAt(0),
    m_playingSignalled(false),
    m_duration(std::nan(""))
{
    connect(&m_timer, &QTimer::timeout, this, &DTVStreamPlayback::check);
}

DTVStreamPlayback::~DTVStreamPlayback()
{
    stop();
}

void DTVStreamPlayback::load(const QString& url, const QMap<QString, QString>& options)
{
    stop();
    m_url = url;
    m_options = options;
    m_position = 0.0;
    m_duration = std::nan("");
    m_urlResolved = false;
    startRemuxer(0.0);
    m_timer.start(m_checkMs);
}

void DTVStreamPlayback::play()
{
    if (!isLoaded() || m_playing) {
        return;
    }

    m_playing = true;

    if (!m_remuxer) {
        startRemuxer(m_position);
    } else {
        m_remuxer->setPlaying(true);
    }

    startPlayer();
}

// Paused where it is: the player stopped, and the stream made ready to go on from there
void DTVStreamPlayback::pause()
{
    if (!m_playing) {
        return;
    }

    m_position = getPosition();
    m_playing = false;
    stopPlayer();
    stopRemuxer();
    startRemuxer(m_position);
}

void DTVStreamPlayback::seek(double position)
{
    if (!isLoaded()) {
        return;
    }

    m_position = std::max(0.0, position);
    stopPlayer();
    stopRemuxer();
    startRemuxer(m_position);

    if (m_playing) {
        startPlayer();
    }
}

void DTVStreamPlayback::stop()
{
    m_timer.stop();
    stopPlayer();
    stopRemuxer();
    m_playing = false;
    m_url.clear();
}

double DTVStreamPlayback::getPosition() const
{
    const double frameTime = m_frameTime;
    const double offset = m_remuxer ? m_remuxer->getTimestampOffset() : std::nan("");

    if (m_player && !std::isnan(frameTime) && !std::isnan(offset)) {
        return frameTime + offset;
    }

    return m_position;
}

int DTVStreamPlayback::getBuffered()
{
    return m_fifo->fill();
}

double DTVStreamPlayback::getDuration() const
{
    return m_duration;
}

void DTVStreamPlayback::setAudioOutput(int sampleRate, double latency)
{
    if (m_player) {
        m_player->setAudioOutput(sampleRate, latency);
    }
}

void DTVStreamPlayback::startRemuxer(double position)
{
    m_fifo->reset();
    m_remuxer = new DTVStreamRemuxer(m_fifo.get(), m_url, m_options, position, this);
    DTVStreamRemuxer *remuxer = m_remuxer;
    m_remuxer->setPlaying(m_playing);
    // Its redirect found once: the URL it gives used after (as when it's paused or seeks)
    m_remuxer->setResolveRedirects(!m_urlResolved);
    connect(m_remuxer, &DTVStreamRemuxer::resolved, this, [this, remuxer](const QString& url) {
        if (remuxer == m_remuxer) {
            m_url = url;
        }
    }, Qt::QueuedConnection);

    connect(m_remuxer, &DTVStreamRemuxer::opened, this, [this, remuxer](double duration, int width, int height) {
        if (remuxer != m_remuxer) {
            return;
        }

        const bool first = std::isnan(m_duration);
        m_duration = duration;
        m_urlResolved = true;

        if (first) {
            emit metadata(duration, width, height);
        }
    }, Qt::QueuedConnection);
    connect(m_remuxer, &DTVStreamRemuxer::failed, this, [this, remuxer](const QString& message) {
        if (remuxer == m_remuxer) {
            emit error(message);
        }
    }, Qt::QueuedConnection);

    m_remuxer->start();
}

void DTVStreamPlayback::stopRemuxer()
{
    if (m_remuxer)
    {
        m_remuxer->stop();
        delete m_remuxer;
        m_remuxer = nullptr;
    }
}

void DTVStreamPlayback::startPlayer()
{
    if (m_player) {
        return;
    }

    m_frameTime = std::nan("");
    m_playingSignalled = false;
    m_player = new DTVVideoPlayer(m_fifo.get(), m_audioFifo);
    m_player->setService(0);    // The remuxed stream's one

    if (m_configure) {
        m_configure(m_player);
    }

    m_player->setFrameCallback([this](GLVideoFramePtr frame) {
        m_frameTime = frame->m_timestamp;
        m_frameAt = QDateTime::currentMSecsSinceEpoch();

        if (!m_playingSignalled.exchange(true))
        {
            qDebug("DTVStreamPlayback: first picture at %.3f", frame->m_timestamp);
            QMetaObject::invokeMethod(this, [this]() { emit playing(); }, Qt::QueuedConnection);
        }

        if (m_frameCallback) {
            m_frameCallback(frame);
        }
    });
    m_player->setCanShowCallback(m_canShowCallback);
    m_player->start();
}

void DTVStreamPlayback::stopPlayer()
{
    if (m_player)
    {
        m_player->stopPlayer();
        delete m_player;
        m_player = nullptr;
    }
}

// The end: the stream's all been read and played (no pictures for a while)
void DTVStreamPlayback::check()
{
    if (m_playing && m_remuxer && m_remuxer->isFinished() && (m_fifo->fill() == 0) && m_playingSignalled
        && (QDateTime::currentMSecsSinceEpoch() - m_frameAt > m_endedMs))
    {
        m_position = getPosition();
        m_playing = false;
        stopPlayer();
        emit ended();
    }
}
