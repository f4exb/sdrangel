///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "bfmmodsettings.h"

// Stereo with RDS needs about 256 kHz (Carson's rule with 75 kHz deviation and a
// 53 kHz multiplex). Narrower settings truncate the FM sidebands, and below about
// 150 kHz the channel sample rate is too low for the 57 kHz RDS subcarrier.
const int BFMModSettings::m_rfBW[] = {
    150000, 180000, 200000, 220000, 240000, 256000, 280000, 300000
};
const int BFMModSettings::m_nbRfBW = 8;
const Real BFMModSettings::m_minAFBandwidth = 1000.0f;
const Real BFMModSettings::m_maxAFBandwidth = 15000.0f;

BFMModSettings::BFMModSettings() :
    m_channelMarker(nullptr),
    m_cwKeyerGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void BFMModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 256000.0f;
    m_afBandwidth = 15000.0f;
    m_fmDeviation = 75000.0f;
    m_toneFrequency = 1000.0f;
    m_volumeFactor = 1.0f;
    m_audioStereo = true;
    m_preEmphasis = 1;
    m_pilotLevel = 0.09f;
    m_rdsActive = true;
    m_rdsLevel = 0.03f;
    m_rdsPI = 0x1234;
    m_rdsPTY = 0;
    m_rdsPS = "SDRangel";
    m_rdsRadioText = "SDRangel Broadcast FM";
    m_fileName = "";
    m_channelMute = false;
    m_playLoop = false;
    m_rgbColor = QColor(0, 96, 255).rgb();
    m_title = "Broadcast FM Modulator";
    m_modAFInput = BFMModInputNone;
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

QByteArray BFMModSettings::serialize() const
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

    if (m_channelMarker) {
        s.writeBlob(9, m_channelMarker->serialize());
    }

    s.writeString(10, m_title);
    s.writeString(11, m_audioDeviceName);
    s.writeS32(12, (int) m_modAFInput);
    s.writeBool(13, m_useReverseAPI);
    s.writeString(14, m_reverseAPIAddress);
    s.writeU32(15, m_reverseAPIPort);
    s.writeU32(16, m_reverseAPIDeviceIndex);
    s.writeU32(17, m_reverseAPIChannelIndex);
    s.writeS32(18, m_streamIndex);
    s.writeString(19, m_feedbackAudioDeviceName);
    s.writeReal(20, m_feedbackVolumeFactor);
    s.writeBool(21, m_feedbackAudioEnable);

    if (m_rollupState) {
        s.writeBlob(22, m_rollupState->serialize());
    }

    s.writeS32(23, m_workspaceIndex);
    s.writeBlob(24, m_geometryBytes);
    s.writeBool(25, m_hidden);
    s.writeBool(26, m_audioStereo);
    s.writeS32(27, m_preEmphasis);
    s.writeReal(28, m_pilotLevel);
    s.writeBool(29, m_rdsActive);
    s.writeReal(30, m_rdsLevel);
    s.writeU32(31, m_rdsPI);
    s.writeU32(32, m_rdsPTY);
    s.writeString(33, m_rdsPS);
    s.writeString(34, m_rdsRadioText);
    s.writeString(35, m_fileName);

    return s.final();
}

