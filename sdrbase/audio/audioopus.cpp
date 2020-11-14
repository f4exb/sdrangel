///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// In this version we will use a fixed constant bit rate of 64kbit/s.            //
// With a frame time of 20ms the encoder output size is always 160 bytes.        //
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

#include "opus/opus.h"
#include <QDebug>
#include <QMutexLocker>

#include "audioopus.h"

AudioOpus::AudioOpus() :
    m_encoderState(0),
    m_encoderOK(false),
    m_mutex(QMutex::Recursive)
{
    qDebug("AudioOpus::AudioOpus: libopus version %s", opus_get_version_string());
}

AudioOpus::~AudioOpus()
{
    if (m_encoderState) {
        opus_encoder_destroy(m_encoderState);
    }
}

void AudioOpus::setEncoder(int32_t fs, int nChannels)
{
    int error;
    QMutexLocker mutexLocker(&m_mutex);

    if (m_encoderState) {
        opus_encoder_destroy(m_encoderState);
    }

    m_encoderState = opus_encoder_create(fs, nChannels, OPUS_APPLICATION_AUDIO, &error);

    if (error != OPUS_OK)
    {
        qWarning("AudioOpus::setEncoder: error: %s", opus_strerror(error));
        m_encoderOK = false;
        return;
    }
    else
    {
        qDebug("AudioOpus::setEncoder: fs: %d, nChannels: %d", fs, nChannels);
        m_encoderOK = true;
    }

    error = opus_encoder_ctl(m_encoderState, OPUS_SET_BITRATE(m_bitrate));

    if (error != OPUS_OK)
    {
        qWarning("AudioOpus::setEncoder: set bitrate error: %s", opus_strerror(error));
        m_encoderOK = false;
        return;
    }

    error = opus_encoder_ctl(m_encoderState, OPUS_SET_VBR(0)); // force constant bit rate

    if (error != OPUS_OK)
    {
        qWarning("AudioOpus::setEncoder: set constant bitrate error: %s", opus_strerror(error));
        m_encoderOK = false;
        return;
    }
}

int AudioOpus::encode(int frameSize, int16_t *in, uint8_t *out)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_encoderOK)
    {
        qWarning("AudioOpus::encode: encoder not initialized");
        return 0;
    }

    int nbBytes = opus_encode(m_encoderState, in, frameSize, out, m_maxPacketSize);

    if (nbBytes < 0)
    {
        qWarning("AudioOpus::encode failed: %s", opus_strerror(nbBytes));
        return 0;
    }
    else
    {
        return nbBytes;
    }
}
