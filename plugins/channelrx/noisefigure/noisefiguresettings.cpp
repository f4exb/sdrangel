///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

void NoiseFigureSettings::applySettings(const QStringList& settingsKeys, const NoiseFigureSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("fftSize")) {
        m_fftSize = settings.m_fftSize;
    }
    if (settingsKeys.contains("fftCount")) {
        m_fftCount = settings.m_fftCount;
    }
    if (settingsKeys.contains("sweepSpec")) {
        m_sweepSpec = settings.m_sweepSpec;
    }
    if (settingsKeys.contains("startValue")) {
        m_startValue = settings.m_startValue;
    }
    if (settingsKeys.contains("stopValue")) {
        m_stopValue = settings.m_stopValue;
    }
    if (settingsKeys.contains("steps")) {
        m_steps = settings.m_steps;
    }
    if (settingsKeys.contains("step")) {
        m_step = settings.m_step;
    }
    if (settingsKeys.contains("sweepList")) {
        m_sweepList = settings.m_sweepList;
    }
    if (settingsKeys.contains("visaDevice")) {
        m_visaDevice = settings.m_visaDevice;
    }
    if (settingsKeys.contains("powerOnSCPI")) {
        m_powerOnSCPI = settings.m_powerOnSCPI;
    }
    if (settingsKeys.contains("powerOffSCPI")) {
        m_powerOffSCPI = settings.m_powerOffSCPI;
    }
    if (settingsKeys.contains("powerOnCommand")) {
        m_powerOnCommand = settings.m_powerOnCommand;
    }
    if (settingsKeys.contains("powerOffCommand")) {
        m_powerOffCommand = settings.m_powerOffCommand;
    }
    if (settingsKeys.contains("powerDelay")) {
        m_powerDelay = settings.m_powerDelay;
    }
    if (settingsKeys.contains("interpolation")) {
        m_interpolation = settings.m_interpolation;
    }
    if (settingsKeys.contains("setting")) {
        m_setting = settings.m_setting;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString NoiseFigureSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("fftSize") || force) {
        ostr << " m_fftSize: " << m_fftSize;
    }
    if (settingsKeys.contains("fftCount") || force) {
        ostr << " m_fftCount: " << m_fftCount;
    }
    if (settingsKeys.contains("sweepSpec") || force) {
        ostr << " m_sweepSpec: " << m_sweepSpec;
    }
    if (settingsKeys.contains("startValue") || force) {
        ostr << " m_startValue: " << m_startValue;
    }
    if (settingsKeys.contains("stopValue") || force) {
        ostr << " m_stopValue: " << m_stopValue;
    }
    if (settingsKeys.contains("steps") || force) {
        ostr << " m_steps: " << m_steps;
    }
    if (settingsKeys.contains("step") || force) {
        ostr << " m_step: " << m_step;
    }
    if (settingsKeys.contains("sweepList") || force) {
        ostr << " m_sweepList: " << m_sweepList.toStdString();
    }
    if (settingsKeys.contains("visaDevice") || force) {
        ostr << " m_visaDevice: " << m_visaDevice.toStdString();
    }
    if (settingsKeys.contains("powerOnSCPI") || force) {
        ostr << " m_powerOnSCPI: " << m_powerOnSCPI.toStdString();
    }
    if (settingsKeys.contains("powerOffSCPI") || force) {
        ostr << " m_powerOffSCPI: " << m_powerOffSCPI.toStdString();
    }
    if (settingsKeys.contains("powerOnCommand") || force) {
        ostr << " m_powerOnCommand: " << m_powerOnCommand.toStdString();
    }
    if (settingsKeys.contains("powerOffCommand") || force) {
        ostr << " m_powerOffCommand: " << m_powerOffCommand.toStdString();
    }
    if (settingsKeys.contains("powerDelay") || force) {
        ostr << " m_powerDelay: " << m_powerDelay;
    }
    if (settingsKeys.contains("interpolation") || force) {
        ostr << " m_interpolation: " << m_interpolation;
    }
    if (settingsKeys.contains("setting") || force) {
        ostr << " m_setting: " << m_setting.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex") || force) {
        ostr << " m_reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
