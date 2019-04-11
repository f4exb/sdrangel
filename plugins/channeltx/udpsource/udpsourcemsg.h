///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEMSG_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEMSG_H_

#include "util/message.h"

/**
 * Message(s) used to communicate back from UDPSinkUDPHandler to UDPSource
 */
class UDPSourceMessages
{
public:
    class MsgSampleRateCorrection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getCorrectionFactor() const { return m_correctionFactor; }
        float getRawDeltaRatio() const { return m_rawDeltaRatio; }

        static MsgSampleRateCorrection* create(float correctionFactor, float rawDeltaRatio)
        {
            return new MsgSampleRateCorrection(correctionFactor, rawDeltaRatio);
        }

    private:
        float m_correctionFactor;
        float m_rawDeltaRatio;

        MsgSampleRateCorrection(float correctionFactor, float rawDeltaRatio) :
            Message(),
            m_correctionFactor(correctionFactor),
            m_rawDeltaRatio(rawDeltaRatio)
        { }
    };
};


#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEMSG_H_ */
