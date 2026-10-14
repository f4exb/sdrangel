///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_SELCALMESSAGES_H
#define INCLUDE_SELCALMESSAGES_H

#include <QDateTime>
#include <QString>

#include "util/message.h"

#include "selcaldecoder.h"

namespace SELCALMessages {

struct Call {
    QString m_code;
    QDateTime m_dateTime;
    qint64 m_frequency = 0;
    float m_rssi = -120;
    float m_snr = 0;                 // dB, weaker tone relative to the noise in a tone bin
    float m_toneFrequencyOffset = 0; // Hz, measured from the tones
};

struct Status {
    SELCALDecoder::State m_state = SELCALDecoder::State::Idle;
    std::array<double, SELCALDecoder::m_toneCount> m_levels{}; // dB relative to noise
    SELCALDecoder::Pair m_pair{{-1, -1}};
    bool m_carrierLocked = false;
    bool m_squelchOpen = false;
    double m_carrierFrequencyOffset = 0;
    double m_toneFrequencyOffset = 0;
};

class MsgCall : public Message {
    MESSAGE_CLASS_DECLARATION
public:
    static MsgCall *create(const Call& call)
    {
        return new MsgCall(call);
    }

    const Call& getMessage() const
    {
        return m_call;
    }

private:
    explicit MsgCall(const Call& call) :
        m_call(call)
    {
    }

    Call m_call;
};

class MsgStatus : public Message {
    MESSAGE_CLASS_DECLARATION
public:
    static MsgStatus *create(const Status& status)
    {
        return new MsgStatus(status);
    }

    const Status& getStatus() const
    {
        return m_status;
    }

private:
    explicit MsgStatus(const Status& status) :
        m_status(status)
    {
    }

    Status m_status;
};

} // namespace SELCALMessages

#endif
