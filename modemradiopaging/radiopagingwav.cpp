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

#include "radiopagingwav.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include <QtEndian>

#include "dsp/interpolator.h"

const int RadioPagingWav::m_sampleRate;
const int RadioPagingWav::m_maxRate;
const int RadioPagingWav::m_maxBytes;

namespace {

const int interpolatorPhases = 48;         // As the modulator's resampling
const double interpolatorTaps = 3.0;

uint32_t le32(const uint8_t *p)
{
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

uint16_t le16(const uint8_t *p)
{
    return (uint16_t) (p[0] | (p[1] << 8));
}

} // namespace

bool RadioPagingWav::decode(const std::vector<uint8_t>& file, Audio& audio, std::string& error, double maxSeconds, bool trim)
{
    audio = Audio();

    if ((file.size() < 12) || (std::memcmp(file.data(), "RIFF", 4) != 0) || (std::memcmp(file.data() + 8, "WAVE", 4) != 0))
    {
        error = "Not a WAV file";
        return false;
    }

    int format = 0;
    int channels = 0;
    uint32_t rate = 0;
    int bits = 0;
    const uint8_t *data = nullptr;
    size_t dataSize = 0;

    // Chunks: fmt then data (other chunks skipped)
    size_t pos = 12;

    while (pos + 8 <= file.size())
    {
        uint32_t size = le32(file.data() + pos + 4);
        const uint8_t *body = file.data() + pos + 8;
        size_t available = file.size() - (pos + 8);

        if (!std::memcmp(file.data() + pos, "fmt ", 4) && (size >= 16) && (available >= 16))
        {
            format = le16(body);
            channels = le16(body + 2);
            rate = le32(body + 4);
            bits = le16(body + 14);

            // WAVE_FORMAT_EXTENSIBLE: the format is the first two bytes of the sub format
            if ((format == 0xfffe) && (size >= 26) && (available >= 26)) {
                format = le16(body + 24);
            }
        }
        else if (!std::memcmp(file.data() + pos, "data", 4))
        {
            data = body;
            dataSize = std::min<size_t>(size, available);     // A size of a file being written can be too large
            break;
        }

        // The size of a chunk can be anything: past the end of the file there is no other chunk
        if ((uint64_t) size + (size & 1) > available) {
            break;
        }

        pos += 8 + (size_t) size + (size & 1);
    }

    bool pcm = (format == 1) && ((bits == 8) || (bits == 16) || (bits == 24) || (bits == 32));
    bool floating = (format == 3) && (bits == 32);

    if (!data || (!pcm && !floating) || (channels < 1) || (channels > 8) || (rate < 1000) || (rate > (uint32_t) m_maxRate))
    {
        error = "WAV format not supported: PCM 8, 16, 24 or 32 bit, or float 32 bit, up to 8 channels, 1 to 76.8 kHz";
        return false;
    }

    size_t frameBytes = (size_t) channels * (bits / 8);
    size_t frames = dataSize / frameBytes;

    if (frames == 0)
    {
        error = "The WAV file has no samples";
        return false;
    }

    audio.m_fileSeconds = (double) frames / rate;

    if (audio.m_fileSeconds > maxSeconds)
    {
        if (!trim)
        {
            error = "The WAV file is longer than the voice length";
            return false;
        }

        frames = std::max<size_t>(1, (size_t) std::floor(maxSeconds * rate));
        audio.m_trimmed = true;
    }

    // The samples of the file, the channels averaged
    std::vector<float> mono(frames);

    for (size_t i = 0; i < frames; i++)
    {
        double sum = 0.0;

        for (int c = 0; c < channels; c++)
        {
            const uint8_t *p = data + i * frameBytes + (size_t) c * (bits / 8);
            double v;

            if (floating)
            {
                float f = qFromLittleEndian<float>(p);   // IEEE 754 binary32, little-endian (WAVE_FORMAT_IEEE_FLOAT)
                v = std::isfinite(f) ? f : 0.0f;     // A NaN or an infinity would stay in the filters of the modulator
            }
            else if (bits == 8) {
                v = ((int) p[0] - 128) / 128.0;
            } else if (bits == 16) {
                v = (int16_t) le16(p) / 32768.0;
            } else if (bits == 24) {
                v = ((int32_t) (((uint32_t) p[0] << 8) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 24))) / 2147483648.0;
            } else {
                v = (int32_t) le32(p) / 2147483648.0;
            }

            sum += v;
        }

        mono[i] = (float) (sum / channels);
    }

