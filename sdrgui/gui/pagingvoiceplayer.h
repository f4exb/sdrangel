///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef SDRGUI_GUI_PAGINGVOICEPLAYER_H_
#define SDRGUI_GUI_PAGINGVOICEPLAYER_H_

#include <QObject>
#include <QString>
#include <QTimer>

#include <vector>

#include "audio/audiofifo.h"
#include "util/messagequeue.h"
#include "export.h"

// Plays the WAV file of a voice message on an audio output of SDRangel (AudioDeviceManager), one file at a
// time. The samples are resampled to the rate of the output and fed to its FIFO as they are played.
class SDRGUI_API PagingVoicePlayer : public QObject
{
    Q_OBJECT
public:
    explicit PagingVoicePlayer(QObject *parent = nullptr);
    ~PagingVoicePlayer();

    //!< The audio output, by its name in AudioDeviceManager; a file being played goes on there
    void setAudioDevice(const QString& deviceName);
    const QString& audioDevice() const { return m_deviceName; }
    //!< Plays a file, after the one played is stopped. False and an error if the file can not be read
    bool play(const QString& file, QString& error);
    void stop();
    bool isPlaying() const { return m_playing; }
    //!< The file played, empty when none
    const QString& file() const { return m_file; }
    //!< The path, the length and the format of a WAV file, for a tool tip; why not if it can not be read
    static QString fileInfo(const QString& file);

signals:
    //!< Started or stopped (at its end, or stopped)
    void stateChanged();

private:
    AudioFifo m_audioFifo;
    MessageQueue m_inputMessageQueue;   //!< Changes of the rate of the audio output
    QTimer m_timer;
    QString m_deviceName;
    QString m_file;
    std::vector<float> m_samples;       //!< The file at RadioPagingWav::m_sampleRate
    double m_position = 0.0;            //!< In m_samples
    int m_sampleRate = 48000;           //!< Of the audio output
    bool m_playing = false;
    bool m_attached = false;

    void attach();
    void detach();
    void feed();
    void handleInputMessages();
};

#endif // SDRGUI_GUI_PAGINGVOICEPLAYER_H_
