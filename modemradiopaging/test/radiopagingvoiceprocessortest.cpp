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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "radiopagingvoiceprocessor.h"

namespace {

int passes = 0;
int failures = 0;
const int rate = 38400;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

// Peak and RMS of the samples out from 'from' to 'to' seconds
struct Level
{
    double m_peak = 0.0;
    double m_rms = 0.0;
};

Level level(const std::vector<float>& out, double from, double to)
{
    Level l;
    size_t a = (size_t) (from * rate);
    size_t b = std::min(out.size(), (size_t) (to * rate));
    double sum = 0.0;

    for (size_t i = a; i < b; i++)
    {
        l.m_peak = std::max(l.m_peak, (double) std::fabs(out[i]));
        sum += out[i] * out[i];
    }

    l.m_rms = (b > a) ? std::sqrt(sum / (double) (b - a)) : 0.0;
    return l;
}

// A tone of the amplitude and the frequency for the seconds, processed
std::vector<float> tones(RadioPagingVoiceProcessor& p, const std::vector<std::pair<double, double>>& parts, double frequency, double seconds)
{
    std::vector<float> out;
    size_t n = 0;

    for (const auto& part : parts)
    {
        for (int i = 0; i < (int) (seconds * rate); i++, n++)
        {
            float x = (float) (part.first * std::sin(2.0 * M_PI * frequency * n / rate) + part.second);
            out.push_back(p.process(x));
        }
    }

    return out;
}

} // namespace

int main()
{
    char what[200];
    RadioPagingVoiceProcessor p(rate);

    // A quiet voice is brought up to a level near the full scale
    std::vector<float> out = tones(p, {{0.03, 0.0}}, 1000.0, 4.0);
    Level quiet = level(out, 3.0, 4.0);
    std::snprintf(what, sizeof(what), "-30 dBFS at 1 kHz: up to a peak of %.2f", quiet.m_peak);
    check((quiet.m_peak > 0.8) && (quiet.m_peak <= 1.0), what);

    // A voice far over the full scale is never over it
    p.reset();
    out = tones(p, {{4.0, 0.0}}, 1000.0, 2.0);
    Level loud = level(out, 0.0, 2.0);
    Level loudSettled = level(out, 1.0, 2.0);
    std::snprintf(what, sizeof(what), "+12 dBFS at 1 kHz: peak %.3f, settled %.2f", loud.m_peak, loudSettled.m_peak);
    check((loud.m_peak <= 1.0) && (loudSettled.m_peak > 0.8), what);

    // Out of the voice band: 5 kHz and DC are taken out
    p.reset();
    out = tones(p, {{0.5, 0.0}}, 5000.0, 2.0);
    Level high = level(out, 1.0, 2.0);
    std::snprintf(what, sizeof(what), "5 kHz: peak %.3f", high.m_peak);
    check(high.m_peak < 0.05, what);
    p.reset();
    out = tones(p, {{0.0, 0.5}}, 1000.0, 2.0);
    Level dc = level(out, 1.0, 2.0);
    std::snprintf(what, sizeof(what), "DC: peak %.4f", dc.m_peak);
    check(dc.m_peak < 0.01, what);

    // Syllables 14 dB apart keep some of their difference
    p.reset();
    out = tones(p, {{0.5, 0.0}, {0.1, 0.0}, {0.5, 0.0}, {0.1, 0.0}}, 1000.0, 0.3);
    Level a = level(out, 0.65, 0.9);
    Level b = level(out, 0.95, 1.2);
    double ratio = a.m_rms / std::max(1e-9, b.m_rms);
    std::snprintf(what, sizeof(what), "Levels 14 dB apart (ratio 5): %.2f out", ratio);
    check((ratio > 1.3) && (ratio < 5.0), what);

    // A pause after the voice: its noise is not raised
    p.reset();
    out = tones(p, {{0.5, 0.0}, {0.0003, 0.0}}, 1000.0, 2.0);
    Level pause = level(out, 2.5, 4.0);
    std::snprintf(what, sizeof(what), "-70 dBFS pause after the voice: peak %.4f", pause.m_peak);
    check(pause.m_peak < 0.02, what);

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
