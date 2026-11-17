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

#include <QDateTime>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include "audio/audiodevicemanager.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/dsptypes.h"
#include "dsp/wavfilerecord.h"
#include "radiopagingwav.h"

#include "pagingvoiceplayer.h"

namespace {

const int feedMs = 20;          // The FIFO is filled every 20 ms
const double aheadSeconds = 0.2; // with 200 ms of samples
const double fifoSeconds = 0.5;
const double maxSeconds = 600.0;
const float fullScale = 32767.0f;

QString sizeText(qint64 bytes)
{
    return (bytes >= 1024 * 1024) ? QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1)
        : (bytes >= 1024) ? QString("%1 kB").arg(bytes / 1024.0, 0, 'f', 1) : QString("%1 B").arg(bytes);
}

} // namespace

PagingVoicePlayer::PagingVoicePlayer(QObject *parent) :
    QObject(parent),
    m_deviceName(AudioDeviceManager::m_defaultDeviceName)
{
    m_timer.setInterval(feedMs);
    connect(&m_timer, &QTimer::timeout, this, &PagingVoicePlayer::feed);
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &PagingVoicePlayer::handleInputMessages);
}

PagingVoicePlayer::~PagingVoicePlayer()
{
    m_timer.stop();
    detach();
}

void PagingVoicePlayer::setAudioDevice(const QString& deviceName)
{
    if (deviceName == m_deviceName) {
        return;
    }

    m_deviceName = deviceName;

    if (m_attached)
    {
        detach();
        attach();
    }
}

// The FIFO on the audio output: the samples are resampled to its rate
void PagingVoicePlayer::attach()
{
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    int index = audioDeviceManager->getOutputDeviceIndex(m_deviceName);
    m_sampleRate = audioDeviceManager->getOutputSampleRate(index);

    if (m_sampleRate <= 0) {
        m_sampleRate = 48000;
    }

    m_audioFifo.setSize((uint32_t) (fifoSeconds * m_sampleRate));
    audioDeviceManager->addAudioSink(&m_audioFifo, &m_inputMessageQueue, index);
    m_attached = true;
}

void PagingVoicePlayer::detach()
{
    if (m_attached)
    {
        DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);
        m_audioFifo.clear();
        m_attached = false;
    }
}

bool PagingVoicePlayer::play(const QString& file, QString& error)
{
    stop();

    RadioPagingWav::Audio audio;
    std::string e;

    if (!QFileInfo::exists(file))
    {
        error = QString("%1\nis not there any more").arg(file);
        return false;
    }
    if (!RadioPagingWav::load(file.toStdString(), audio, e, maxSeconds, true))
    {
        error = QString("%1\n%2").arg(file, QString::fromStdString(e));
        return false;
    }

    m_samples.swap(audio.m_samples);
    m_position = 0.0;
    m_file = file;
    m_playing = true;
    attach();
    feed();
    m_timer.start();
    emit stateChanged();
    return true;
}

void PagingVoicePlayer::stop()
{
    if (!m_playing) {
        return;
    }

    m_timer.stop();
    detach();
    m_samples.clear();
    m_file.clear();
    m_playing = false;
    emit stateChanged();
}

// The samples of the next 200 ms, linear interpolation to the rate of the output; at the end of the file the
// player stops once the FIFO has been played
void PagingVoicePlayer::feed()
{
    if (!m_playing) {
        return;
    }

    const double step = (double) RadioPagingWav::m_sampleRate / (double) m_sampleRate;
    uint32_t target = (uint32_t) (aheadSeconds * m_sampleRate);
    uint32_t fill = m_audioFifo.fill();
    std::vector<AudioSample> out;

    while ((fill + out.size() < target) && (m_position < (double) m_samples.size() - 1.0))
    {
        size_t i = (size_t) m_position;
        double frac = m_position - (double) i;
        float s = (float) ((1.0 - frac) * m_samples[i] + frac * m_samples[i + 1]);
        qint16 v = (qint16) std::max(-fullScale, std::min(fullScale, s * fullScale));
        out.push_back(AudioSample{v, v});
        m_position += step;
    }

    if (!out.empty()) {
        m_audioFifo.write((const quint8*) out.data(), (uint32_t) out.size());
    } else if ((m_position >= (double) m_samples.size() - 1.0) && m_audioFifo.isEmpty()) {
        stop();
    }
}

void PagingVoicePlayer::handleInputMessages()
{
    Message *message;

    while ((message = m_inputMessageQueue.pop()))
    {
        // The rate of the audio output changed
        if (DSPConfigureAudio::match(*message))
        {
            const DSPConfigureAudio& cfg = (const DSPConfigureAudio&) *message;

            if (cfg.getSampleRate() > 0)
            {
                m_sampleRate = cfg.getSampleRate();
                m_audioFifo.setSize((uint32_t) (fifoSeconds * m_sampleRate));
            }
        }

        delete message;
    }
}

QString PagingVoicePlayer::fileInfo(const QString& file)
{
    QFileInfo info(file);

    if (!info.exists()) {
        return QString("%1\n(not there any more)").arg(file);
    }

    std::ifstream f(file.toStdString(), std::ios::binary);
    WavFileRecord::Header header;

    // Any PCM layout: the check of WavFileRecord is for its own I/Q recordings
    if (!f || !WavFileRecord::readHeader(f, header, false) || (header.m_sampleRate == 0) || (header.m_blockAlign == 0)) {
        return QString("%1\n%2, not a WAV file that can be read").arg(file, sizeText(info.size()));
    }

    // RF64: the data size is in the ds64 chunk. A file being written may not have its size yet
    quint64 dataBytes = !strncmp(header.m_riffHeader.m_id, "RF64", 4) ? header.m_ds64.m_dataSize : header.m_dataHeader.m_size;

    if ((dataBytes == 0) || (dataBytes > (quint64) info.size())) {
        dataBytes = (quint64) info.size();
    }

    double seconds = (double) dataBytes / header.m_blockAlign / header.m_sampleRate;
    return QString("%1\n%2 s, %3 Hz, %4 %5, %6 bit, %7, %8")
        .arg(file)
        .arg(seconds, 0, 'f', 1)
        .arg(header.m_sampleRate)
        .arg(header.m_numChannels)
        .arg((header.m_numChannels == 1) ? "channel" : "channels")
        .arg(header.m_bitsPerSample)
        .arg(sizeText(info.size()))
        .arg(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
}
