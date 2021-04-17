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

#ifndef PLUGINS_CHANNELRX_DEMONFM_NFMDEMODREPORT_H_
#define PLUGINS_CHANNELRX_DEMONFM_NFMDEMODREPORT_H_

#include <QObject>

#include "dsp/dsptypes.h"
#include "util/message.h"

class NFMDemodReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportCTCSSFreq : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getFrequency() const { return m_freq; }

        static MsgReportCTCSSFreq* create(Real freq)
        {
            return new MsgReportCTCSSFreq(freq);
        }

    private:
        Real m_freq;

        MsgReportCTCSSFreq(Real freq) :
            Message(),
            m_freq(freq)
        { }
    };

    class MsgReportDCSCode : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        unsigned int getCode() const { return m_code; }

        static MsgReportDCSCode* create(unsigned int code)
        {
            return new MsgReportDCSCode(code);
        }

    private:
        unsigned int m_code;

        MsgReportDCSCode(unsigned int code) :
            Message(),
            m_code(code)
        { }
    };

public:
    NFMDemodReport();
    ~NFMDemodReport();
};

#endif // PLUGINS_CHANNELRX_DEMONFM_NFMDEMODREPORT_H_
