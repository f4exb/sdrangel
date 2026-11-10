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

#ifndef INCLUDE_DSP_DV_DVDTMF_H
#define INCLUDE_DSP_DV_DVDTMF_H

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "export.h"

/**
 * DTMF (ITU-T Q.23): the 16 keys, their tones, a generator, a detector, and the keys of AMBE's
 * tone frames. Shared by the DSE Modulator and the DSD Demodulator (sdrbase/dsp/dv/ANGEL61.md 3.8
 * for Angel61, which carries DTMF as data; the other protocols carry it in AMBE tone frames or,
 * worst, through their voice codec).
 *
 * A key's **value** is its 4 bit code in Angel61's DTMF slot and its offset in AMBE+2's tone ids
 * (128 + value): `0`-`9` 0 to 9, `A`-`D` 10 to 13, `*` 14, `#` 15.
 */
class SDRBASE_API DVDTMF
{
public:
    //! The keys in value order
    static const char *keys() { return "0123456789ABCD*#"; }
    //! A key's value, 0 to 15 (`a`-`d` taken as `A`-`D`), or -1 for any other character
    static int keyValue(char key);
    //! The key of a value, 0 for one out of range
    static char keyChar(int value);
    //! Row (low group) and column (high group) frequencies of a key; false for another character
    static bool frequencies(char key, float& low, float& high);
    //! A string's DTMF keys, upper case, others dropped
    static std::string validKeys(const std::string& text);

    // ---- AMBE tone frames (mbelib 3's docs/tone-frames.md) ------------------------------------
    //! AMBE+2 tone ids: 5 to 122 single tones, 128 to 143 DTMF (128 + value), 144 to 159 KNOX,
    //! 160 to 163 call progress (dial, ring, busy, reorder)
    static const int ToneIdDTMF = 128;
    /**
     * The tone id of AMBE+2 (3600x2450) parameter bits, 49 of them one per byte in mbelib's order
     * (`ambe_d`), as mbelib's decoder classifies a frame: `111111` at the head of u0, and u3's last
     * four bits zero or u1's two nibbles equal; the id in u1's first 8 bits. -1 for a voice frame,
     * or for an id that is no tone.
     */
    static int ambe2450ToneId(const uint8_t *params);
    /**
     * The tone id, in AMBE+2's numbering, of D-STAR (3600x2400) parameter bits in mbelib's order:
     * a fundamental index of 126 or 127, the tone index in d[6..11], d[42], d[43]; D-STAR's own dual
     * tone numbering (DTMF 128 + 4 x column + row, call progress 144 to 147) converted. -1 for a
     * voice frame, a silence frame, or an index that is no tone.
     */
    static int ambe2400ToneId(const uint8_t *params);
    //! The key of a tone id, 0 when it is no DTMF tone
    static char toneIdKey(int toneId) { return (toneId >= ToneIdDTMF) && (toneId < ToneIdDTMF + 16) ? keyChar(toneId - ToneIdDTMF) : 0; }

    /**
     * Tones for keys, in the order queued: each key's two tones at `level` (each at -10 dBFS by
     * default, 1.0 being full scale) for its tone length, with 2 ms raised cosine ramps, then its gap
     * of silence.
     */
    class SDRBASE_API Generator
    {
    public:
        explicit Generator(int sampleRate = 8000);
        void setSampleRate(int sampleRate);
        void setLevel(float level) { m_level = level; }
        void reset();
        //! Queue a key; false for a character that is no key
        bool queue(char key, int toneMs, int gapMs);
        bool active() const { return !m_items.empty(); }
        //! Keys queued, the one sounding included
        int pending() const { return (int) m_items.size(); }
        //! The time left to play, milliseconds
        int remainingMs() const;
        /**
         * The next samples of the queue into `out` (tones, and zeros in the gaps), up to n; returns
         * how many, fewer than n when the queue ran out (0 when it is idle).
         */
        int generate(float *out, int n);

    private:
        struct Item { char m_key; int m_toneSamples; int m_gapSamples; };
        int m_sampleRate;
        float m_level;
        std::deque<Item> m_items;
        int m_position;         //!< samples into the item at the front
    };

    /**
     * Keys in audio, after ITU-T Q.24: 12.8 ms Goertzel windows every 6.4 ms (spandsp's block; a
     * longer window loses a tone 1.5% off the nominal frequency at the nominal bin), each deciding a key
     * when one tone of each group stands out (8 dB over the others of its group), the two are within
     * the twist limits (the column tone at most 4 dB and the row tone at most 8 dB the stronger),
     * they hold most of the window's energy (so speech, whose energy spreads, seldom passes), each
     * is nearer its nominal frequency than 4% either side (a tone 1.5% off passes, 3.5% off does
     * not), and the level is above -45 dBFS per tone. Five windows in a row start a key (38 ms of
     * tone: 40 ms are accepted, 20 ms are not); four without it end it (a dropout of a few ms does
     * not split a key). A key is reported when it ends, with its measured length and the silence
     * before it.
     */
    class SDRBASE_API Detector
    {
    public:
        struct Key
        {
            char m_key = 0;
            int64_t m_start = 0;    //!< sample index of the tone's start
            int m_lengthMs = 0;     //!< the tone's length
            int m_gapMs = -1;       //!< the silence since the previous key ended, -1 for the first
        };

        explicit Detector(int sampleRate = 8000);
        void setSampleRate(int sampleRate);
        int getSampleRate() const { return m_sampleRate; }
        //! The share of a window's energy the two tones must hold (default 0.7)
        void setDominance(float share) { m_dominance = share; }
        void reset();
        //! Samples, full scale 1.0; the keys that ended in them are appended to `keys`
        void process(const float *samples, int n, std::vector<Key>& keys);
        //! A key has started and not yet ended
        bool sounding() const { return m_state == StateSounding; }
        //! The key sounding, 0 for none
        char soundingKey() const { return sounding() ? m_key : 0; }
        //! The sample index its tone started at, -1 when none sounds
        int64_t soundingSince() const { return sounding() ? m_keyStart : -1; }
        //! Samples from a tone's start until it is decided a key: how far a caller that must
        //! remove the tone from the audio has to delay it
        int decisionDelay() const { return m_window + (StartWindows - 1) * m_hop; }
        //! Samples processed
        int64_t samples() const { return m_samples; }

    private:
        static const int StartWindows = 5;
        static const int EndWindows = 4;
        enum State { StateIdle, StateSounding };

        int m_sampleRate;
        int m_window;               //!< samples a window
        int m_hop;                  //!< samples between windows
        float m_dominance;
        std::vector<float> m_ring;  //!< the last window's samples
        int m_ringPos;
        int m_sinceHop;
        int64_t m_samples;
        State m_state;
        char m_candidate;           //!< the key the last windows decided
        int m_candidateRun;         //!< how many in a row
        int64_t m_candidateStart;   //!< the first of them's start
        char m_key;
        int64_t m_keyStart;
        int m_keyWindows;           //!< windows that decided the sounding key
        int m_missRun;              //!< windows in a row that did not
        int64_t m_lastEnd;          //!< the last key's end, -1 for none
        float m_coeff[3][8];        //!< Goertzel coefficients: nominal, -4%, +4% of the eight tones

        char decide(const float *window) const;
        void finishKey(std::vector<Key>& keys);
    };
};

#endif // INCLUDE_DSP_DV_DVDTMF_H
