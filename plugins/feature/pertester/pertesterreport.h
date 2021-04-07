///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_PERTESTERREPORT_H_
#define INCLUDE_FEATURE_PERTESTERREPORT_H_

#include <QObject>

#include "util/message.h"

class PERTesterReport : public QObject
{
    Q_OBJECT
public:

    class MsgReportStats : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getTx() const { return m_tx; }
        int getRxMatched() const { return m_rxMatched; }
        int getRxUnmatched() const { return m_rxUnmatched; }

        static MsgReportStats* create(int tx, int rxMatched, int rxUnmatched)
        {
            return new MsgReportStats(tx, rxMatched, rxUnmatched);
        }

    private:
        int m_tx;
        int m_rxMatched;
        int m_rxUnmatched;

        MsgReportStats(int tx, int rxMatched, int rxUnmatched) :
            Message(),
            m_tx(tx),
            m_rxMatched(rxMatched),
            m_rxUnmatched(rxUnmatched)
        {
        }
    };

public:
    PERTesterReport() {}
    ~PERTesterReport() {}
};

#endif // INCLUDE_FEATURE_PERTESTERREPORT_H_
