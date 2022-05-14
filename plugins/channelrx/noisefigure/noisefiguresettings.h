///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_NOISEFIGURESETTINGS_H
#define INCLUDE_NOISEFIGURESETTINGS_H

#include <QByteArray>
#include <QString>
#include <QList>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the table
#define NOISEFIGURE_COLUMNS 6

struct NoiseFigureSettings
{
    struct ENR
    {
        double m_frequency; //!< Frequency in MHz
        double m_enr;       //!< ENR in dB
        ENR() :
            m_frequency(0.0),
            m_enr(0.0)
        { }
        ENR(double frequency, double enr) :
            m_frequency(frequency),
            m_enr(enr)
        { }
    };

    qint32 m_inputFrequencyOffset;
    int m_fftSize;
    Real m_fftCount;        //!< Number of FFT bins to average

    enum SweepSpec {
        RANGE,
        STEP,
        LIST
    } m_sweepSpec;
    double m_startValue;
    double m_stopValue;
    int m_steps;
    double m_step;
    QString m_sweepList;

    QString m_visaDevice;
    QString m_powerOnSCPI;
    QString m_powerOffSCPI;
    QString m_powerOnCommand;
    QString m_powerOffCommand;
    double m_powerDelay;       //<! Delay in seconds before starting a measurement

    QList<ENR *> m_enr;
    enum Interpolation {
        LINEAR,
        BARYCENTRIC
    } m_interpolation;

    QString m_setting;    //<! Device setting to sweep

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_resultsColumnIndexes[NOISEFIGURE_COLUMNS];//!< How the columns are ordered in the table
    int m_resultsColumnSizes[NOISEFIGURE_COLUMNS];  //!< Size of the columns in the table

    NoiseFigureSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    QByteArray serializeENRs(QList<ENR *> enrs) const;
    void deserializeENRs(const QByteArray& data, QList<ENR *>& enrs);
};

#endif /* INCLUDE_NOISEFIGURESETTINGS_H */
