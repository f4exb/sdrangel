///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "sigmffilesinksettings.h"

SigMFFileSinkSettings::SigMFFileSinkSettings()
{
    resetToDefaults();
}

void SigMFFileSinkSettings::resetToDefaults()
{
    m_ncoMode = false;
    m_inputFrequencyOffset = 0;
    m_fileRecordName = "";
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "SigMF File Sink";
    m_log2Decim = 0;
    m_channelMarker = nullptr;
    m_spectrumGUI = nullptr;
    m_rollupState = nullptr;
    m_spectrumSquelchMode = false;
    m_spectrumSquelch = -50;
    m_preRecordTime = 0;
    m_squelchPostRecordTime = 0;
    m_squelchRecordingEnable = false;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray SigMFFileSinkSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeBool(2, m_ncoMode);
    s.writeString(3, m_fileRecordName);
    s.writeS32(4, m_streamIndex);
    s.writeU32(5, m_rgbColor);
    s.writeString(6, m_title);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIDeviceIndex);
    s.writeU32(11, m_reverseAPIChannelIndex);
    s.writeU32(12, m_log2Decim);

    if (m_spectrumGUI) {
        s.writeBlob(13, m_spectrumGUI->serialize());
    }

    s.writeBool(14, m_spectrumSquelchMode);
    s.writeS32(15, m_spectrumSquelch);
    s.writeS32(16, m_preRecordTime);
    s.writeS32(17, m_squelchPostRecordTime);
    s.writeBool(18, m_squelchRecordingEnable);

    if (m_rollupState) {
        s.writeBlob(19, m_rollupState->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(20, m_channelMarker->serialize());
    }

    s.writeS32(21, m_workspaceIndex);
    s.writeBlob(22, m_geometryBytes);
    s.writeBool(23, m_hidden);

    return s.final();
}

bool SigMFFileSinkSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t tmp;
        int stmp;
        QString strtmp;
        QByteArray bytetmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readBool(2, &m_ncoMode, false);
        d.readString(3, &m_fileRecordName, "");
        d.readS32(4, &m_streamIndex, 0);
        d.readU32(5, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readString(6, &m_title, "SigMF File Sink");
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_reverseAPIPort = tmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &tmp, 0);
        m_reverseAPIDeviceIndex = tmp > 99 ? 99 : tmp;
        d.readU32(11, &tmp, 0);
        m_reverseAPIChannelIndex = tmp > 99 ? 99 : tmp;
        d.readU32(12, &tmp, 0);
        m_log2Decim = tmp > 6 ? 6 : tmp;

        if (m_spectrumGUI)
        {
            d.readBlob(13, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readBool(14, &m_spectrumSquelchMode, false);
        d.readS32(15, &stmp, -50);
        m_spectrumSquelch = stmp;
        d.readS32(16, &m_preRecordTime, 0);
        d.readS32(17, &m_squelchPostRecordTime, 0);
        d.readBool(18, &m_squelchRecordingEnable, false);

        if (m_rollupState)
        {
            d.readBlob(19, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(20, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(21, &m_workspaceIndex, 0);
        d.readBlob(22, &m_geometryBytes);
        d.readBool(23, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

unsigned int SigMFFileSinkSettings::getNbFixedShiftIndexes(int log2Decim)
{
    int decim = (1<<log2Decim);
    return 2*decim - 1;
}

int SigMFFileSinkSettings::getHalfBand(int sampleRate, int log2Decim)
{
    int decim = (1<<log2Decim);
    return sampleRate / (2*decim);
}

unsigned int SigMFFileSinkSettings::getFixedShiftIndexFromOffset(int sampleRate, int log2Decim, int frequencyOffset)
{
    if (sampleRate == 0) {
        return 0;
    }

    int decim = (1<<log2Decim);
    int mid = decim - 1;
    return ((frequencyOffset*2*decim) / sampleRate) + mid;
}

int SigMFFileSinkSettings::getOffsetFromFixedShiftIndex(int sampleRate, int log2Decim, int shiftIndex)
{
    int decim = (1<<log2Decim);
    int mid = decim - 1;
    return ((shiftIndex - mid) * sampleRate) / (2*decim);
}


