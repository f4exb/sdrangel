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

#ifndef INCLUDE_DTVSTREAMPLAYBACK_H
#define INCLUDE_DTVSTREAMPLAYBACK_H

#include <atomic>
#include <functional>
#include <memory>

#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>

#include "dtv/dtvvideoplayer.h"
#include "export.h"

class AudioFifo;
class DTVTSFifo;
class DTVStreamRemuxer;

// Plays a stream from the internet, as an interactive application's video element would (as iPlayer's): remuxed to a
// transport stream (DTVStreamRemuxer), played by a DTVVideoPlayer as a broadcast is. Loading opens the stream (and
// fills the buffer); playing starts the player; pausing and seeking stop it, and start the stream again from the
// position, as the player plays what it's given as it comes
class SDRGUI_API DTVStreamPlayback : public QObject
{
    Q_OBJECT

public:
    //! configure: called with each player made, to set it up as the broadcast's is (deinterlacing, volume...)
    DTVStreamPlayback(AudioFifo *audioFifo, DTVVideoPlayer::FrameCallback frameCallback, DTVVideoPlayer::CanShowCallback canShowCallback,
        std::function<void(DTVVideoPlayer *)> configure, QObject *parent = nullptr);
    ~DTVStreamPlayback();

    //! options: FFmpeg's, as user_agent and headers
    void load(const QString& url, const QMap<QString, QString>& options);
    void play();
    void pause();
    void seek(double position);
    void stop();

    bool isLoaded() const { return !m_url.isEmpty(); }
    bool isPlaying() const { return m_playing; }
    double getPosition() const;         //!< Seconds into the stream, of the picture shown
    double getDuration() const;         //!< NaN until known, infinite if live
    void setAudioOutput(int sampleRate, double latency);
    DTVVideoPlayer *getPlayer() { return m_player; }

signals:
    void metadata(double duration, int width, int height);
    void playing();                     //!< The first picture's shown, after play() or a seek
    void ended();
    void error(const QString& message);

private:
    void startRemuxer(double position);
    void stopRemuxer();
    void startPlayer();
    void stopPlayer();
    void check();

    AudioFifo *m_audioFifo;
    DTVVideoPlayer::FrameCallback m_frameCallback;
    DTVVideoPlayer::CanShowCallback m_canShowCallback;
    std::function<void(DTVVideoPlayer *)> m_configure;
    QString m_url;
    QMap<QString, QString> m_options;
    double m_position;                  //!< Where the stream's started from (when paused, where it's paused)
    bool m_playing;
    std::unique_ptr<DTVTSFifo> m_fifo;
    DTVStreamRemuxer *m_remuxer;
    DTVVideoPlayer *m_player;
    std::atomic<double> m_frameTime;    //!< Timestamp of the last picture shown, NaN if none yet
    std::atomic<qint64> m_frameAt;      //!< When, ms since epoch
    std::atomic<bool> m_playingSignalled;
    double m_duration;
    QTimer m_timer;
};

#endif // INCLUDE_DTVSTREAMPLAYBACK_H
