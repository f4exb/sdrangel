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
    m_profiles.resize(10);
    resetToDefaults();
}

void WDSPRxSettings::resetToDefaults()
{
    m_demod = WDSPRxProfile::DemodSSB;
    m_audioBinaural = false;
    m_audioFlipChannels = false;
    m_dsb = false;
    m_audioMute = false;
    // AGC
    m_agc = false;
    m_agcMode = WDSPRxProfile::AGCMedium;
    m_agcGain = 80;
    m_agcSlope = 35; // 3.5 dB
    m_agcHangThreshold = 0;
    // Noise blanker
    m_dnb = false;
    m_nbScheme = WDSPRxProfile::WDSPRxNBScheme::NBSchemeNB;
    m_nb2Mode = WDSPRxProfile::WDSPRxNB2Mode::NB2ModeZero;
    m_nbSlewTime = 0.1;
    m_nbLeadTime = 0.1;
    m_nbLagTime = 0.1;
    m_nbThreshold = 30;
    m_nbAvgTime = 50.0;
    // Noise reduction
    m_dnr = false;
    m_snb = false;
    m_anf = false;
    m_nrScheme = WDSPRxProfile::NRSchemeNR;
    m_nr2Gain = WDSPRxProfile::NR2GainGamma;
    m_nr2NPE = WDSPRxProfile::NR2NPEOSMS;
    m_nrPosition = WDSPRxProfile::NRPositionPreAGC;
    m_nr2ArtifactReduction = true;
    // Demods
    m_amFadeLevel = false;
    m_cwPeaking = false;
    m_cwPeakFrequency = 600.0;
    m_cwBandwidth = 100.0;
    m_cwGain = 2.0;
    //
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
    m_profileIndex = 0;
}

