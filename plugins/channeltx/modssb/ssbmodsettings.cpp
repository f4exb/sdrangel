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

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "ssbmodsettings.h"

const int SSBModSettings::m_agcTimeConstant[] = {
        1,
        2,
        5,
       10,
       20,
       50,
      100,
      200,
      500,
      990};

const int SSBModSettings::m_nbAGCTimeConstants = 10;

SSBModSettings::SSBModSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_cwKeyerGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void SSBModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_bandwidth = 3000.0;
    m_lowCutoff = 300.0;
    m_usb = true;
    m_toneFrequency = 1000.0;
    m_volumeFactor = 1.0;
    m_spanLog2 = 3;
    m_audioBinaural = false;
    m_audioFlipChannels = false;
    m_dsb = false;
    m_audioMute = false;
    m_playLoop = false;
    m_agc = false;
    m_cmpPreGainDB = -10;
    m_cmpThresholdDB = -60;
    m_rgbColor = QColor(0, 255, 0).rgb();
    m_title = "SSB Modulator";
    m_modAFInput = SSBModInputAF::SSBModInputNone;
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

QByteArray SSBModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, roundf(m_bandwidth / 100.0));
    s.writeS32(3, roundf(m_toneFrequency / 10.0));

    if (m_spectrumGUI) {
        s.writeBlob(4, m_spectrumGUI->serialize());
    }

    s.writeU32(5, m_rgbColor);

    if (m_cwKeyerGUI) {
        s.writeBlob(6, m_cwKeyerGUI->serialize());
    } else { // standalone operation with presets
        s.writeBlob(6, m_cwKeyerSettings.serialize());
    }

    s.writeS32(7, roundf(m_lowCutoff / 100.0));
    s.writeS32(8, m_spanLog2);
    s.writeBool(9, m_audioBinaural);
    s.writeBool(10, m_audioFlipChannels);
    s.writeBool(11, m_dsb);
    s.writeBool(12, m_agc);
    s.writeS32(13, m_cmpPreGainDB);
    s.writeS32(14, m_cmpThresholdDB);

    if (m_channelMarker) {
        s.writeBlob(18, m_channelMarker->serialize());
    }

    s.writeString(19, m_title);
    s.writeString(20, m_audioDeviceName);
    s.writeS32(21, (int) m_modAFInput);
    s.writeBool(22, m_useReverseAPI);
    s.writeString(23, m_reverseAPIAddress);
    s.writeU32(24, m_reverseAPIPort);
    s.writeU32(25, m_reverseAPIDeviceIndex);
    s.writeU32(26, m_reverseAPIChannelIndex);
    s.writeString(27, m_feedbackAudioDeviceName);
    s.writeReal(28, m_feedbackVolumeFactor);
    s.writeBool(29, m_feedbackAudioEnable);
    s.writeS32(30, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(31, m_rollupState->serialize());
    }

    s.writeS32(32, m_workspaceIndex);
    s.writeBlob(33, m_geometryBytes);
    s.writeBool(34, m_hidden);

    return s.final();
}

bool SSBModSettings::deserialize(const QByteArray& data)
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
        d.readS32(2, &tmp, 30);
        m_bandwidth = tmp * 100.0;
        d.readS32(3, &tmp, 100);
        m_toneFrequency = tmp * 10.0;

        if (m_spectrumGUI)
        {
            d.readBlob(4, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor);
        d.readBlob(6, &bytetmp);

        if (m_cwKeyerGUI) {
            m_cwKeyerGUI->deserialize(bytetmp);
        } else { // standalone operation with presets
            m_cwKeyerSettings.deserialize(bytetmp);
        }

        d.readS32(7, &tmp, 3);
        m_lowCutoff = tmp * 100.0;
        d.readS32(8, &m_spanLog2, 3);
        d.readBool(9, &m_audioBinaural, false);
        d.readBool(10, &m_audioFlipChannels, false);
        d.readBool(11, &m_dsb, false);
        d.readBool(12, &m_agc, false);
        d.readS32(13, &m_cmpPreGainDB, -10);
        d.readS32(14, &m_cmpThresholdDB, -60);

        if (m_channelMarker)
        {
            d.readBlob(18, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(19, &m_title, "SSB Modulator");
        d.readString(20, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(21, &tmp, 0);

        if ((tmp < 0) || (tmp > (int) SSBModInputAF::SSBModInputTone)) {
            m_modAFInput = SSBModInputNone;
        } else {
            m_modAFInput = (SSBModInputAF) tmp;
        }

        d.readBool(22, &m_useReverseAPI, false);
        d.readString(23, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(24, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(25, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(26, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(27, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(28, &m_feedbackVolumeFactor, 1.0);
        d.readBool(29, &m_feedbackAudioEnable, false);
        d.readS32(30, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(31, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(32, &m_workspaceIndex, 0);
        d.readBlob(33, &m_geometryBytes);
        d.readBool(34, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
