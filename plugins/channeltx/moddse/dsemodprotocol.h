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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODPROTOCOL_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODPROTOCOL_H_

#include <algorithm>
#include <cstdint>
#include <complex>
#include <functional>
#include <string>
#include <vector>

#include "dsemodsettings.h"
#include "dsemodvocoder.h"

/**
 * Abstract digital voice protocol frame builder.
 *
 * A protocol turns a stream of vocoder frames into a stream of symbols. Symbols
 * are expressed as FM deviation levels in {-3, -1, +1, +3} (units of a third of
 * the peak deviation): 2-FSK protocols use only +3/-3.
 *
 * Vocoder frames are pulled through the frame source callback which must fill
 * getFrameBytes(getVocoderRate()) bytes in the vocoder's canonical order and
 * return true, or return false when no frame is available (the protocol then
 * inserts silence).
 */
class DSEModProtocol
{
public:
    typedef std::function<bool(uint8_t *frame)> FrameSource;

    enum Shaping
    {
        ShapingGMSK,    //!< Gaussian filtered 2-FSK (D-STAR)
        ShapingRRC4FSK, //!< Root raised cosine 4FSK (DMR, YSF, NXDN, dPMR, P25)
        ShapingLinear,  //!< Root raised cosine shaped complex symbols (Angel61): the IQ goes out as it is, no FM modulator
        ShapingOFDM     //!< OFDM frames of cells (Angel61 revision 3 HF): DVAngel61OFDM's modulator, the IQ as it is
    };

    DSEModProtocol() {}
    virtual ~DSEModProtocol() {}

    //! Latch settings and start a transmission (preamble, header ...)
    virtual void start(const DSEModSettings& settings) = 0;
    //! Request end of transmission. Symbols keep being produced until isActive() is false.
    virtual void stop() = 0;
    //! Abort immediately without end of transmission sequence
    virtual void abort() = 0;
    //! True while there are symbols to produce
    virtual bool isActive() const = 0;
    //! Produce the next block of symbols. Returns false if nothing was produced (idle).
    virtual bool nextFrame(std::vector<int8_t>& levels) = 0;
    //! Linear protocols (getShaping() == ShapingLinear): the next block of complex symbols at unit mean power
    virtual bool nextSymbols(std::vector<std::complex<float>>& symbols) { (void) symbols; return false; }
    bool isLinear() const { return getShaping() == ShapingLinear; }
    //! OFDM protocols (getShaping() == ShapingOFDM): the next frame's cells (DVAngel61's layout, symbol l carrier k at
    //! l Nc + k) and the constellation of its payload (DVAngel61::Constellation)
    virtual bool nextCells(std::vector<std::complex<float>>& cells, int& constellation) { (void) cells; (void) constellation; return false; }
    bool isOFDM() const { return getShaping() == ShapingOFDM; }
    //! The FIFO holds I and Q rather than an FM modulating signal: linear and OFDM protocols
    bool isIQ() const { return isLinear() || isOFDM(); }
    //! An OFDM protocol's profile (DVAngel61::Profile)
    virtual int getOFDMProfile() const { return 2; }
    //! Protocols with a data channel: queue a text message. Returns false if unsupported or not accepted.
    virtual bool queueMessage(const std::string& text) { (void) text; return false; }
    //! Protocols with a position slot: the station's position (valid false: none)
    /**
     * The station's position, and its velocity when something knows it. `speed` is km/h and
     * `course` degrees true, or the unknown values of `DVAngel61::Position`; `source` is 0 for
     * GNSS, 1 set by hand, 2 network or other (ANGEL61.md 3.2).
     */
    virtual void setPosition(bool valid, double latitude, double longitude, float altitude,
        int speed = -1, int course = -1, int source = 1)
    {
        (void) valid; (void) latitude; (void) longitude; (void) altitude;
        (void) speed; (void) course; (void) source;
    }

    /**
     * Speed and course from two fixes, for a station whose position source gives neither -
     * which is SDRangel's, since its settings carry latitude, longitude and altitude and
     * nothing else. Great circle distance and initial bearing, as an APRS tracker does it.
     *
     * Below `StationaryKmh` the answer is "not moving": a stationary receiver wanders by a few
     * metres between fixes, which over five seconds reads as two or three km/h on a random
     * bearing, and a parked station reporting a course is worse than one reporting none.
     */
    static void velocity(double latitude1, double longitude1, double latitude2, double longitude2,
        double seconds, int& speedKmh, int& courseDegrees);

    static constexpr double StationaryKmh = 3.0;   //!< below this the station is not moving
    static constexpr int MaxSpeedKmh = 1021;       //!< what the 10 bit field holds, 1022 = more
    //! A data only transmission (no voice frames wanted): the protocol ends it by itself when its data is sent
    virtual bool isDataOnly() const { return false; }
    /**
     * What the receiver side reports about the destination (Angel61 2.10), before start():
     * requestedTier: the tier the destination's last frame asked of this station (an "auto" rate
     * follows it); sendTier: the tier to ask of it, this station's judgement of its transmissions
     */
    virtual void setLinkState(int requestedTier, int sendTier) { (void) requestedTier; (void) sendTier; }

