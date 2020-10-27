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

#ifndef PLUGINS_CHANNELRX_DEMOADSB_ADSBDEMODREPORT_H_
#define PLUGINS_CHANNELRX_DEMOADSB_ADSBDEMODREPORT_H_

#include <QObject>
#include <QByteArray>
#include <QDateTime>

#include "dsp/dsptypes.h"
#include "util/message.h"

class ADSBDemodReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportADSB : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QByteArray getData() const { return m_data; }
        QDateTime getDateTime() const { return m_dateTime; }
        float getPreambleCorrelation() const { return m_premableCorrelation; }

        static MsgReportADSB* create(QByteArray data, float premableCorrelation)
        {
            return new MsgReportADSB(data, premableCorrelation);
        }

    private:
        QByteArray m_data;
        QDateTime m_dateTime;
        float m_premableCorrelation;

        MsgReportADSB(QByteArray data, float premableCorrelation) :
            Message(),
            m_data(data),
            m_premableCorrelation(premableCorrelation)
        {
            m_dateTime = QDateTime::currentDateTime();
        }
    };

public:
    ADSBDemodReport() {}
    ~ADSBDemodReport() {}
};

#endif // PLUGINS_CHANNELRX_DEMOADSB_ADSBDEMODREPORT_H_
