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

#ifndef INCLUDE_FEATURE_AFCREPORT_H_
#define INCLUDE_FEATURE_AFCREPORT_H_

#include "util/message.h"

class AFCReport
{
public:
    enum RadioState {
        RadioIdle,
        RadioRx,
        RadioTx
    };
    class MsgUpdateTarget : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFrequencyAdjustment() const { return m_frequencyAdjustment; }
        bool getFrequencyChanged() const { return m_frequencyChanged; }

        static MsgUpdateTarget* create(int frequencyAdjustment, bool frequencyChanged)
        {
            return new MsgUpdateTarget(frequencyAdjustment, frequencyChanged);
        }

    private:
        int m_frequencyAdjustment;
        bool m_frequencyChanged;

        MsgUpdateTarget(int frequencyAdjustment, bool frequencyChanged) :
            Message(),
            m_frequencyAdjustment(frequencyAdjustment),
            m_frequencyChanged(frequencyChanged)
        { }
    };

    AFCReport();
    ~AFCReport();
};

#endif // INCLUDE_FEATURE_AFCREPORT_H_
