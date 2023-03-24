///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef SDRBASE_DSP_MORSEDEMOD_H_
#define SDRBASE_DSP_MORSEDEMOD_H_

#include <QString>

#include "dsp/nco.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "export.h"

// Morse code demodulator for use with VOR and ILS
class SDRBASE_API MorseDemod {

public:
    class SDRBASE_API MsgReportIdent : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getIdent() const { return m_ident; }

        static MsgReportIdent* create(QString ident)
        {
            return new MsgReportIdent(ident);
        }

    private:
        QString m_ident;

        MsgReportIdent(QString ident) :
            Message(),
            m_ident(ident)
        {
        }
    };

    MorseDemod();
    void processOneSample(const Complex &magc);
    void applyChannelSettings(int channelSampleRate);
    void applySettings(int identThreshold);
    void reset();
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    MessageQueue *getMessageQueueToChannel() const { return m_messageQueueToChannel; }

private:

    MessageQueue *m_messageQueueToChannel;
    NCO m_ncoIdent;
    Bandpass<Complex> m_bandpassIdent;
    Lowpass<Complex> m_lowpassIdent;
    MovingAverageUtilVar<Real, double> m_movingAverageIdent;
    static const int m_identBins = 20;
    Real m_identMaxs[m_identBins];
    Real m_identNoise;
    int m_binSampleCnt;
    int m_binCnt;
    int m_samplesPerDot7wpm;
    int m_samplesPerDot10wpm;
    int m_prevBit;
    int m_bitTime;
    QString m_ident;

    int m_identThreshold;

};

#endif /* SDRBASE_DSP_MORSEDEMOD_H_ */

