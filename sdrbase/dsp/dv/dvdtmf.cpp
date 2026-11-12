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
#include <cmath>

#include "dvdtmf.h"

namespace {

const double Pi = 3.14159265358979323846;
const float RowHz[4] = { 697.0f, 770.0f, 852.0f, 941.0f };
const float ColumnHz[4] = { 1209.0f, 1336.0f, 1477.0f, 1633.0f };
// The keypad: row r, column c
const char Keypad[4][5] = { "123A", "456B", "789C", "*0#D" };

bool keypadPosition(char key, int& row, int& column)
{
    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            if (Keypad[r][c] == key)
            {
                row = r;
                column = c;
                return true;
            }
        }
    }

    return false;
}

} // namespace

int DVDTMF::keyValue(char key)
{
    if ((key >= 'a') && (key <= 'd')) {
        key = (char) (key - 'a' + 'A');
    }

    const char *k = keys();

    for (int i = 0; i < 16; i++)
    {
        if (k[i] == key) {
            return i;
        }
    }

    return -1;
}

char DVDTMF::keyChar(int value)
{
    return (value >= 0) && (value < 16) ? keys()[value] : 0;
}

bool DVDTMF::frequencies(char key, float& low, float& high)
{
    int value = keyValue(key);
    int row, column;

    if ((value < 0) || !keypadPosition(keyChar(value), row, column)) {
        return false;
    }

    low = RowHz[row];
    high = ColumnHz[column];
    return true;
}

std::string DVDTMF::validKeys(const std::string& text)
{
    std::string out;

    for (char c : text)
    {
        int value = keyValue(c);

        if (value >= 0) {
            out += keyChar(value);
        }
    }

    return out;
}

// ---- AMBE tone frames -----------------------------------------------------------------------

namespace {

bool validToneId(int id)
{
    return ((id >= 5) && (id <= 122)) || ((id >= 128) && (id <= 163));
}

int bitsAt(const uint8_t *params, int from, int count)
{
    int v = 0;

    for (int i = 0; i < count; i++) {
        v = (v << 1) | (params[from + i] & 1);
    }

    return v;
}

} // namespace

int DVDTMF::ambe2450ToneId(const uint8_t *params)
{
    // u0 = bits 0..11, u1 = 12..23, u2 = 24..34, u3 = 35..48 (mbelib's ambe2450_read_u_fields)
    const int u0 = bitsAt(params, 0, 12);
    const int u1 = bitsAt(params, 12, 12);
    const int u3 = bitsAt(params, 35, 14);

    if (((u0 >> 6) & 0x3F) != 0x3F) {
        return -1;
    }

    if (((u3 & 0xF) != 0) && (((u1 >> 8) & 0xF) != (u1 & 0xF))) {
        return -1;
    }

    const int id = bitsAt(params, 12, 8);
    return validToneId(id) ? id : -1;
}

int DVDTMF::ambe2400ToneId(const uint8_t *params)
{
    const int b0 = (bitsAt(params, 0, 6) << 1) | (params[48] & 1);

    if ((b0 & 0x7E) != 0x7E) {
        return -1;
    }

    // mbelib's ambe2400_decode_tone_index: bits 7..5 through a 3 bit code, then 4, 3..2, 1..0
    static const int t7[8] = { 1, 0, 0, 0, 0, 1, 1, 1 };
    static const int t6[8] = { 0, 0, 0, 1, 1, 1, 1, 0 };
    static const int t5[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    const int def = bitsAt(params, 6, 3);
    const int index = (t7[def] << 7) | (t6[def] << 6) | (t5[def] << 5) | ((params[9] & 1) << 4)
        | ((params[42] & 1) << 3) | ((params[43] & 1) << 2) | ((params[10] & 1) << 1) | (params[11] & 1);

    if ((index >= 5) && (index <= 122)) {
        return index;       // single tones are numbered alike
    }

    if ((index >= 128) && (index <= 143))
    {
        // D-STAR: 128 + 4 x column + row
        const int column = (index - 128) / 4;
        const int row = (index - 128) % 4;
        return ToneIdDTMF + keyValue(Keypad[row][column]);
    }

    if ((index >= 144) && (index <= 147)) {
        return 160 + (index - 144);     // dial, ring, busy, reorder
    }

    return -1;  // a silence frame, or nothing D-STAR renders
}

// ---- Generator ------------------------------------------------------------------------------

DVDTMF::Generator::Generator(int sampleRate) :
    m_sampleRate(sampleRate),
    m_level(0.31622777f),
    m_position(0)
{
}

void DVDTMF::Generator::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    reset();
}

