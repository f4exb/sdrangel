///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/ctcssfrequencies.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "nfmmodsettings.h"

// Standard channel spacings (kHz) using Carson rule
// beta based ............  11F3   16F3  (36F9)
//  5     6.25  7.5   8.33  12.5   25     40     Spacing
//  0.43  0.43  0.43  0.43  0.83   1.67   1.0    Beta
const int NFMModSettings::m_channelSpacings[] = {
    5000, 6250, 7500, 8333, 12500, 25000, 40000
};
const int NFMModSettings::m_rfBW[] = {  // RF bandwidth (Hz)
    4800, 6000, 7200, 8000, 11000, 16000, 36000
};
const int NFMModSettings::m_afBW[] = {  // audio bandwidth (Hz)
    1700, 2100, 2500, 2800,  3000,  3000,  9000
};
const int NFMModSettings::m_fmDev[] = { // peak deviation (Hz) - full is double
     731,  903, 1075, 1204,  2500,  5000,  9000
};
const int NFMModSettings::m_nbChannelSpacings = 7;


NFMModSettings::NFMModSettings() :
    m_channelMarker(nullptr),
    m_cwKeyerGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void NFMModSettings::resetToDefaults()
{
    m_afBandwidth = 3000;
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 16000.0f;
    m_fmDeviation = 10000.0f; //!< full deviation
    m_toneFrequency = 1000.0f;
    m_volumeFactor = 1.0f;
    m_channelMute = false;
    m_playLoop = false;
    m_ctcssOn = false;
    m_ctcssIndex = 0;
    m_dcsOn = false;
    m_dcsCode = 0023;
    m_dcsPositive = false;
    m_preEmphasisOn = true;
    m_bpfOn = true;
    m_rgbColor = QColor(255, 0, 0).rgb();
    m_title = "NFM Modulator";
    m_modAFInput = NFMModInputAF::NFMModInputNone;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_feedbackAudioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_feedbackVolumeFactor = 0.5f;
    m_feedbackAudioEnable = false;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray NFMModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeReal(3, m_afBandwidth);
    s.writeReal(4, m_fmDeviation);
    s.writeU32(5, m_rgbColor);
    s.writeReal(6, m_toneFrequency);
    s.writeReal(7, m_volumeFactor);

    if (m_cwKeyerGUI) {
        s.writeBlob(8, m_cwKeyerGUI->serialize());
    } else { // standalone operation with presets
        s.writeBlob(8, m_cwKeyerSettings.serialize());
    }

    s.writeBool(9, m_ctcssOn);
    s.writeS32(10, m_ctcssIndex);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeString(12, m_title);
    s.writeS32(13, (int) m_modAFInput);
    s.writeString(14, m_audioDeviceName);
    s.writeBool(15, m_useReverseAPI);
    s.writeString(16, m_reverseAPIAddress);
    s.writeU32(17, m_reverseAPIPort);
    s.writeU32(18, m_reverseAPIDeviceIndex);
    s.writeU32(19, m_reverseAPIChannelIndex);
    s.writeString(20, m_feedbackAudioDeviceName);
    s.writeReal(21, m_feedbackVolumeFactor);
    s.writeBool(22, m_feedbackAudioEnable);
    s.writeS32(23, m_streamIndex);
    s.writeBool(24, m_dcsOn);
    s.writeS32(25, m_dcsCode);
    s.writeBool(26, m_dcsPositive);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);
    s.writeBool(31, m_preEmphasisOn);
    s.writeBool(32, m_bpfOn);

    return s.final();
}

bool NFMModSettings::deserialize(const QByteArray& data)
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
        qint32 tmp;
        uint32_t utmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 12500.0);
        d.readReal(3, &m_afBandwidth, 1000.0);
        d.readReal(4, &m_fmDeviation, 10000.0);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_toneFrequency, 1000.0);
        d.readReal(7, &m_volumeFactor, 1.0);
        d.readBlob(8, &bytetmp);

        if (m_cwKeyerGUI) {
            m_cwKeyerGUI->deserialize(bytetmp);
        } else { // standalone operation with presets
            m_cwKeyerSettings.deserialize(bytetmp);
        }

        d.readBool(9, &m_ctcssOn, false);
        d.readS32(10, &m_ctcssIndex, 0);

        if (m_channelMarker)
        {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(12, &m_title, "NFM Modulator");

        d.readS32(13, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) NFMModInputAF::NFMModInputTone)) {
            m_modAFInput = NFMModInputNone;
        } else {
            m_modAFInput = (NFMModInputAF) tmp;
        }

        d.readString(14, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(15, &m_useReverseAPI, false);
        d.readString(16, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(17, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(18, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(19, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(20, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(21, &m_feedbackVolumeFactor, 1.0);
        d.readBool(22, &m_feedbackAudioEnable, false);
        d.readS32(23, &m_streamIndex, 0);
        d.readBool(24, &m_dcsOn, false);
        d.readS32(25, &tmp, 0023);
        m_dcsCode = tmp < 0 ? 0 : tmp > 511 ? 511 : tmp;
        d.readBool(26, &m_dcsPositive, false);

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);
        d.readBool(31, &m_preEmphasisOn, true);
        d.readBool(32, &m_bpfOn, true);

        return true;
    }
    else
    {
        qDebug() << "NFMModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

int NFMModSettings::getChannelSpacing(int index)
{
    if (index < 0) {
        return m_channelSpacings[0];
    } else if (index < m_nbChannelSpacings) {
        return m_channelSpacings[index];
    } else {
        return m_channelSpacings[m_nbChannelSpacings-1];
    }
}

int NFMModSettings::getChannelSpacingIndex(int channelSpacing)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (channelSpacing <= m_channelSpacings[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}

int NFMModSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbChannelSpacings) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbChannelSpacings-1];
    }
}

int NFMModSettings::getRFBWIndex(int rfbw)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (rfbw <= m_rfBW[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}

int NFMModSettings::getAFBW(int index)
{
    if (index < 0) {
        return m_afBW[0];
    } else if (index < m_nbChannelSpacings) {
        return m_afBW[index];
    } else {
        return m_afBW[m_nbChannelSpacings-1];
    }
}

int NFMModSettings::getAFBWIndex(int afbw)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (afbw <= m_afBW[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}

int NFMModSettings::getFMDev(int index)
{
    if (index < 0) {
        return m_fmDev[0];
    } else if (index < m_nbChannelSpacings) {
        return m_fmDev[index];
    } else {
        return m_fmDev[m_nbChannelSpacings-1];
    }
}

int NFMModSettings::getFMDevIndex(int fmDev)
{
    for (int i = 0; i < m_nbChannelSpacings; i++)
    {
        if (fmDev <= m_fmDev[i]) {
            return i;
        }
    }

    return m_nbChannelSpacings-1;
}

int NFMModSettings::getNbCTCSSFreq()
{
    return CTCSSFrequencies::m_nbFreqs;
}

float NFMModSettings::getCTCSSFreq(int index)
{
    if (index < CTCSSFrequencies::m_nbFreqs) {
        return CTCSSFrequencies::m_Freqs[index];
    } else {
        return CTCSSFrequencies::m_Freqs[0];
    }
}

int NFMModSettings::getCTCSSFreqIndex(float ctcssFreq)
{
    for (int i = 0; i < CTCSSFrequencies::m_nbFreqs; i++)
    {
        if (ctcssFreq <= CTCSSFrequencies::m_Freqs[i]) {
            return i;
        }
    }

    return CTCSSFrequencies::m_nbFreqs - 1;
}

