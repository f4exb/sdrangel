///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>

#include "util/csv.h"

#include "vlftransmitters.h"

// https://sidstation.loudet.org/stations-list-en.xhtml
// https://core.ac.uk/download/pdf/224769021.pdf -- Table 1
// GQD/GQZ callsigns: https://groups.io/g/VLF/message/19212?p=%2C%2C%2C20%2C0%2C0%2C0%3A%3Arecentpostdate%2Fsticky%2C%2C19.6%2C20%2C2%2C0%2C38924431
QList<VLFTransmitters::Transmitter> VLFTransmitters::m_transmitters = {
    {QStringLiteral("JXN"),   16400, 66.974353, 13.873617, -1},     // Novik, Norway (Only transmits 6 times a day)
    {QStringLiteral("VTX2"),  17000, 8.387015,  77.752762, -1},     // South Vijayanarayanam, India
    {QStringLiteral("RDL"),   18100, 44.773333, 39.547222, -1},     // Krasnodar, Russia (Transmits short bursts, possibly FSK)
    {QStringLiteral("GQD"),   19580, 54.911643, -3.278456, 100},    // Anthorn, UK, Often referred to as GBZ
    {QStringLiteral("NWC"),   19800, -21.816325, 114.16546, 1000},  // Exmouth, Aus
    {QStringLiteral("ICV"),   20270, 40.922946, 9.731881,  50},     // Isola di Tavolara, Italy (Can be distorted on 3D map if terrain used)
    {QStringLiteral("FTA"),   20900, 48.544632, 2.579429,  50},     // Sainte-Assise, France (Satellite imagary obfuscated)
    {QStringLiteral("NPM"),   21400, 21.420166, -158.151140, 600},  // Pearl Harbour, Lualuahei, USA (Not seen?)
    {QStringLiteral("HWU"),   21750, 46.713129, 1.245248, 200},     // Rosnay, France
    {QStringLiteral("GQZ"),   22100, 54.731799, -2.883033, 100},    // Skelton, UK (GVT in paper)
    {QStringLiteral("DHO38"), 23400, 53.078900, 7.615000,  300},    // Rhauderfehn, Germany - Off air 7-8 UTC
    {QStringLiteral("NAA"),   24000, 44.644506, -67.284565, 1000},  // Cutler, Maine, USA
    {QStringLiteral("TBB"),   26700, 37.412725, 27.323342, -1},     // Bafa, Turkey
    {QStringLiteral("TFK/NRK"), 37500, 63.850365, -22.466773, 100}, // Grindavik, Iceland
    {QStringLiteral("SRC"),   40400, 57.120328, 16.153083, -1},     // Grimeton, Sweden
    {QStringLiteral("NSY"),   45900, 37.125660, 14.436416, -1},     // Niscemi, Italy
    {QStringLiteral("SXA"),   49000, 38.145155, 24.019718, -1},     // Marathon, Greece
    {QStringLiteral("GYW1"),  51950, 57.617463, -1.887589, -1},     // Crimond, UK
    {QStringLiteral("FUE"),   65800, 48.637673, -4.350758, -1},     // Kerlouan, France
};

QHash<QString, const VLFTransmitters::Transmitter*> VLFTransmitters::m_callsignHash;

VLFTransmitters::Init VLFTransmitters::m_init;

VLFTransmitters::Init::Init()
{
    // Get directory to store app data in
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    QString dir = locations[0];

    // Try reading transmitters from .csv file
    QString filename = QString("%1/%2/%3/vlftransmitters.csv").arg(dir).arg(COMPANY).arg(APPLICATION_NAME); // Because this method is called before main(), we need to add f4exb/SDRangel
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);

        QString error;
        QHash<QString, int> colIndexes = CSV::readHeader(in, {
            QStringLiteral("Callsign"),
            QStringLiteral("Frequency"),
            QStringLiteral("Latitude"),
            QStringLiteral("Longitude"),
            QStringLiteral("Power")
           }, error);
        if (error.isEmpty())
        {
            QStringList cols;
            int callsignCol = colIndexes.value(QStringLiteral("Callsign"));
            int frequencyCol = colIndexes.value(QStringLiteral("Frequency"));
            int latitudeCol = colIndexes.value(QStringLiteral("Latitude"));
            int longitudeCol = colIndexes.value(QStringLiteral("Longitude"));
            int powerCol = colIndexes.value(QStringLiteral("Power"));
            int maxCol = std::max({callsignCol, frequencyCol, latitudeCol, longitudeCol, powerCol});

            m_transmitters.clear(); // Replace builtin list

            while(CSV::readRow(in, &cols))
            {
                if (cols.size() > maxCol)
                {
                    Transmitter transmitter;

                    transmitter.m_callsign = cols[callsignCol];
                    transmitter.m_frequency = cols[frequencyCol].toLongLong();
                    transmitter.m_latitude = cols[latitudeCol].toFloat();
                    transmitter.m_longitude = cols[longitudeCol].toFloat();
                    transmitter.m_power = cols[powerCol].toInt();

                    m_transmitters.append(transmitter);
                }
            }
        }
        else
        {
            qWarning() << filename << "did not contain expected headers.";
        }
    }

    // Create hash table for faster searching
    for (const auto& transmitter : VLFTransmitters::m_transmitters) {
        VLFTransmitters::m_callsignHash.insert(transmitter.m_callsign, &transmitter);
    }
}
