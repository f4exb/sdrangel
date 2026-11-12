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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODER_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODER_H_

#include <cstdint>
#include <functional>

class Message;

/**
 * Abstract speech encoder (vocoder) back end.
 *
 * Input is one 20 ms frame of 16 bit PCM: 160 samples at 8 kHz for the MBE
 * rates, 480 at 24 kHz for V1, 960 at 48 kHz for Opus (getPcmFrameSize /
 * getPcmSampleRate). Output is one vocoder frame:
 *  - AMBE 3600x2400 (D-STAR): 72 bits, 9 bytes, D-STAR on-air order (LSB first per byte)
 *  - AMBE+2 3600x2450: 72 bits, 9 bytes, DMR on-air order (MSB first)
 *  - IMBE 7200x4400: 144 bits, 18 bytes, P25 Phase 1 on-air order
 *  - V1 (Angel61): a 13.33 ms packet of 16 codebooks, 20 bytes, the protocol truncating it
 *  - Opus 18400 (Angel61 WB tier 3): one 80 ms packet of 184 bytes for four 20 ms input frames
 * The protocol layer maps this to its on-air bit order.
 *
 * Encoding may be asynchronous: the result is delivered through the frame sink
 * callback, possibly from a later call to handleMessage() on the owner's thread.
 */
class DSEModVocoder
{
public:
    enum Rate
    {
        RateNone = 0,
        Rate3600x2400 = 1,   //!< D-STAR (SerialDV::DVRate3600x2400)
        Rate3600x2450 = 2,   //!< DMR, dPMR, NXDN, YSF (SerialDV::DVRate3600x2450)
        RateIMBE7200x4400 = 3, //!< P25 Phase 1, YSF VW (SerialDV::DVRate7200x4400)
        RateCodec23200 = 6,   //!< M17 (Codec2 3200 bit/s, 8 kHz audio)
        RateVocos = 9,        //!< Angel61 V1 (vocoscodec): a 13.33 ms packet of 16 codebooks (20 bytes) from 24 kHz audio; the protocol truncates it
        RateOpus80 = 10       //!< Angel61 WB tier 3 audio (O1): one 80 ms Opus packet of 184 bytes, 48 kHz audio
    };                        // (4, 5, 7 and 8 were Angel61 revision 2's, ANGEL61.md 12)

    static constexpr int m_maxFrameBytes = 184;    //!< the largest packet: the O1 packet (ANGEL61.md 3.1.2)
    //! In a V1 packet's buffer, after its 20 bytes: the codebooks it carries when a relay gives it
    //! fewer than the encoder's 16 (ANGEL61.md 3.1.1's truncation), 0 for all 16
    static constexpr int m_vocosCodebooksByte = 20;

    //! Row (low) and column (high) frequencies of a DTMF key (0-9, A-D, *, #). Returns false for other characters.
    static bool dtmfFrequencies(char digit, float& low, float& high);
    static constexpr int m_pcmFrameSize = 160;     //!< PCM samples per frame of the 8 kHz rates
    static constexpr int m_maxPcmFrameSize = 960;  //!< of any rate (20 ms at 48 kHz, Opus)
    static bool isOpus(Rate rate) { return rate == RateOpus80; }
    static bool isVocos(Rate rate) { return rate == RateVocos; }
    //! The audio fed to the encoder: 20 ms frames at 8 kHz (MBE, Codec2), 24 kHz (V1) or 48 kHz (Opus)
    static int getPcmSampleRate(Rate rate) { return isOpus(rate) ? 48000 : isVocos(rate) ? 24000 : 8000; }
    static int getPcmFrameSize(Rate rate) { return getPcmSampleRate(rate) / 50; }
    //! The audio one encoded frame carries: 20 ms, or 80 ms for an Opus packet (four input frames);
    //! V1's 13.33 ms rounded down (getFrameSamples48k is exact)
    static int getFrameMs(Rate rate) { return isOpus(rate) ? 80 : isVocos(rate) ? 13 : 20; }
    //! The audio one encoded frame carries, in samples at 48 kHz: 960, 3840 for Opus, 640 for V1
    static int getFrameSamples48k(Rate rate) { return isVocos(rate) ? 640 : 48 * getFrameMs(rate); }

    typedef std::function<void(uint32_t sequence, const uint8_t *frame)> FrameSink;

    DSEModVocoder() : m_rate(RateNone) {}
    virtual ~DSEModVocoder() {}

    virtual bool open(Rate rate) = 0;
    virtual void close() = 0;
    //! The transmission has ended: closed, unless the back end is too slow to open at each key-up
    virtual void endTransmission() { close(); }
    //! True while open and able to encode
    virtual bool isReady() const = 0;
    //! True if open() can be expected to succeed (back end built in, device connected)
    virtual bool canOpen() const = 0;
    //! The rates this encoder makes: the MBE rates, unless the back end says otherwise (Codec2, V1, Opus)
    virtual bool canEncode(Rate rate) const { return !isOpus(rate) && !isVocos(rate) && (rate != RateCodec23200); }
    //! Encode one 160 sample PCM frame. Result arrives via the frame sink.
    virtual void encode(uint32_t sequence, const int16_t *pcm) = 0;
    //! Encode a tone frame (single tone: f2 = 0; dual tone: f1 and f2; amplitude 0..127 on the AMBE-3000R's law).
    //! Returns false if the back end or the rate has no tone frames: the caller then encodes the audio instead.
    virtual bool encodeTone(uint32_t sequence, float f1, float f2, int amplitude) { (void) sequence; (void) f1; (void) f2; (void) amplitude; return false; }
    //! Handle asynchronous replies. Returns true if the message was consumed.
    virtual bool handleMessage(const Message& msg) { (void) msg; return false; }
    //! Number of frames of algorithmic/transport delay before a result is available
    virtual int getLatencyFrames() const = 0;
    virtual const char *getName() const = 0;

    void setFrameSink(FrameSink sink) { m_frameSink = sink; }
    Rate getRate() const { return m_rate; }

    static int getFrameBytes(Rate rate) {
        return rate == RateOpus80 ? 184 : rate == RateVocos ? 20 : rate == RateIMBE7200x4400 ? 18
            : rate == RateCodec23200 ? 8 : 9;
    }
    static int getFrameBits(Rate rate) { return 8 * getFrameBytes(rate); }
    //! A frame of silence: fixed for the MBE rates; for Codec2, V1 and Opus the packet an encoder
    //! produced for silence, zeros until one has
    static const uint8_t *getSilenceFrame(Rate rate);
    static void setSilenceFrame(Rate rate, const uint8_t *frame);

protected:
    Rate m_rate;
    FrameSink m_frameSink;

    void deliver(uint32_t sequence, const uint8_t *frame)
    {
        if (m_frameSink) {
            m_frameSink(sequence, frame);
        }
    }

private:
    static const uint8_t m_silence3600x2400[9];
    static const uint8_t m_silence3600x2450[9];
    static const uint8_t m_silenceIMBE7200x4400[18];
    static uint8_t m_silenceCodec23200[8];
    static uint8_t m_silenceVocos[20];
    static uint8_t m_silenceOpus80[184];
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODER_H_
