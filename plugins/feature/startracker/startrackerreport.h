///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_FEATURE_STARTRACKERREPORT_H_
#define INCLUDE_FEATURE_STARTRACKERREPORT_H_

#include <QObject>
#include <QList>
#include <QVector3D>

#include "util/message.h"

class StarTrackerReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportAzAl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getAzimuth() const { return m_azimuth; }
        double getElevation() const { return m_elevation; }

        static MsgReportAzAl* create(double azimuth, double elevation)
        {
            return new MsgReportAzAl(azimuth, elevation);
        }

    private:
        double m_azimuth;
        double m_elevation;

        MsgReportAzAl(double azimuth, double elevation) :
            Message(),
            m_azimuth(azimuth),
            m_elevation(elevation)
        {
        }
    };

    class MsgReportRADec : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getRA() const { return m_ra; }
        double getDec() const { return m_dec; }
        QString getTarget() const { return m_target; }

        static MsgReportRADec* create(double ra, double dec, const QString& target)
        {
            return new MsgReportRADec(ra, dec, target);
        }

    private:
        double m_ra;
        double m_dec;
        QString m_target; // "target", "sun" or "moon"

        MsgReportRADec(double ra, double dec, const QString& target) :
            Message(),
            m_ra(ra),
            m_dec(dec),
            m_target(target)
        {
        }
    };

    class MsgReportGalactic : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getL() const { return m_l; }
        double getB() const { return m_b; }

        static MsgReportGalactic* create(double l, double b)
        {
            return new MsgReportGalactic(l, b);
        }

    private:
        double m_l;
        double m_b;

        MsgReportGalactic(double l, double b) :
            Message(),
            m_l(l),
            m_b(b)
        {
        }
    };

    class MsgReportAzElVsTime : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getTarget() const { return m_target; }
        const QList<double> &getAzimuths() const { return m_azimuths; }
        const QList<double> &getElevations() const { return m_elevations; }
        const QList<QDateTime> &getDateTimes() const { return m_dateTimes; }

        static MsgReportAzElVsTime* create(const QString&target, const QList<double> &azimuths, const QList<double> &elevations, const QList<QDateTime> &dateTimes)
        {
            return new MsgReportAzElVsTime(target, azimuths, elevations, dateTimes);
        }

    private:
        QString m_target;
        QList<double> m_azimuths;
        QList<double> m_elevations;
        QList<QDateTime> m_dateTimes;

        MsgReportAzElVsTime(const QString&target, const QList<double> &azimuths, const QList<double> &elevations, const QList<QDateTime> &dateTimes) :
            Message(),
            m_target(target),
            m_azimuths(azimuths),
            m_elevations(elevations),
            m_dateTimes(dateTimes)
        {
        }

    };

    class MsgReportSolarSystemPositions : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        QDateTime getDateTime() const { return m_dateTime; }
        const QStringList &getNames() const { return m_names; }
        const QList<QVector3D> &getPositions() const { return m_positions; }
        const QList<QList<QPointF>> &getOrbit() const { return m_orbit; }

        static MsgReportSolarSystemPositions* create(const QDateTime& dateTime, const QStringList &names, const QList<QVector3D> &positions, QList<QList<QPointF>> &orbit)
        {
            return new MsgReportSolarSystemPositions(dateTime, names, positions, orbit);
        }

    private:
        QDateTime m_dateTime;
        QStringList m_names;
        QList<QVector3D> m_positions;
        QList<QList<QPointF>> m_orbit;

        MsgReportSolarSystemPositions(const QDateTime& dateTime, const QStringList &names, const QList<QVector3D> &positions, const QList<QList<QPointF>> &orbit) :
            Message(),
            m_dateTime(dateTime),
            m_names(names),
            m_positions(positions),
            m_orbit(orbit)
        {
        }

    };

    struct JupiterData {
        QDateTime m_dateTime;
        double m_azimuth;
        double m_elevation;
    };

    struct JupiterMoonData {
        double m_cml;
        double m_phase;
    };


    class MsgReportJupiter : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        QDateTime getDateTime() const { return m_dateTime; }
        double getAzimuth() const { return m_azimuth; }
        double getElevation() const { return m_elevation; }
        double getCML() const { return m_cml; }
        double getIoPhase() const { return m_ioPhase; }
        double getGanymedePhase() const { return m_ganymedePhase; }

        static MsgReportJupiter* create(const QDateTime& dateTime, double azimuth, double elevation, double cml, double ioPhase, double ganymedePhase)
        {
            return new MsgReportJupiter(dateTime, azimuth, elevation, cml, ioPhase, ganymedePhase);
        }

    private:
        QDateTime m_dateTime;
        double m_azimuth;
        double m_elevation;
        double m_cml;
        double m_ioPhase;
        double m_ganymedePhase;

        MsgReportJupiter(const QDateTime& dateTime, double azimuth, double elevation, double cml, double ioPhase, double ganymedePhase) :
            Message(),
            m_dateTime(dateTime),
            m_azimuth(azimuth),
            m_elevation(elevation),
            m_cml(cml),
            m_ioPhase(ioPhase),
            m_ganymedePhase(ganymedePhase)
        {
        }

    };

    class MsgReportJupiterData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        QList<JupiterData> getJupiterData() const { return m_jupiterData; }
        QList<JupiterMoonData> getMoonData() const { return m_moonData; }

        static MsgReportJupiterData* create(const QList<JupiterData>& jupiterData, const QList<JupiterMoonData>& moonData)
        {
            return new MsgReportJupiterData(jupiterData, moonData);
        }

    private:
        QList<JupiterData> m_jupiterData;
        QList<JupiterMoonData> m_moonData;

        MsgReportJupiterData(const QList<JupiterData>& jupiterData, const QList<JupiterMoonData>& moonData) :
            Message(),
            m_jupiterData(jupiterData),
            m_moonData(moonData)
        {
        }

    };

public:
    StarTrackerReport() {}
    ~StarTrackerReport() {}
};

#endif // INCLUDE_FEATURE_STARTRACKERREPORT_H_