void DVDTMF::Generator::reset()
{
    m_items.clear();
    m_position = 0;
}

bool DVDTMF::Generator::queue(char key, int toneMs, int gapMs)
{
    const int value = keyValue(key);

    if (value < 0) {
        return false;
    }

    Item item;
    item.m_key = keyChar(value);
    item.m_toneSamples = (int) ((int64_t) std::max(0, toneMs) * m_sampleRate / 1000);
    item.m_gapSamples = (int) ((int64_t) std::max(0, gapMs) * m_sampleRate / 1000);
    m_items.push_back(item);
    return true;
}

int DVDTMF::Generator::remainingMs() const
{
    int64_t samples = -m_position;

    for (const Item& item : m_items) {
        samples += item.m_toneSamples + item.m_gapSamples;
    }

    return (int) (std::max<int64_t>(0, samples) * 1000 / m_sampleRate);
}

int DVDTMF::Generator::generate(float *out, int n)
{
    const int ramp = std::max(1, m_sampleRate / 500);  // 2 ms
    int done = 0;

    while ((done < n) && !m_items.empty())
    {
        const Item& item = m_items.front();
        const int length = item.m_toneSamples + item.m_gapSamples;

        if (m_position < item.m_toneSamples)
        {
            float low, high;
            frequencies(item.m_key, low, high);
            const double w1 = 2.0 * Pi * low / m_sampleRate;
            const double w2 = 2.0 * Pi * high / m_sampleRate;
            const int r = std::min(ramp, item.m_toneSamples / 2);

            while ((done < n) && (m_position < item.m_toneSamples))
            {
                double gain = 1.0;

                if (m_position < r) {
                    gain = 0.5 * (1.0 - std::cos(Pi * m_position / r));
                } else if (m_position >= item.m_toneSamples - r) {
                    gain = 0.5 * (1.0 - std::cos(Pi * (item.m_toneSamples - m_position) / r));
                }

                out[done++] = (float) (gain * m_level * (std::sin(w1 * m_position) + std::sin(w2 * m_position)));
                m_position++;
            }
        }

        while ((done < n) && (m_position >= item.m_toneSamples) && (m_position < length))
        {
            out[done++] = 0.0f;
            m_position++;
        }

        if (m_position >= length)
        {
            m_items.pop_front();
            m_position = 0;
        }
    }

    return done;
}

// ---- Detector -------------------------------------------------------------------------------

DVDTMF::Detector::Detector(int sampleRate) :
    m_dominance(0.7f)
{
    setSampleRate(sampleRate);
}

void DVDTMF::Detector::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    m_window = std::max(16, (int) std::lround(sampleRate * 0.0128));
    m_hop = m_window / 2;
    const float scale[3] = { 1.0f, 0.96f, 1.04f };

    for (int s = 0; s < 3; s++)
    {
        for (int i = 0; i < 8; i++)
        {
            const float f = (i < 4 ? RowHz[i] : ColumnHz[i - 4]) * scale[s];
            m_coeff[s][i] = (float) (2.0 * std::cos(2.0 * Pi * f / sampleRate));
        }
    }

    reset();
}

void DVDTMF::Detector::reset()
{
    m_ring.assign(m_window, 0.0f);
    m_ringPos = 0;
    m_sinceHop = 0;
    m_samples = 0;
    m_state = StateIdle;
    m_candidate = 0;
    m_candidateRun = 0;
    m_candidateStart = 0;
    m_key = 0;
    m_keyStart = 0;
    m_keyWindows = 0;
    m_missRun = 0;
    m_lastEnd = -1;
}