    double fileSeconds = audio.m_fileSeconds;
    bool trimmed = audio.m_trimmed;
    bool ok = fromSamples(mono, (double) rate, audio, error);
    audio.m_fileSeconds = fileSeconds;
    audio.m_trimmed = trimmed;
    return ok;
}

bool RadioPagingWav::fromSamples(std::vector<float>& mono, double rate, Audio& audio, std::string& error)
{
    // The rate of the modulator: resampled with the Interpolator of sdrbase, as the modulator
    // resamples to the channel rate
    size_t frames = mono.size();
    audio.m_rate = m_sampleRate;

    if ((rate < 1000.0) || (rate > m_maxRate))
    {
        error = "Sample rate not supported: 1 to 76.8 kHz";
        return false;
    }

    if ((int) rate == m_sampleRate)
    {
        audio.m_samples.swap(mono);
        return !audio.m_samples.empty();
    }

    RadioPagingResampler resampler;

    if (!resampler.setup(rate))
    {
        error = "Sample rate not supported: 1 to 76.8 kHz";
        return false;
    }

    audio.m_samples.reserve((size_t) (frames / (rate / m_sampleRate)) + 2);
    resampler.process(mono.data(), frames, audio.m_samples);

    if (audio.m_samples.empty())
    {
        error = "No samples";
        return false;
    }

    return true;
}

struct RadioPagingResampler::Private
{
    Interpolator m_interpolator;
    Real m_remain = 0.0f;
    double m_step = 1.0;                //!< Input samples per output sample
    bool m_passthrough = false;         //!< The rate is that of the modulator
};

RadioPagingResampler::RadioPagingResampler() :
    m_private(new Private())
{
}

RadioPagingResampler::~RadioPagingResampler()
{
    delete m_private;
}

bool RadioPagingResampler::setup(double rate)
{
    if ((rate < 1000.0) || (rate > RadioPagingWav::m_maxRate)) {
        return false;
    }

    m_private->m_step = rate / RadioPagingWav::m_sampleRate;
    m_private->m_remain = 0.0f;
    m_private->m_passthrough = ((int) rate == RadioPagingWav::m_sampleRate);

    // The filter is made at the rate of the input, its cutoff below the lower of the two rates
    m_private->m_interpolator.create(interpolatorPhases, rate, 0.45 * std::min<double>(rate, RadioPagingWav::m_sampleRate),
        interpolatorTaps);
    return true;
}

void RadioPagingResampler::process(const float *in, size_t count, std::vector<float>& out)
{
    Complex c;

    if (m_private->m_passthrough)
    {
        out.insert(out.end(), in, in + count);
        return;
    }

    Interpolator& interpolator = m_private->m_interpolator;
    Real& remain = m_private->m_remain;
    Real step = (Real) m_private->m_step;

    if (m_private->m_step > 1.0)
    {
        for (size_t i = 0; i < count; i++)
        {
            if (interpolator.decimate(&remain, Complex(in[i], 0.0f), &c))
            {
                out.push_back(c.real());
                remain += step;
            }
        }
    }
    else
    {
        size_t i = 0;

        while (i < count)
        {
            bool consumed = interpolator.interpolate(&remain, Complex(in[i], 0.0f), &c);
            out.push_back(c.real());
            remain += step;

            if (consumed) {
                i++;
            }
        }
    }
}

bool RadioPagingWav::load(const std::string& path, Audio& audio, std::string& error, double maxSeconds, bool trim)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        error = "Cannot open the sound file";
        return false;
    }

    // A huge file is refused at once
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);

    if ((size < 12) || (size > (std::streamoff) m_maxBytes))
    {
        error = (size < 12) ? "Not a WAV file" : "The sound file is too large";
        return false;
    }

    std::vector<uint8_t> bytes((size_t) size);

    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
    {
        error = "Cannot read the sound file";
        return false;
    }

    return decode(bytes, audio, error, maxSeconds, trim);
}
