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

#ifndef INCLUDE_DTVSTREAMREMUXER_H
#define INCLUDE_DTVSTREAMREMUXER_H

#include <atomic>

#include <QMap>
#include <QString>
#include <QThread>

#include "export.h"

class DTVTSFifo;
struct AVFormatContext;

// A stream from the internet (DASH, HLS, or a file), as an application plays, remuxed to a transport stream of one
// service, its best video and audio, for DTVVideoPlayer to play as it does a broadcast. Written to a FIFO, waiting
// while it's full, so it's read as fast as it's played. Timestamps start at 1 s, so the stream's time of what's
// shown (from its start) is the frame's timestamp plus getTimestampOffset()
class SDRGUI_API DTVStreamRemuxer : public QThread
{
    Q_OBJECT

public:
    //! options: FFmpeg's, as user_agent and headers. From start seconds into the stream
    DTVStreamRemuxer(DTVTSFifo *fifo, const QString& url, const QMap<QString, QString>& options, double start, QObject *parent = nullptr);
    ~DTVStreamRemuxer();

    void stop();                        //!< And waits for the thread
    double getDuration() const { return m_duration; }   //!< Seconds, NaN until known, infinite if live
    double getTimestampOffset() const { return m_offset; }  //!< NaN until the first packet's written
    bool isFinished() const { return m_finished; }      //!< The end of the stream written
    //! Whether it's played. Until it is (loaded before it's played, or paused), only what's due as it starts is
    //! written, so it's not given all at once when it's played; and its time stands still while it isn't
    void setPlaying(bool playing) { m_playing = playing; }
    //! Whether the URL's redirect's found first (before it's started): not if it's the URL a redirect gave
    void setResolveRedirects(bool resolve) { m_resolveRedirects = resolve; }

    static constexpr double m_timestampStart = 1.0;

signals:
    void opened(double duration, int width, int height);
    //! The URL the given one redirects to
    void resolved(const QString& url);
    void failed(const QString& error);

protected:
    void run() override;

private:
    static int writePacket(void *opaque, const uint8_t *buf, int size);
    static int interrupt(void *opaque);

    DTVTSFifo *m_fifo;
    const QString m_url;
    const QMap<QString, QString> m_options;
    const double m_start;
    std::atomic<bool> m_stop;
    std::atomic<double> m_duration;
    std::atomic<double> m_offset;
    std::atomic<bool> m_finished;
    std::atomic<bool> m_playing{false};
    bool m_resolveRedirects = true;
};

#endif // INCLUDE_DTVSTREAMREMUXER_H