bool BFMModSettings::deserialize(const QByteArray& data)
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
        d.readReal(2, &m_rfBandwidth, 256000.0);
        m_rfBandwidth = boundRFBandwidth(m_rfBandwidth);
        d.readReal(3, &m_afBandwidth, 15000.0);
        m_afBandwidth = boundAFBandwidth(m_afBandwidth);
        d.readReal(4, &m_fmDeviation, 75000.0);
        m_fmDeviation = boundFMDeviation(m_fmDeviation);
        d.readU32(5, &m_rgbColor);
        d.readReal(6, &m_toneFrequency, 1000.0);
        m_toneFrequency = boundToneFrequency(m_toneFrequency);
        d.readReal(7, &m_volumeFactor, 1.0);
        m_volumeFactor = boundVolumeFactor(m_volumeFactor);
        d.readBlob(8, &bytetmp);

        if (m_cwKeyerGUI) {
            m_cwKeyerGUI->deserialize(bytetmp);
        } else { // standalone operation with presets
            m_cwKeyerSettings.deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(9, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(10, &m_title, "Broadcast FM Modulator");
        d.readString(11, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);

        d.readS32(12, &tmp, 0);
        if ((tmp < 0) || (tmp > (int) BFMModInputAF::BFMModInputCWTone)) {
            m_modAFInput = BFMModInputNone;
        } else {
            m_modAFInput = (BFMModInputAF) tmp;
        }

        d.readBool(13, &m_useReverseAPI, false);
        d.readString(14, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(15, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(16, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(17, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(18, &m_streamIndex, 0);
        d.readString(19, &m_feedbackAudioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readReal(20, &m_feedbackVolumeFactor, 0.5);
        m_feedbackVolumeFactor = boundFeedbackVolumeFactor(m_feedbackVolumeFactor);
        d.readBool(21, &m_feedbackAudioEnable, false);

        if (m_rollupState)
        {
            d.readBlob(22, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(23, &m_workspaceIndex, 0);
        d.readBlob(24, &m_geometryBytes);
        d.readBool(25, &m_hidden, false);
        d.readBool(26, &m_audioStereo, true);
        d.readS32(27, &m_preEmphasis, 1);
        m_preEmphasis = qBound(0, m_preEmphasis, 2);
        d.readReal(28, &m_pilotLevel, 0.09f);
        m_pilotLevel = qBound(0.0f, m_pilotLevel, 0.2f);
        d.readBool(29, &m_rdsActive, true);
        d.readReal(30, &m_rdsLevel, 0.03f);
        m_rdsLevel = qBound(0.0f, m_rdsLevel, 0.1f);
        d.readU32(31, &utmp, 0x1234);
        m_rdsPI = quint16(utmp);
        d.readU32(32, &utmp, 0);
        m_rdsPTY = quint8(qMin(utmp, 31U));
        d.readString(33, &m_rdsPS, "SDRangel");
        m_rdsPS = m_rdsPS.left(8);
        d.readString(34, &m_rdsRadioText, "SDRangel Broadcast FM");
        m_rdsRadioText = m_rdsRadioText.left(64);
        d.readString(35, &m_fileName, "");

        return true;
    }
    else
    {
        qDebug() << "BFMModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

void BFMModSettings::applySettings(const QStringList& settingsKeys, const BFMModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("afBandwidth")) {
        m_afBandwidth = settings.m_afBandwidth;
    }
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (settingsKeys.contains("toneFrequency")) {
        m_toneFrequency = settings.m_toneFrequency;
    }
    if (settingsKeys.contains("volumeFactor")) {
        m_volumeFactor = settings.m_volumeFactor;
    }
    if (settingsKeys.contains("audioStereo")) {
        m_audioStereo = settings.m_audioStereo;
    }
    if (settingsKeys.contains("preEmphasis")) {
        m_preEmphasis = settings.m_preEmphasis;
    }
    if (settingsKeys.contains("pilotLevel")) {
        m_pilotLevel = settings.m_pilotLevel;
    }
    if (settingsKeys.contains("rdsActive")) {
        m_rdsActive = settings.m_rdsActive;
    }
    if (settingsKeys.contains("rdsLevel")) {
        m_rdsLevel = settings.m_rdsLevel;
    }
    if (settingsKeys.contains("rdsPI")) {
        m_rdsPI = settings.m_rdsPI;
    }
    if (settingsKeys.contains("rdsPTY")) {
        m_rdsPTY = settings.m_rdsPTY;
    }
    if (settingsKeys.contains("rdsPS")) {
        m_rdsPS = settings.m_rdsPS;
    }
    if (settingsKeys.contains("rdsRadioText")) {
        m_rdsRadioText = settings.m_rdsRadioText;
    }
    if (settingsKeys.contains("fileName")) {
        m_fileName = settings.m_fileName;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("playLoop")) {
        m_playLoop = settings.m_playLoop;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("modAFInput")) {
        m_modAFInput = settings.m_modAFInput;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("feedbackAudioDeviceName")) {
        m_feedbackAudioDeviceName = settings.m_feedbackAudioDeviceName;
    }
    if (settingsKeys.contains("feedbackVolumeFactor")) {
        m_feedbackVolumeFactor = settings.m_feedbackVolumeFactor;
    }
    if (settingsKeys.contains("feedbackAudioEnable")) {
        m_feedbackAudioEnable = settings.m_feedbackAudioEnable;
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
    if (settingsKeys.contains("cwKeyerSettings")) {
        m_cwKeyerSettings = settings.m_cwKeyerSettings;
    }
}

QString BFMModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("afBandwidth") || force) {
        ostr << " m_afBandwidth: " << m_afBandwidth;
    }
    if (settingsKeys.contains("fmDeviation") || force) {
        ostr << " m_fmDeviation: " << m_fmDeviation;
    }
    if (settingsKeys.contains("toneFrequency") || force) {
        ostr << " m_toneFrequency: " << m_toneFrequency;
    }
    if (settingsKeys.contains("volumeFactor") || force) {
        ostr << " m_volumeFactor: " << m_volumeFactor;
    }
    if (settingsKeys.contains("audioStereo") || force) {
        ostr << " m_audioStereo: " << m_audioStereo;
    }
    if (settingsKeys.contains("preEmphasis") || force) {
        ostr << " m_preEmphasis: " << m_preEmphasis;
    }
    if (settingsKeys.contains("rdsActive") || force) {
        ostr << " m_rdsActive: " << m_rdsActive;
    }
    if (settingsKeys.contains("pilotLevel") || force) {
        ostr << " m_pilotLevel: " << m_pilotLevel;
    }
    if (settingsKeys.contains("rdsLevel") || force) {
        ostr << " m_rdsLevel: " << m_rdsLevel;
    }
    if (settingsKeys.contains("rdsPI") || force) {
        ostr << " m_rdsPI: " << m_rdsPI;
    }
    if (settingsKeys.contains("rdsPTY") || force) {
        ostr << " m_rdsPTY: " << (int) m_rdsPTY;
    }
    if (settingsKeys.contains("rdsPS") || force) {
        ostr << " m_rdsPS: " << m_rdsPS.toStdString();
    }
    if (settingsKeys.contains("rdsRadioText") || force) {
        ostr << " m_rdsRadioText: " << m_rdsRadioText.toStdString();
    }
    if (settingsKeys.contains("fileName") || force) {
        ostr << " m_fileName: " << m_fileName.toStdString();
    }
    if (settingsKeys.contains("channelMute") || force) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (settingsKeys.contains("playLoop") || force) {
        ostr << " m_playLoop: " << m_playLoop;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("modAFInput") || force) {
        ostr << " m_modAFInput: " << (int)m_modAFInput;
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        ostr << " m_audioDeviceName: " << m_audioDeviceName.toStdString();
    }
    if (settingsKeys.contains("feedbackAudioDeviceName") || force) {
        ostr << " m_feedbackAudioDeviceName: " << m_feedbackAudioDeviceName.toStdString();
    }
    if (settingsKeys.contains("feedbackVolumeFactor") || force) {
        ostr << " m_feedbackVolumeFactor: " << m_feedbackVolumeFactor;
    }
    if (settingsKeys.contains("feedbackAudioEnable") || force) {
        ostr << " m_feedbackAudioEnable: " << m_feedbackAudioEnable;
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

int BFMModSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbRfBW) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbRfBW-1];
    }
}

int BFMModSettings::getRFBWIndex(int rfbw)
{
    for (int i = 0; i < m_nbRfBW; i++)
    {
        if (rfbw <= m_rfBW[i])
        {
            return i;
        }
    }

    return m_nbRfBW-1;
}

Real BFMModSettings::boundRFBandwidth(Real rfBandwidth)
{
    // Round up to one of the bandwidths the GUI offers
    return getRFBW(getRFBWIndex(rfBandwidth));
}

Real BFMModSettings::boundAFBandwidth(Real afBandwidth)
{
    return qBound(m_minAFBandwidth, afBandwidth, m_maxAFBandwidth);
}

// The remaining limits match the ranges of the GUI controls

float BFMModSettings::boundFMDeviation(float fmDeviation)
{
    return qBound(0.0f, fmDeviation, 100000.0f);
}

float BFMModSettings::boundToneFrequency(float toneFrequency)
{
    return qBound(100.0f, toneFrequency, 10000.0f);
}

float BFMModSettings::boundVolumeFactor(float volumeFactor)
{
    return qBound(0.0f, volumeFactor, 2.0f);
}

float BFMModSettings::boundFeedbackVolumeFactor(float feedbackVolumeFactor)
{
    return qBound(0.0f, feedbackVolumeFactor, 1.0f);
}
