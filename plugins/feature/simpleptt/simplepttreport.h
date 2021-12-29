///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_SIMPLEPTTREPORT_H_
#define INCLUDE_FEATURE_SIMPLEPTTREPORT_H_

#include "util/message.h"

class SimplePTTReport
{
public:
    enum RadioState {
        RadioIdle,
        RadioRx,
        RadioTx
    };
    class MsgRadioState : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        RadioState getState() const { return m_state; }

        static MsgRadioState* create(RadioState state) {
            return new MsgRadioState(state);
        }

    private:
        RadioState m_state;

        MsgRadioState(RadioState state) :
            Message(),
            m_state(state)
        { }
    };

    class MsgVox : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getVox() const { return m_vox; }

        static MsgVox* create(bool vox) {
            return new MsgVox(vox);
        }

    private:
        bool m_vox;

        MsgVox(bool vox) :
            Message(),
            m_vox(vox)
        {}
    };

    SimplePTTReport();
    ~SimplePTTReport();
};

#endif // INCLUDE_FEATURE_SIMPLEPTTREPORT_H_
