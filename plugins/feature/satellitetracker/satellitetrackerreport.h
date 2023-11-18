///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_FEATURE_SATELLITETRACKERREPORT_H_
#define INCLUDE_FEATURE_SATELLITETRACKERREPORT_H_

#include <QObject>

#include "util/message.h"
#include "satellitetrackersgp4.h"

class SatelliteTrackerReport : public QObject
{
    Q_OBJECT
public:

    // Sent from worker to GUI to give latest satellite data
    class MsgReportSat : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        SatelliteState* getSatelliteState() const { return m_satState; }

        static MsgReportSat* create(SatelliteState* satState)
        {
            return new MsgReportSat(satState);
        }

    private:
        SatelliteState* m_satState;

        MsgReportSat(SatelliteState* satState) :
            Message(),
            m_satState(satState)
        {
        }
    };

    // Sent from worker to GUI to indicate AOS
    class MsgReportAOS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getName() const { return m_name; }
        QString getSpeech() const { return m_speech; }

        static MsgReportAOS* create(const QString& name, const QString &speech)
        {
            return new MsgReportAOS(name, speech);
        }

    private:
        QString m_name;
        QString m_speech;

        MsgReportAOS(const QString& name, const QString &speech) :
            Message(),
            m_name(name),
            m_speech(speech)
        {
        }
    };

    // Sent from worker to GUI to indicaite LOS
    class MsgReportLOS : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getName() const { return m_name; }
        QString getSpeech() const { return m_speech; }

        static MsgReportLOS* create(const QString& name, const QString &speech)
        {
            return new MsgReportLOS(name, speech);
        }

    private:
        QString m_name;
        QString m_speech;

        MsgReportLOS(const QString& name, const QString &speech) :
            Message(),
            m_name(name),
            m_speech(speech)
        {
        }
    };

    // Sent from worker to GUI, to indicate target has changed
    class MsgReportTarget : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getName() const { return m_name; }

        static MsgReportTarget* create(const QString& name)
        {
            return new MsgReportTarget(name);
        }

    private:
        QString m_name;

        MsgReportTarget(const QString& name) :
            Message(),
            m_name(name)
        {
        }
    };

public:
    SatelliteTrackerReport() {}
    ~SatelliteTrackerReport() {}
};

#endif // INCLUDE_FEATURE_SATELLITETRACKERREPORT_H_
