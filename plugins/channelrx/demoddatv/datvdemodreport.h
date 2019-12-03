///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_DATVDEMODREPORT_H
#define INCLUDE_DATVDEMODREPORT_H

#include "util/message.h"

#include "datvdemodsettings.h"

class DATVDemodReport
{
public:
    DATVDemodReport();
    ~DATVDemodReport();
    class MsgReportModcodCstlnChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        DATVDemodSettings::DATVModulation getModulation() const { return m_modulation; }
        DATVDemodSettings::DATVCodeRate getCodeRate() const { return m_codeRate; }

        static MsgReportModcodCstlnChange* create(const DATVDemodSettings::DATVModulation& modulation,
            const DATVDemodSettings::DATVCodeRate& codeRate)
        {
            return new MsgReportModcodCstlnChange(modulation, codeRate);
        }

    private:
        DATVDemodSettings::DATVModulation m_modulation;
        DATVDemodSettings::DATVCodeRate m_codeRate;

        MsgReportModcodCstlnChange(
            const DATVDemodSettings::DATVModulation& modulation,
            const DATVDemodSettings::DATVCodeRate& codeRate
        ) :
            Message(),
            m_modulation(modulation),
            m_codeRate(codeRate)
        { }
    };
};

#endif // INCLUDE_DATVDEMODREPORT_H
