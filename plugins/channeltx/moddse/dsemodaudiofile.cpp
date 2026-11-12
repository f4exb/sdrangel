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
#include <vector>

#include <QDebug>

#include "dsp/wavfilerecord.h"

#include "dsemodaudiofile.h"

DSEModAudioFile::DSEModAudioFile() :
    m_wav(false),
    m_sampleRate(RawSampleRate),
    m_channels(1),
    m_frames(0),
    m_position(0),
    m_dataStart(0),
    m_outputSampleRate(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_interpolatorSample(0.0f, 0.0f)
{
}

DSEModAudioFile::~DSEModAudioFile()
{
    close();
}

void DSEModAudioFile::close()
{
    if (m_stream.is_open()) {
        m_stream.close();
    }

    m_frames = 0;
    m_position = 0;
}

bool DSEModAudioFile::open(const QString& fileName)
{
    close();
    m_fileName = fileName;
    m_error.clear();
    m_wav = fileName.endsWith(".wav", Qt::CaseInsensitive);
    m_sampleRate = RawSampleRate;
    m_channels = 1;
    m_dataStart = 0;

    m_stream.open(fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);

    if (!m_stream.is_open())
    {
        m_error = QString("cannot open %1").arg(fileName);
        qDebug() << "DSEModAudioFile::open:" << m_error;
        return false;
    }

    std::streamoff fileSize = m_stream.tellg();
    m_stream.seekg(0, std::ios::beg);
    std::streamoff dataBytes = fileSize;

    if (m_wav)
    {
        WavFileRecord::Header header;

        if (!WavFileRecord::readHeader(m_stream, header, false))
        {
            m_error = "not a RIFF WAVE file";
        }
        // (WAVE_FORMAT_EXTENSIBLE too, as many editors write even mono 16 bit: its sub-format is not
        // read, but at 16 bits it is PCM)
        else if (((header.m_audioFormat != 1) && (header.m_audioFormat != 0xFFFE)) || (header.m_bitsPerSample != 16))
        {
            m_error = QString("format %1 at %2 bits: only 16 bit PCM is supported").arg(header.m_audioFormat).arg(header.m_bitsPerSample);
        }
        else if ((header.m_numChannels < 1) || (header.m_numChannels > 2))
        {
            m_error = QString("%1 channels: only mono and stereo are supported").arg(header.m_numChannels);
        }
        else if ((header.m_sampleRate < 1000) || (header.m_sampleRate > 384000))
        {
            m_error = QString("sample rate %1 Hz: 1 kHz to 384 kHz are supported").arg(header.m_sampleRate);
        }

        if (!m_error.isEmpty())
        {
            qDebug() << "DSEModAudioFile::open:" << fileName << ":" << m_error;
            m_stream.close();
            return false;
        }

        m_sampleRate = (int) header.m_sampleRate;
        m_channels = header.m_numChannels;
        m_dataStart = m_stream.tellg();
        // The data chunk's size, or what the file holds when it is shorter (a recording cut short) or
        // says 0 / 0xFFFFFFFF (a stream written without a known length)
        std::streamoff declared = header.m_dataHeader.m_size;
        std::streamoff available = fileSize - (std::streamoff) m_dataStart;
        dataBytes = ((declared > 0) && (declared < available) && (header.m_dataHeader.m_size != 0xFFFFFFFFu)) ? declared : available;
    }

    m_frames = (long) (dataBytes / (sizeof(int16_t) * m_channels));
    m_position = 0;
    m_stream.seekg(m_dataStart, std::ios::beg);

    if (m_frames <= 0)
    {
        m_error = "no audio samples";
        qDebug() << "DSEModAudioFile::open:" << fileName << ":" << m_error;
        m_stream.close();
        return false;
    }

    qDebug() << "DSEModAudioFile::open:" << fileName << (m_wav ? "wav" : "raw") << m_sampleRate << "Hz" << m_channels << "channel(s)"
        << m_frames << "frames" << getLengthSeconds() << "s";
    setupResampler();
    return true;
}

void DSEModAudioFile::setOutputSampleRate(int sampleRate)
{
    m_outputSampleRate = sampleRate;
    setupResampler();
}

void DSEModAudioFile::setupResampler()
{
    // The file's rate to the output rate: an arbitrary ratio, the filter at the higher of the two
    // rates with its cutoff under the lower Nyquist frequency
    if ((m_outputSampleRate > 0) && (m_outputSampleRate != m_sampleRate))
    {
        int high = std::max(m_sampleRate, m_outputSampleRate);
        int low = std::min(m_sampleRate, m_outputSampleRate);
        m_interpolator.create(16, high, 0.45 * low, 3.0);
        m_interpolatorDistance = (Real) m_sampleRate / (Real) m_outputSampleRate;
    }
    else
    {
        m_interpolatorDistance = 1.0f;
    }

    m_interpolatorDistanceRemain = 0.0f;
    m_interpolatorSample = Complex(0.0f, 0.0f);
}

bool DSEModAudioFile::nextSample(int16_t& sample)
{
    if (atEnd() || (readMono(&sample, 1) != 1))
    {
        sample = 0;
        return false;
    }

    return true;
}

int DSEModAudioFile::read(int16_t *out, int n, bool loop)
{
    std::fill(out, out + n, 0);

    if (!isOpen() || (n <= 0)) {
        return 0;
    }

    // Looping rewinds at the call after the end, so that a block is never cut short: the end of
    // the file is heard as a block of silence
    if (atEnd() && loop) {
        rewind();
    }

    if ((m_outputSampleRate == 0) || (m_outputSampleRate == m_sampleRate)) {
        return readMono(out, n);
    }

    int produced = 0;

    if (m_sampleRate > m_outputSampleRate)
    {
        // Decimation: the file samples go through the filter, an output every distance of them
        for (int i = 0; i < n; i++)
        {
            Complex result(0.0f, 0.0f);
            bool got = false;

            while (!got)
            {
                int16_t s;

                if (!nextSample(s)) {
                    return produced;
                }

                got = m_interpolator.decimate(&m_interpolatorDistanceRemain, Complex((float) s, 0.0f), &result);
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
            out[i] = (int16_t) std::max(-32768.0f, std::min(32767.0f, result.real()));
            produced++;
        }
    }
    else
    {
        // Interpolation: an output per step, a file sample consumed when the distance says so
        for (int i = 0; i < n; i++)
        {
            Complex result(0.0f, 0.0f);

            if (atEnd() && (m_interpolatorDistanceRemain >= 1.0f)) {
                return produced; // the next output needs a file sample there is not
            }

            if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_interpolatorSample, &result))
            {
                int16_t s;

                if (!nextSample(s)) {
                    return produced;
                }

                m_interpolatorSample = Complex((float) s, 0.0f);
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
            out[i] = (int16_t) std::max(-32768.0f, std::min(32767.0f, result.real()));
            produced++;
        }
    }

    return produced;
}

void DSEModAudioFile::seekFrame(long frame)
{
    if (!m_stream.is_open()) {
        return;
    }

    frame = std::max(0L, std::min(m_frames, frame));
    m_stream.clear();
    m_stream.seekg(m_dataStart + (std::streamoff) frame * (std::streamoff) (sizeof(int16_t) * m_channels), std::ios::beg);
    m_position = frame;
}

int DSEModAudioFile::readMono(int16_t *out, int n)
{
    if (!isOpen() || (n <= 0)) {
        return 0;
    }

    n = (int) std::min((long) n, m_frames - m_position);

    if (n <= 0) {
        return 0;
    }

    if (m_channels == 1)
    {
        m_stream.read(reinterpret_cast<char*>(out), sizeof(int16_t) * n);
    }
    else
    {
        std::vector<int16_t> frames(2 * n);
        m_stream.read(reinterpret_cast<char*>(frames.data()), sizeof(int16_t) * 2 * n);

        for (int i = 0; i < n; i++) {
            out[i] = (int16_t) (((int) frames[2 * i] + (int) frames[2 * i + 1]) / 2);
        }
    }

    int read = (int) (m_stream.gcount() / (std::streamsize) (sizeof(int16_t) * m_channels));

    if (read < n)
    {
        // The file is shorter than its header said: the rest is the end
        std::fill(out + read, out + n, 0);
        m_frames = m_position + read;
    }

    m_position += read;
    return read;
}
