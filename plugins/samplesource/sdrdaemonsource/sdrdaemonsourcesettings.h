///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_SDRDAEMONSOURCE_SDRDAEMONSOURCESETTINGS_H_
#define PLUGINS_SAMPLESOURCE_SDRDAEMONSOURCE_SDRDAEMONSOURCESETTINGS_H_

#include <QByteArray>
#include <QString>

struct SDRdaemonSourceSettings {
    quint64 m_centerFrequency;
    quint64 m_sampleRate;
    quint32 m_log2Decim;
    float   m_txDelay;
    quint32 m_nbFECBlocks;
    QString m_address;
    quint16 m_dataPort;
    quint16 m_controlPort;
    QString m_specificParameters;
    bool    m_dcBlock;
    bool    m_iqCorrection;
    quint32 m_fcPos;
    QString m_fileRecordName;

    SDRdaemonSourceSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* PLUGINS_SAMPLESOURCE_SDRDAEMONSOURCE_SDRDAEMONSOURCESETTINGS_H_ */
