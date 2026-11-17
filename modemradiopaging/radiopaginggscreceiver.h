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

#ifndef MODEMRADIOPAGING_RADIOPAGINGGSCRECEIVER_H
#define MODEMRADIOPAGING_RADIOPAGINGGSCRECEIVER_H

#include <deque>
#include <vector>

#include "export.h"
#include "radiopagingdedup.h"
#include "radiopagingfskdemod.h"
#include "radiopaginggscdecoder.h"

// Sequential Golay receiver: FM discriminator samples to messages of both polarities.
//
// One demodulator at 600 baud, the rate of commas and data blocks; the 300 bit/s words
// are two symbols per bit and are decided by the decoders on the sum of the two soft
// symbols. The demodulator starts with acquisition loop gains and is locked (tracking
// gains) while a decoder is synchronised, that is from the confirmation of the preamble
// (three preamble words) to the end of the transmission. During the alert time and the
// voice message of a voice page the timing goes back to acquisition gains and the levels
// are held.
//
// The polarity of a battery saver transmission is given by its preamble and start code,
// only the decoder of the right polarity decodes it. A non battery saver transmission
// (Rep. 900-2 Annex I 2.5) has neither, so both decoders decode it. The polarity is the
// same for the whole transmission and is decided by what in it tells the polarities
// apart: a data block whose check character (Annex I 2.6.1) is right (every block fails
// it in the wrong polarity), or the activation code of a voice page (seen before its
// complement only in the right polarity). Without either, a positive frequency shift is
// a binary 1 (Annex I 2): the normal decoder. The messages of a non battery saver
// transmission are held until it is decided, at the latest until it ends.
class MODEMRADIOPAGING_API RadioPagingGscReceiver
{
public:
    static const int m_baud = 600;

    struct Status
    {
        bool m_locked;                              //!< Demodulator locked (a transmission is being received)
        RadioPagingGscDecoder::Status m_normal;
        RadioPagingGscDecoder::Status m_inverted;
        float m_deviation;                          //!< Measured deviation, discriminator units
        float m_dc;                                 //!< Measured DC offset (carrier offset), discriminator units
    };

    RadioPagingGscReceiver();

    //!< sampleRate in Hz
    void create(double sampleRate);
    void reset();
    //!< The decoders back to hunting, what they were receiving dropped without a message: a lock the receiver of
    //!< the channel does not take (another protocol is receiving). The demodulator, the time and the duplicates stay
    void dropLock();
    void setPolarities(bool normal, bool inverted) { m_normalEnabled = normal; m_invertedEnabled = inverted; }
    //!< Enable the detection of the non battery saver pattern. Off by default
    void setNonBatterySaver(bool enable);
    //!< Longest voice message without a turn-off word, in seconds
    void setVoiceTimeout(double seconds);
    //!< Pages remembered for duplicates: the last ones of each pager, within this time (default 60 s)
    void setDuplicateWindow(double seconds) { m_dedup.setWindow(seconds); }
    //!< Frequency of the receiver, Hz: a pager is on one frequency (part of the duplicate key)
    void setFrequency(int64_t frequency) { m_dedup.setFrequency(frequency); }
    //!< Messages remembered for duplicate detection, of a polarity (0 normal, 1 inverted) or of both (-1)
    int remembered(int polarity = -1) const { return (int) m_dedup.messages(m_time, polarity); }

    void process(float sample);
    //!< The signal is gone: finish the transmissions in progress
    void flush();

    bool hasMessage() const { return !m_messages.empty(); }
    RadioPagingGscDecoder::Message takeMessage();
    Status getStatus() const;
    //!< A voice page is in its alert time or voice message, until its end
    bool inVoice() const { return m_normal.inVoice() || m_inverted.inVoice(); }
    //!< A voice page is in its voice message (the alert time is over), until its end
    bool inVoiceMessage() const { return m_normal.inVoiceMessage() || m_inverted.inVoiceMessage(); }
    //!< A transmission is being decoded
    bool locked() const { return isLocked(); }
    //!< DC offset and deviation were just measured on a known pattern: the DC offset, discriminator units
    bool takeLevelsMeasured(float& dc);
    //!< The input was moved by delta (frequency correction before the discriminator)
    void offsetLevels(float delta) { m_demod.offsetLevels(delta); }

private:
    bool m_normalEnabled;
    bool m_levelsMeasured = false;
    float m_measuredDc = 0.0f;
    void fitKnown(const RadioPagingGscDecoder::KnownPattern& pattern);
    bool m_invertedEnabled;
    RadioPagingFskDemod m_demod;
    RadioPagingGscDecoder m_normal;
    RadioPagingGscDecoder m_inverted;
    std::deque<RadioPagingGscDecoder::Message> m_messages;

    // Non battery saver transmission being received by both decoders
    enum NbsPolarity {
        NbsUndecided,
        NbsNormal,
        NbsInverted
    };
    NbsPolarity m_nbsPolarity;
    std::vector<RadioPagingGscDecoder::Message> m_nbsPending[2];   //!< Held messages, by decoder polarity
    RadioPagingDedup m_dedup{"gsc"};
    double m_time = 0.0;                        //!< Seconds of signal received, for the duplicate window

    void collect();
    void takeFrom(RadioPagingGscDecoder& decoder);
    void nonBatterySaverMessage(const RadioPagingGscDecoder::Message& message);
    void endNonBatterySaver();
    bool isLocked() const;
    //!< +1 if the message shows its decoder has the right polarity, -1 the wrong one, 0 if it tells nothing
    static int polarityEvidence(const RadioPagingGscDecoder::Message& message);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGGSCRECEIVER_H
