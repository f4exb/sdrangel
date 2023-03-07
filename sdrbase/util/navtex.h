///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_UTIL_NAVTEX_H
#define INCLUDE_UTIL_NAVTEX_H

#include <QString>
#include <QDateTime>
#include <QMap>

#include "export.h"

class SDRBASE_API NavtexTransmitter {

public:

    struct Schedule {
        char m_id;
        qint64 m_frequency;
        QList<QTime> m_times;
        Schedule(char id, qint64 frequency) :
            m_id(id),
            m_frequency(frequency)
        {
        }
        Schedule(char id, qint64 frequency, QList<QTime> times) :
            m_id(id),
            m_frequency(frequency),
            m_times(times)
        {
        }
    };

    int m_area;
    QString m_station;
    float m_latitude;
    float m_longitude;
    QList<Schedule> m_schedules;

    static const QList<NavtexTransmitter> m_navtexTransmitters;
    static const NavtexTransmitter* getTransmitter(QTime time, int area, qint64 frequency);
};

class SDRBASE_API NavtexMessage {

public:

    QString m_stationId;
    QString m_typeId;
    QString m_id;
    QString m_message;
    QDateTime m_dateTime;
    bool m_valid;

    static const QMap<QString,QString> m_types;

    NavtexMessage(const QString& text);
    NavtexMessage(QDateTime dataTime, const QString& stationId, const QString& typeId, const QString& id, const QString& message);
    QString getStation(int area, qint64 frequency) const;
    QString getType() const;

};

class SDRBASE_API SitorBDecoder {

public:

    void init();
    signed char decode(signed char c);
    int getErrors() const { return m_errors; }
    static QString printable(signed char c);

private:
    static const signed char PHASING_1 = 0x78;
    static const signed char PHASING_2 = 0x33;
    static const int BUFFER_SIZE = 3;
    signed char m_buf[3];
    bool m_figureSet;
    enum State {
        PHASING,
        FILL_DX,
        FILL_RX,
        DX,
        RX
    } m_state;
    int m_idx;
    int m_errors;

    static const signed char m_ccir476LetterSetDecode[128];
    static const signed char m_ccir476FigureSetDecode[128];

    signed char ccir476Decode(signed char c);

};

#endif // INCLUDE_UTIL_NAVTEX_H