QByteArray WDSPRxSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(    1, m_inputFrequencyOffset);
    s.writeS32(    2, (int) m_demod);
    s.writeS32(    3, m_volume * 10.0);

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(    5, m_rgbColor);
    s.writeBool(   8, m_audioBinaural);
    s.writeBool(   9, m_audioFlipChannels);
    s.writeBool(  10, m_dsb);
    // AGC
    s.writeBool(  11, m_agc);
    s.writeS32(   12, (int) m_agcMode);
    s.writeS32(   13, m_agcGain);
    s.writeS32(   14, m_agcSlope);
    s.writeS32(   15, m_agcHangThreshold);
    // Noise blanker
    s.writeBool(  20, m_dnb);
    s.writeS32(   21, (int) m_nbScheme);
    s.writeS32(   22, (int) m_nb2Mode);
    s.writeDouble(23, m_nbSlewTime);
    s.writeDouble(24, m_nbLeadTime);
    s.writeDouble(25, m_nbLagTime);
    s.writeS32(   26, m_nbThreshold);
    s.writeDouble(27, m_nbAvgTime);
    // Noise reduction
    s.writeBool(  30, m_dnr);
    s.writeBool(  31, m_snb);
    s.writeBool(  32, m_anf);
    s.writeS32(   33, (int) m_nrScheme);
    s.writeS32(   34, (int) m_nr2Gain);
    s.writeS32(   35, (int) m_nr2NPE);
    s.writeS32(   36, (int) m_nrPosition);
    s.writeBool(  37, m_nr2ArtifactReduction);
    // Demods
    s.writeBool(  40, m_amFadeLevel);
    s.writeBool(  41, m_cwPeaking);
    s.writeDouble(42, m_cwPeakFrequency);
    s.writeDouble(43, m_cwBandwidth);
    s.writeDouble(44, m_cwGain);
    //
    s.writeString(70, m_title);
    s.writeString(71, m_audioDeviceName);
    s.writeBool(  72, m_useReverseAPI);
    s.writeString(73, m_reverseAPIAddress);
    s.writeU32(   74, m_reverseAPIPort);
    s.writeU32(   75, m_reverseAPIDeviceIndex);
    s.writeU32(   76, m_reverseAPIChannelIndex);
    s.writeS32(   77, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(78, m_rollupState->serialize());
    }

    s.writeS32(   79, m_workspaceIndex);
    s.writeBlob(  80, m_geometryBytes);
    s.writeBool(  81, m_hidden);
    s.writeU32(   82, m_profileIndex);

    for (unsigned int i = 0; i < 10; i++)
    {
        s.writeS32   (104 + 50*i, (int) m_profiles[i].m_demod);
        // Filter
        s.writeS32   (100 + 50*i, m_profiles[i].m_spanLog2);
        s.writeS32   (101 + 50*i, m_profiles[i].m_highCutoff / 100.0);
        s.writeS32   (102 + 50*i, m_profiles[i].m_lowCutoff / 100.0);
        s.writeS32   (103 + 50*i, m_profiles[i].m_fftWindow);
        // AGC
        s.writeBool  (110 + 50*i, m_profiles[i].m_agc);
        s.writeS32   (111 + 50*i, (int) m_profiles[i].m_agcMode);
        s.writeS32   (112 + 50*i, m_profiles[i].m_agcGain);
        s.writeS32   (113 + 50*i, m_profiles[i].m_agcSlope);
        s.writeS32   (114 + 50*i, m_profiles[i].m_agcHangThreshold);
        // Noise blanjer
        s.writeBool  (120 + 50*i, m_profiles[i].m_dnb);
        s.writeS32   (121 + 50*i, (int) m_profiles[i].m_nbScheme);
        s.writeS32   (122 + 50*i, (int) m_profiles[i].m_nb2Mode);
        s.writeDouble(123 + 50*i, m_profiles[i].m_nbSlewTime);
        s.writeDouble(124 + 50*i, m_profiles[i].m_nbLeadTime);
        s.writeDouble(125 + 50*i, m_profiles[i].m_nbLagTime);
        s.writeS32   (126 + 50*i, m_profiles[i].m_nbThreshold);
        s.writeDouble(127 + 50*i, m_profiles[i].m_nbAvgTime);
        // Noise reduction
        s.writeBool  (130 + 50*i, m_profiles[i].m_dnr);
        s.writeBool  (131 + 50*i, m_profiles[i].m_snb);
        s.writeBool  (132 + 50*i, m_profiles[i].m_anf);
        s.writeS32   (133 + 50*i, (int) m_profiles[i].m_nrScheme);
        s.writeS32   (134 + 50*i, (int) m_profiles[i].m_nr2Gain);
        s.writeS32   (135 + 50*i, (int) m_profiles[i].m_nr2NPE);
        s.writeS32   (136 + 50*i, (int) m_profiles[i].m_nrPosition);
        s.writeBool  (137 + 50*i, m_profiles[i].m_nr2ArtifactReduction);
        // Demods
        s.writeBool  (140 + 50*i, m_profiles[i].m_amFadeLevel);
        s.writeBool  (141 + 50*i, m_profiles[i].m_cwPeaking);
        s.writeDouble(142 + 50*i, m_profiles[i].m_cwPeakFrequency);
        s.writeDouble(143 + 50*i, m_profiles[i].m_cwBandwidth);
        s.writeDouble(144 + 50*i, m_profiles[i].m_cwGain);
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

        d.readS32(    1, &m_inputFrequencyOffset, 0);
        d.readS32(    2, &tmp, 0);
        m_demod = (WDSPRxProfile::WDSPRxDemod) tmp;
        d.readS32(    3, &tmp, 30);
        m_volume = tmp / 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(    5, &m_rgbColor);
        d.readBool(   8, &m_audioBinaural, false);
        d.readBool(   9, &m_audioFlipChannels, false);
        d.readBool(  10, &m_dsb, false);
        // AGC
        d.readBool(  11, &m_agc, true);
        d.readS32(   12, &tmp, 2);
        m_agcMode = (WDSPRxProfile::WDSPRxAGCMode) tmp;
        d.readS32(   13, &m_agcGain, 80);
        d.readS32(   14, &m_agcSlope, 35);
        d.readS32(   15, &m_agcHangThreshold, 0);
        // Noise blanker
        d.readBool(  20, &m_dnb, false);
        d.readS32(   21, &tmp, 2);
        m_nbScheme = (WDSPRxProfile::WDSPRxNBScheme) tmp;
        d.readS32(   22, &tmp, 2);
        m_nb2Mode = (WDSPRxProfile::WDSPRxNB2Mode) tmp;
        d.readDouble(23, &m_nbSlewTime, 0.1);
        d.readDouble(24, &m_nbLeadTime, 0.1);
        d.readDouble(25, &m_nbLagTime, 0.1);
        d.readS32(   26, &m_nbThreshold, 30);
        d.readDouble(27, &m_nbAvgTime, 50.0);
        // Nosie reduction
        d.readBool(  30, &m_dnr, false);
        d.readBool(  31, &m_snb, false);
        d.readBool(  32, &m_anf, false);
        d.readS32(   33, &tmp, 2);
        m_nrScheme = (WDSPRxProfile::WDSPRxNRScheme) tmp;
        d.readS32(   34, &tmp, 2);
        m_nr2Gain = (WDSPRxProfile::WDSPRxNR2Gain) tmp;
        d.readS32(   35, &tmp, 2);
        m_nr2NPE = (WDSPRxProfile::WDSPRxNR2NPE) tmp;
        d.readS32(   36, &tmp, 2);
        m_nrPosition = (WDSPRxProfile::WDSPRxNRPosition) tmp;
        d.readBool(  37, &m_nr2ArtifactReduction, true);
        // Demods
        d.readBool(  40, &m_amFadeLevel, false);
        d.readBool(  41, &m_cwPeaking, false);
        d.readDouble(42, &m_cwPeakFrequency, 600.0);
        //
        d.readString(70, &m_title, "WDSP Receiver");
        d.readString(71, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(  72, &m_useReverseAPI, false);
        d.readString(73, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(   74, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(   75, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(   76, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(   77, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(78, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(    79, &m_workspaceIndex, 0);
        d.readBlob(   80, &m_geometryBytes);
        d.readBool(   81, &m_hidden, false);
        d.readU32(    82, &utmp, 0);
        m_profileIndex = utmp < 10 ? utmp : 0;

        for (unsigned int i = 0; (i < 10); i++)
        {
            d.readS32   (104 + 50*i, &tmp, 9);
            m_profiles[i].m_demod = (WDSPRxProfile::WDSPRxDemod) tmp;
            // Filter
            d.readS32   (100 + 50*i, &m_profiles[i].m_spanLog2, 3);
            d.readS32   (101 + 50*i, &tmp, 30);
            m_profiles[i].m_highCutoff = tmp * 100.0;
            d.readS32   (102 + 50*i, &tmp, 3);
            m_profiles[i].m_lowCutoff = tmp * 100.0;
            d.readS32   (103 + 50*i, &m_profiles[i].m_fftWindow, 0);
            // AGC
            d.readBool(  110, &m_profiles[i].m_agc, true);
            d.readS32(   111, &tmp, 2);
            m_profiles[i].m_agcMode = (WDSPRxProfile::WDSPRxAGCMode) tmp;
            d.readS32(   112, &m_profiles[i].m_agcGain, 80);
            d.readS32(   113, &m_profiles[i].m_agcSlope, 35);
            d.readS32(   114, &m_profiles[i].m_agcHangThreshold, 0);
            // Noise blanker
            d.readBool  (120 + 50*i, &m_profiles[i].m_dnb, false);
            d.readS32   (121 + 50*i, &tmp);
            m_profiles[i].m_nbScheme = (WDSPRxProfile::WDSPRxNBScheme) tmp;
            d.readS32   (122 + 50*i, &tmp);
            m_profiles[i].m_nb2Mode = (WDSPRxProfile::WDSPRxNB2Mode) tmp;
            d.readDouble(123 + 50*i, &m_profiles[i].m_nbSlewTime, 0.1);
            d.readDouble(124 + 50*i, &m_profiles[i].m_nbLeadTime, 0.1);
            d.readDouble(125 + 50*i, &m_profiles[i].m_nbLagTime, 0.1);
            d.readS32   (126 + 50*i, &m_profiles[i].m_nbThreshold, 30);
            d.readDouble(127 + 50*i, &m_profiles[i].m_nbAvgTime, 50.0);
            // Noise reduction
            d.readBool  (130 + 50*i, &m_profiles[i].m_dnr, false);
            d.readBool  (131 + 50*i, &m_profiles[i].m_snb, false);
            d.readBool  (132 + 50*i, &m_profiles[i].m_anf, false);
            d.readS32   (133 + 50*i, &tmp);
            m_profiles[i].m_nrScheme = (WDSPRxProfile::WDSPRxNRScheme) tmp;
            d.readS32   (134 + 50*i, &tmp);
            m_profiles[i].m_nr2Gain = (WDSPRxProfile::WDSPRxNR2Gain) tmp;
            d.readS32   (135 + 50*i, &tmp);
            m_profiles[i].m_nr2NPE = (WDSPRxProfile::WDSPRxNR2NPE) tmp;
            d.readS32   (136 + 50*i, &tmp);
            m_profiles[i].m_nrPosition = (WDSPRxProfile::WDSPRxNRPosition) tmp;
            d.readBool  (137 + 50*i, &m_profiles[i].m_nr2ArtifactReduction);
            // Demods
            d.readBool  (140 + 50*i, &m_amFadeLevel, false);
            d.readBool  (141 + 50*i, &m_cwPeaking, false);
            d.readDouble(142 + 50*i, &m_profiles[i].m_cwPeakFrequency, 600.0);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
