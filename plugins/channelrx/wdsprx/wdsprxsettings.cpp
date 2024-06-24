///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "wdsprxsettings.h"

#ifdef SDR_RX_SAMPLE_24BIT
const int WDSPRxSettings::m_minPowerThresholdDB = -120;
const float WDSPRxSettings::m_mminPowerThresholdDBf = 120.0f;
#else
const int WDSPRxSettings::m_minPowerThresholdDB = -100;
const float WDSPRxSettings::m_mminPowerThresholdDBf = 100.0f;
#endif

WDSPRxSettings::WDSPRxSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_rollupState(nullptr)
{
    m_filterBank.resize(10);
    resetToDefaults();
}

void WDSPRxSettings::resetToDefaults()
{
    m_audioBinaural = false;
    m_audioFlipChannels = false;
    m_dsb = false;
    m_audioMute = false;
    m_agc = true;
    m_agcMode = AGCMedium;
    m_agcGain = 80;
    m_agcSlope = 35; // 3.5 dB
    m_agcHangThreshold = 0;
    m_volume = 1.0;
    m_inputFrequencyOffset = 0;
    m_rgbColor = QColor(0, 255, 0).rgb();
    m_title = "WDSP Receiver";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_filterIndex = 0;
}

QByteArray WDSPRxSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(3, m_volume * 10.0);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);
    s.writeBool(8, m_audioBinaural);
    s.writeBool(9, m_audioFlipChannels);
    s.writeBool(10, m_dsb);
    s.writeBool(11, m_agc);
    s.writeS32(12, (int) m_agcMode);
    s.writeS32(13, m_agcGain);
    s.writeS32(14, m_agcSlope);
    s.writeS32(15, m_agcHangThreshold);
    s.writeString(16, m_title);
    s.writeString(17, m_audioDeviceName);
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIDeviceIndex);
    s.writeU32(22, m_reverseAPIChannelIndex);
    s.writeS32(23, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(24, m_rollupState->serialize());
    }

    s.writeS32(25, m_workspaceIndex);
    s.writeBlob(26, m_geometryBytes);
    s.writeBool(27, m_hidden);
    s.writeU32(29, m_filterIndex);

    for (unsigned int i = 0; i <  10; i++)
    {
        s.writeS32(100 + 10*i, m_filterBank[i].m_spanLog2);
        s.writeS32(101 + 10*i, m_filterBank[i].m_highCutoff / 100.0);
        s.writeS32(102 + 10*i, m_filterBank[i].m_lowCutoff / 100.0);
        s.writeS32(103 + 10*i, (int) m_filterBank[i].m_fftWindow);
        s.writeBool(104 + 10*i, m_filterBank[i].m_dnr);
        s.writeS32(105 + 10*i, m_filterBank[i].m_dnrScheme);
        s.writeFloat(106 + 10*i, m_filterBank[i].m_dnrAboveAvgFactor);
        s.writeFloat(107 + 10*i, m_filterBank[i].m_dnrSigmaFactor);
        s.writeS32(108 + 10*i, m_filterBank[i].m_dnrNbPeaks);
        s.writeFloat(109 + 10*i, m_filterBank[i].m_dnrAlpha);
    }

    return s.final();
}

bool WDSPRxSettings::deserialize(const QByteArray& data)
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
        QString strtmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(3, &tmp, 30);
        m_volume = tmp / 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);
        d.readBool(8, &m_audioBinaural, false);
        d.readBool(9, &m_audioFlipChannels, false);
        d.readBool(10, &m_dsb, false);
        d.readBool(11, &m_agc, true);
        d.readS32(12, &tmp, 2);
        m_agcMode = (AGCMode) tmp;
        d.readS32(13, &m_agcGain, 80);
        d.readS32(14, &m_agcSlope, 35);
        d.readS32(15, &m_agcHangThreshold, 0);
        d.readString(16, &m_title, "WDSP Receiver");
        d.readString(17, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(18, &m_useReverseAPI, false);
        d.readString(19, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(20, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(21, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(22, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(23, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(24, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(25, &m_workspaceIndex, 0);
        d.readBlob(26, &m_geometryBytes);
        d.readBool(27, &m_hidden, false);
        d.readU32(29, &utmp, 0);
        m_filterIndex = utmp < 10 ? utmp : 0;

        for (unsigned int i = 0; (i < 10); i++)
        {
            d.readS32(100 + 10*i, &m_filterBank[i].m_spanLog2, 3);
            d.readS32(101 + 10*i, &tmp, 30);
            m_filterBank[i].m_highCutoff = tmp * 100.0;
            d.readS32(102+ 10*i, &tmp, 3);
            m_filterBank[i].m_lowCutoff = tmp * 100.0;
            d.readS32(103 + 10*i, &tmp, (int) FFTWindow::Blackman);
            m_filterBank[i].m_fftWindow =
                (FFTWindow::Function) (tmp < 0 ? 0 : tmp > (int) FFTWindow::BlackmanHarris7 ? (int) FFTWindow::BlackmanHarris7 : tmp);
            d.readBool(104 + 10*i, &m_filterBank[i].m_dnr, false);
            d.readS32(105 + 10*i, &m_filterBank[i].m_dnrScheme, 0);
            d.readFloat(106 + 10*i, &m_filterBank[i].m_dnrAboveAvgFactor, 20.0f);
            d.readFloat(107 + 10*i, &m_filterBank[i].m_dnrSigmaFactor, 4.0f);
            d.readS32(108 + 10*i, &m_filterBank[i].m_dnrNbPeaks, 10);
            d.readFloat(109 + 10*i, &m_filterBank[i].m_dnrAlpha, 0.95f);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
