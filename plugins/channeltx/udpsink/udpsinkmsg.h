///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSINKMSG_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSINKMSG_H_

#include "util/message.h"

/**
 * Message(s) used to communicate back from UDPSinkUDPHandler to UDPSink
 */
class UDPSinkMessages
{
public:
    class MsgSampleRateCorrection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getCorrectionFactor() const { return m_correctionFactor; }

        static MsgSampleRateCorrection* create(float correctionFactor)
        {
            return new MsgSampleRateCorrection(correctionFactor);
        }

    private:
        float m_correctionFactor;

        MsgSampleRateCorrection(float correctionFactor) :
            Message(),
            m_correctionFactor(correctionFactor)
        { }
    };
};


#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINKMSG_H_ */
