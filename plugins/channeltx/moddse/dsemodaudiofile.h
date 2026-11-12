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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODAUDIOFILE_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODAUDIOFILE_H_

#include <cstdint>
#include <fstream>

#include <QString>

#include "dsp/dsptypes.h"
#include "dsp/interpolator.h"

/**
 * The speech file input of the modulator: a .wav file (RIFF PCM, 16 bit signed, mono or
 * stereo, any sample rate; the header read by WavFileRecord) or a headerless .raw file
 * (16 bit signed mono at 8 kHz, as before). Frames are read as mono 16 bit samples at the
 * file's rate (a stereo file's channels are averaged), or resampled to the vocoder's rate
 * (setOutputSampleRate, read). The channel opens and seeks it and the DSP thread reads it,
 * under the caller's mutex.
 */
class DSEModAudioFile
{
public:
    static constexpr int RawSampleRate = 8000;

    DSEModAudioFile();
    ~DSEModAudioFile();

    //! Opens the file; on failure isOpen() is false and getError() says why
    bool open(const QString& fileName);
    void close();
    bool isOpen() const { return m_stream.is_open() && (m_frames > 0); }
    const QString& getError() const { return m_error; }
    const QString& getFileName() const { return m_fileName; }
    bool isWav() const { return m_wav; }
    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }
    //! Frames (samples per channel) in the file
    long getFrameCount() const { return m_frames; }
    //! Length in whole seconds
    int getLengthSeconds() const { return m_sampleRate > 0 ? (int) (m_frames / m_sampleRate) : 0; }
    //! Frames read so far from the start
    long getPosition() const { return m_position; }
    bool atEnd() const { return m_position >= m_frames; }
    void rewind() { seekFrame(0); }
    void seekFrame(long frame);
    void seekPercentage(int percentage) { seekFrame((long) ((m_frames * (long long) percentage) / 100)); }
    /**
     * Reads up to n frames as mono samples at the file's rate. Returns the number read (less than
     * n at the end of the file, 0 at the end or when not open).
     */
    int readMono(int16_t *out, int n);
    //! The rate read() delivers: the file is resampled to it (0: the file's own rate)
    void setOutputSampleRate(int sampleRate);
    int getOutputSampleRate() const { return m_outputSampleRate; }
    /**
     * n mono samples at the output rate. The end of the file is silence to the end of the call;
     * with loop the file starts again at the next call. Returns the samples that came from the
     * file (0 when the file is over or not open).
     */
    int read(int16_t *out, int n, bool loop);

private:
    //! The next file sample; false (and 0) at the end
    bool nextSample(int16_t& sample);
    void setupResampler();

    std::ifstream m_stream;
    QString m_fileName;
    QString m_error;
    bool m_wav;
    int m_sampleRate;
    int m_channels;
    long m_frames;
    long m_position;
    std::streampos m_dataStart;
    int m_outputSampleRate;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    Complex m_interpolatorSample;   //!< The file sample the interpolator is working on (upsampling)
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODAUDIOFILE_H_