    virtual DSEModSettings::DSEModMode getMode() const = 0;
    virtual DSEModVocoder::Rate getVocoderRate() const = 0;
    //! The PCM rate the audio is fed at until a transmission opens its encoder: the highest any
    //! transmission of the protocol may need (Angel61 WB: 48 kHz, as its tier 2 may be Opus). From
    //! key up the source feeds the encoder's own rate; a frame queued before is converted
    virtual int getInputSampleRate() const { return DSEModVocoder::getPcmSampleRate(getVocoderRate()); }
    virtual Shaping getShaping() const = 0;
    //! A linear protocol's output level (RMS) for unit power symbols: 16-QAM wants 3 dB more
    //! headroom for its peaks than QPSK (ANGEL61.md 2.1)
    virtual float getLinearLevel() const { return 0.5f; }
    //! A linear protocol's samples per symbol at 48 kS/s: its pulse (10 for Angel61 NB, 5 for WB)
    virtual int getLinearSamplesPerSymbol() const { return 10; }
    //! Root raised cosine shaping: the roll off and the span in symbols (M17 uses 0.5, the others 0.2)
    virtual double getShapingAlpha() const { return 0.2; }
    virtual int getShapingSpan() const { return 8; }
    virtual const char *getName() const = 0;
    int getPeakDeviation() const { return peakDeviation(getMode()); } //!< Hz
    int getSymbolRate() const { return symbolRate(getMode()); }       //!< symbols/s

    // Modulation parameters per mode: the single source of truth for the source (FM deviation,
    // channel filter) and the processor (shaper)
    static int peakDeviation(DSEModSettings::DSEModMode mode);      //!< Peak FM deviation (Hz) at level +/-3
    static int symbolRate(DSEModSettings::DSEModMode mode);         //!< Symbols/s: 2400, 4800 or 6000
    static int occupiedBandwidth(DSEModSettings::DSEModMode mode);  //!< Carson bandwidth (Hz) of the modulated signal
    //! Factory
    static DSEModProtocol *create(DSEModSettings::DSEModMode mode);
    //! The vocoder rate of a mode's protocol (what create(mode)->getVocoderRate() would say)
    static DSEModVocoder::Rate vocoderRate(DSEModSettings::DSEModMode mode);
    //! The PCM rate of a mode's protocol (what create(mode)->getInputSampleRate() would say)
    static int inputSampleRate(DSEModSettings::DSEModMode mode);

    //! Number of vocoder frames consumed so far in the current transmission
    virtual uint32_t getVoiceFrameCount() const = 0;
    //! Largest number of vocoder frames one nextFrame() call consumes (sets the priming lead)
    virtual int getFramesPerBlock() const = 0;
    //! Vocoder frames the priming lead need not hold because the protocol sends that much time before
    //! it takes any (a preamble of frames with no voice)
    virtual int getPrimeCredit() const { return 0; }

    void setFrameSource(FrameSource source) { m_frameSource = source; }

    //! Dibit to 4FSK level: 01 -> +3, 00 -> +1, 10 -> -1, 11 -> -3 (ETSI/TIA C4FM convention)
    static int8_t dibitToLevel(uint8_t dibit)
    {
        static const int8_t levels[4] = { 1, 3, -1, -3 };
        return levels[dibit & 3];
    }

    //! Append bits (MSB of pair first) as 4FSK dibit levels
    static void appendDibits(std::vector<int8_t>& levels, const uint8_t *bits, int nbBits)
    {
        for (int i = 0; i + 1 < nbBits; i += 2) {
            levels.push_back(dibitToLevel((bits[i] << 1) | bits[i + 1]));
        }
    }

    //! Unpack bytes to bits, MSB first
    static void bytesToBits(const uint8_t *bytes, int nbBits, uint8_t *bits)
    {
        for (int i = 0; i < nbBits; i++) {
            bits[i] = (bytes[i / 8] >> (7 - (i % 8))) & 1;
        }
    }

protected:
    FrameSource m_frameSource;

    //! Get a vocoder frame or silence. Returns true if a real frame was obtained.
    bool pullVocoderFrame(uint8_t *frame)
    {
        if (m_frameSource && m_frameSource(frame)) {
            return true;
        }

        // The whole frame: V1's codebook count follows its 20 bytes (0: all 16), and the caller's
        // buffer may hold anything there
        const uint8_t *silence = DSEModVocoder::getSilenceFrame(getVocoderRate());
        std::fill(frame, frame + DSEModVocoder::m_maxFrameBytes, 0);
        std::copy(silence, silence + DSEModVocoder::getFrameBytes(getVocoderRate()), frame);
        return false;
    }
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODPROTOCOL_H_
