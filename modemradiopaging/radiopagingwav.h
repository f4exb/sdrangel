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

#ifndef MODEMRADIOPAGING_RADIOPAGINGWAV_H
#define MODEMRADIOPAGING_RADIOPAGINGWAV_H

#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

// Sound file of a GSC voice page. The samples of a WAV file (PCM 8, 16, 24 or 32 bit, or 32 bit
// float) are used as they are: -1 to +1, full scale is the deviation of the voice. No gain,
// limiting, filtering or fading. The channels are averaged to one, and the samples are resampled
// to the rate of the modulator (38400 Hz) with the Interpolator of sdrbase, which the modulator
// uses too. The length is limited by the caller (the voice length of the transmitter): a longer
// file is refused, or cut at the limit.
class MODEMRADIOPAGING_API RadioPagingWav
{
public:
    static const int m_sampleRate = 38400;      //!< Rate of the samples, Hz: the internal rate of the modulator
    static const int m_maxRate = 76800;         //!< Highest rate of a file: the Interpolator decimates by less than 2
    static const int m_maxBytes = 64 * 1024 * 1024;     //!< Largest file read

    struct Audio
    {
        std::vector<float> m_samples;
        double m_rate = 0.0;                    //!< Hz, m_sampleRate
        double m_fileSeconds = 0.0;             //!< Length of the file
        bool m_trimmed = false;                 //!< The file was longer than the limit and was cut
    };

    //!< Samples of a WAV file in memory. False and an error if it is not a WAV file of a supported
    //!< format, or empty, or (when trim is not set) longer than maxSeconds
    static bool decode(const std::vector<uint8_t>& file, Audio& audio, std::string& error, double maxSeconds, bool trim);
    //!< Mono samples at the given rate (1 to 76.8 kHz), -1 to +1, resampled to m_sampleRate (the samples are taken)
    static bool fromSamples(std::vector<float>& mono, double rate, Audio& audio, std::string& error);
    //!< Samples of a WAV file; files over m_maxBytes are refused
    static bool load(const std::string& path, Audio& audio, std::string& error, double maxSeconds, bool trim);
};

// Samples resampled to the rate of the modulator as they come (live voice): the state of the
// filter is kept between the calls
class MODEMRADIOPAGING_API RadioPagingResampler
{
public:
    RadioPagingResampler();
    ~RadioPagingResampler();
    //!< Input rate 1 to 76.8 kHz; false if not supported. Starts again
    bool setup(double rate);
    //!< Appends the resampled samples to out
    void process(const float *in, size_t count, std::vector<float>& out);

private:
    RadioPagingResampler(const RadioPagingResampler&);
    RadioPagingResampler& operator=(const RadioPagingResampler&);

    struct Private;
    Private *m_private;
};

#endif // MODEMRADIOPAGING_RADIOPAGINGWAV_H
