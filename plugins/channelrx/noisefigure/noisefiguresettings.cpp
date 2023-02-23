///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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

#include <QColor>
#include <QDataStream>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "noisefiguresettings.h"

NoiseFigureSettings::NoiseFigureSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

#define DEFAULT_FREQUENCIES "430 435 440"
#define DEFAULT_VISA_DEVICE "USB0::0x1AB1::0x0E11::DP8C155102576::0::INSTR"
#define DEFAULT_POWER_ON "#:SOURCE1:VOLTage 28\n:OUTPut:STATe CH1,ON"
#define DEFAULT_POWER_OFF ":OUTPut:STATe CH1,OFF"

void NoiseFigureSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_fftSize = 64;
    m_fftCount = 20000.0f;
    m_sweepSpec = RANGE;
    m_startValue = 430.0;
    m_stopValue = 440.0;
    m_steps = 3;
    m_step = 5.0f;
    m_sweepList = DEFAULT_FREQUENCIES;
    m_visaDevice = DEFAULT_VISA_DEVICE;
    m_powerOnSCPI = DEFAULT_POWER_ON;
    m_powerOffSCPI = DEFAULT_POWER_OFF;
    m_powerOnCommand = "";
    m_powerOffCommand = "";
    m_powerDelay = 0.5;
    qDeleteAll(m_enr);
    m_enr << new ENR(1000.0, 15.0);
    m_interpolation = LINEAR;
    m_setting = "centerFrequency";
    m_rgbColor = QColor(0, 100, 200).rgb();
    m_title = "Noise Figure";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < NOISEFIGURE_COLUMNS; i++)
    {
        m_resultsColumnIndexes[i] = i;
        m_resultsColumnSizes[i] = -1; // Autosize
    }
}

QByteArray NoiseFigureSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_fftSize);
    s.writeFloat(3, m_fftCount);

    s.writeS32(4, (int)m_sweepSpec);
    s.writeDouble(5, m_startValue);
    s.writeDouble(6, m_stopValue);
    s.writeS32(7, m_steps);
    s.writeDouble(8, m_step);
    s.writeString(9, m_sweepList);

    s.writeString(10, m_visaDevice);
    s.writeString(11, m_powerOnSCPI);
    s.writeString(12, m_powerOffSCPI);
    s.writeString(13, m_powerOnCommand);
    s.writeString(14, m_powerOffCommand);
    s.writeDouble(15, m_powerDelay);

    s.writeBlob(16, serializeENRs(m_enr));

    s.writeU32(17, m_rgbColor);
    s.writeString(18, m_title);

    if (m_channelMarker) {
        s.writeBlob(19, m_channelMarker->serialize());
    }

    s.writeS32(20, m_streamIndex);
    s.writeBool(21, m_useReverseAPI);
    s.writeString(22, m_reverseAPIAddress);
    s.writeU32(23, m_reverseAPIPort);
    s.writeU32(24, m_reverseAPIDeviceIndex);
    s.writeU32(25, m_reverseAPIChannelIndex);
    s.writeS32(26, (int)m_interpolation);
    s.writeString(27, m_setting);

    if (m_rollupState) {
        s.writeBlob(28, m_rollupState->serialize());
    }

    s.writeS32(29, m_workspaceIndex);
    s.writeBlob(30, m_geometryBytes);
    s.writeBool(31, m_hidden);

    for (int i = 0; i < NOISEFIGURE_COLUMNS; i++) {
        s.writeS32(100 + i, m_resultsColumnIndexes[i]);
    }

    for (int i = 0; i < NOISEFIGURE_COLUMNS; i++) {
        s.writeS32(200 + i, m_resultsColumnSizes[i]);
    }

    return s.final();
}

bool NoiseFigureSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;
        QByteArray blob;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_fftSize, 64);
        d.readFloat(3, &m_fftCount, 10000.0f);
        d.readS32(4, (int*)&m_sweepSpec, NoiseFigureSettings::RANGE);
        d.readDouble(5, &m_startValue, 430.0);
        d.readDouble(6, &m_stopValue, 440.0);
        d.readS32(7, &m_steps, 3);
        d.readDouble(8, &m_step, 5.0);
        d.readString(9, &m_sweepList, DEFAULT_FREQUENCIES);
        d.readString(10, &m_visaDevice, DEFAULT_VISA_DEVICE);
        d.readString(11, &m_powerOnSCPI, DEFAULT_POWER_ON);
        d.readString(12, &m_powerOffSCPI, DEFAULT_POWER_OFF);
        d.readString(13, &m_powerOnCommand, "");
        d.readString(14, &m_powerOffCommand, "");
        d.readDouble(15, &m_powerDelay, 0.5);

        d.readBlob(16, &blob);
        deserializeENRs(blob, m_enr);

        d.readU32(17, &m_rgbColor, QColor(0, 100, 200).rgb());
        d.readString(18, &m_title, "Noise Figure");

        if (m_channelMarker)
        {
            d.readBlob(19, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(20, &m_streamIndex, 0);
        d.readBool(21, &m_useReverseAPI, false);
        d.readString(22, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(23, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(24, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(25, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(26, (int*)&m_interpolation, LINEAR);
        d.readString(27, &m_setting, "centerFrequency");

        if (m_rollupState)
        {
            d.readBlob(28, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(29, &m_workspaceIndex, 0);
        d.readBlob(30, &m_geometryBytes);
        d.readBool(31, &m_hidden, false);

        for (int i = 0; i < NOISEFIGURE_COLUMNS; i++) {
            d.readS32(100 + i, &m_resultsColumnIndexes[i], i);
        }

        for (int i = 0; i < NOISEFIGURE_COLUMNS; i++) {
            d.readS32(200 + i, &m_resultsColumnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QDataStream& operator<<(QDataStream& out, const NoiseFigureSettings::ENR* enr)
{
    out << enr->m_frequency;
    out << enr->m_enr;
    return out;
}

QDataStream& operator>>(QDataStream& in, NoiseFigureSettings::ENR*& enr)
{
    enr = new NoiseFigureSettings::ENR();
    in >> enr->m_frequency;
    in >> enr->m_enr;
    return in;
}

QByteArray NoiseFigureSettings::serializeENRs(QList<ENR *> enrs) const
{
    QByteArray data;
    QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
    (*stream) << enrs;
    delete stream;
    return data;
}

void NoiseFigureSettings::deserializeENRs(const QByteArray& data, QList<ENR *>& enrs)
{
    QDataStream *stream = new QDataStream(data);
    (*stream) >> enrs;
    delete stream;
}