char DVDTMF::Detector::decide(const float *window) const
{
    const int n = m_window;
    double total = 0.0;

    for (int i = 0; i < n; i++) {
        total += (double) window[i] * window[i];
    }

    // -45 dBFS a tone: a sine of amplitude a holds n a^2 / 2 of energy
    const double a = 0.0056234;
    const double minTone = n * a * a / 2.0;

    if (total < 2.0 * minTone) {
        return 0;
    }

    // Goertzel: the energy of a sine at the frequency is about 2 |X|^2 / n
    double e[3][8];

    for (int s = 0; s < 3; s++)
    {
        for (int t = 0; t < 8; t++)
        {
            const double coeff = m_coeff[s][t];
            double s1 = 0.0, s2 = 0.0;

            for (int i = 0; i < n; i++)
            {
                const double s0 = window[i] + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }

            e[s][t] = 2.0 * (s1 * s1 + s2 * s2 - coeff * s1 * s2) / n;
        }
    }

    int row = 0, column = 4;

    for (int t = 1; t < 4; t++)
    {
        if (e[0][t] > e[0][row]) {
            row = t;
        }
        if (e[0][t + 4] > e[0][column]) {
            column = t + 4;
        }
    }

    const double er = e[0][row];
    const double ec = e[0][column];

    if ((er < minTone) || (ec < minTone)) {
        return 0;
    }

    // Each 8 dB over the others of its group
    for (int t = 0; t < 4; t++)
    {
        if ((t != row) && (er < 6.31 * e[0][t])) {
            return 0;
        }
        if ((t + 4 != column) && (ec < 6.31 * e[0][t + 4])) {
            return 0;
        }
    }

    // Twist: the column tone at most 4 dB the stronger, the row tone at most 8 dB
    if ((ec > 2.512 * er) || (er > 6.31 * ec)) {
        return 0;
    }

    // The two tones hold most of the energy
    if (er + ec < m_dominance * total) {
        return 0;
    }

    // Each nearer its nominal frequency than 4% either side
    for (int t : { row, column })
    {
        if ((e[1][t] > e[0][t]) || (e[2][t] > e[0][t])) {
            return 0;
        }
    }

    return Keypad[row][column - 4];
}

void DVDTMF::Detector::finishKey(std::vector<Key>& keys)
{
    // The windows that decide a key are those within its tone (one straddling an edge sees the
    // tone's spectrum spread by the edge), so the tone spans them
    Key key;
    key.m_key = m_key;
    key.m_start = m_keyStart;
    const int64_t length = (int64_t) (m_keyWindows - 1) * m_hop + m_window;
    key.m_lengthMs = (int) (length * 1000 / m_sampleRate);
    key.m_gapMs = m_lastEnd < 0 ? -1 : (int) (std::max<int64_t>(0, m_keyStart - m_lastEnd) * 1000 / m_sampleRate);
    keys.push_back(key);
    m_lastEnd = m_keyStart + length;
    m_state = StateIdle;     // the windows already counted towards the next key stay counted
}

void DVDTMF::Detector::process(const float *samples, int n, std::vector<Key>& keys)
{
    std::vector<float> window(m_window);

    for (int i = 0; i < n; i++)
    {
        m_ring[m_ringPos] = samples[i];
        m_ringPos = (m_ringPos + 1) % m_window;
        m_samples++;

        if ((++m_sinceHop < m_hop) || (m_samples < m_window)) {
            continue;
        }

        m_sinceHop = 0;

        for (int k = 0; k < m_window; k++) {
            window[k] = m_ring[(m_ringPos + k) % m_window];
        }

        const char decided = decide(window.data());
        const int64_t windowStart = m_samples - m_window;

        if (decided && (decided == m_candidate))
        {
            m_candidateRun++;
        }
        else if (decided)
        {
            m_candidate = decided;
            m_candidateRun = 1;
            m_candidateStart = windowStart;
        }
        else
        {
            m_candidate = 0;
            m_candidateRun = 0;
        }

        if (m_state == StateIdle)
        {
            if (m_candidateRun >= StartWindows)
            {
                m_state = StateSounding;
                m_key = m_candidate;
                m_keyStart = m_candidateStart;
                m_keyWindows = m_candidateRun;
                m_missRun = 0;
            }
        }
        else if (decided == m_key)
        {
            m_keyWindows++;
            m_missRun = 0;
        }
        else if (++m_missRun >= EndWindows)
        {
            finishKey(keys);
        }
    }
}
