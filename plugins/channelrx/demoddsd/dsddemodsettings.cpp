///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free som_udpCopyAudioftware; you can redistribute it and/or modify          //
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

#include <QColor>

#include "dsp/dspengine.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "dsddemodsettings.h"

DSDDemodSettings::DSDDemodSettings() :
    m_channelMarker(0),
    m_scopeGUI(0)
{
    resetToDefaults();
}

void DSDDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 12500.0;
    m_fmDeviation = 5000.0;
    m_demodGain = 1.25;
    m_volume = 2.0;
    m_baudRate = 4800;
    m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
    m_squelch = -40.0;
    m_audioMute = false;
    m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
    m_enableCosineFiltering = false;
    m_syncOrConstellation = false;
    m_slot1On = true;
    m_slot2On = false;
    m_tdmaStereo = false;
    m_pllLock = true;
    m_udpCopyAudio = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_rgbColor = QColor(0, 255, 255).rgb();
    m_title = "DSD Demodulator";
    m_highPassFilter = false;
}

QByteArray DSDDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_rfBandwidth/100.0);
    s.writeS32(3, m_demodGain*100.0);
    s.writeS32(4, m_fmDeviation/100.0);
    s.writeS32(5, m_squelch*10.0);
    s.writeU32(7, m_rgbColor);
    s.writeS32(8, m_squelchGate);
    s.writeS32(9, m_volume*10.0);

    if (m_scopeGUI) {
        s.writeBlob(10, m_scopeGUI->serialize());
    }

    s.writeS32(11, m_baudRate);
    s.writeBool(12, m_enableCosineFiltering);
    s.writeBool(13, m_syncOrConstellation);
    s.writeBool(14, m_slot1On);
    s.writeBool(15, m_slot2On);
    s.writeBool(16, m_tdmaStereo);

    if (m_channelMarker) {
        s.writeBlob(17, m_channelMarker->serialize());
    }

    s.writeString(18, m_title);
    s.writeBool(19, m_highPassFilter);

    return s.final();
}

bool DSDDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        QString strtmp;
        qint32 tmp;

        if (m_channelMarker) {
            d.readBlob(17, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &tmp, 125);
        m_rfBandwidth = tmp * 100.0;
        d.readS32(3, &tmp, 125);
        m_demodGain = tmp / 100.0;
        d.readS32(4, &tmp, 50);
        m_fmDeviation = tmp * 100.0;
        d.readS32(5, &tmp, -400);
        m_squelch = tmp / 10.0;
        d.readU32(7, &m_rgbColor);
        d.readS32(8, &m_squelchGate, 5);
        d.readS32(9, &tmp, 20);
        m_volume = tmp / 10.0;

        if (m_scopeGUI) {
            d.readBlob(10, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        d.readS32(11, &m_baudRate, 4800);
        d.readBool(12, &m_enableCosineFiltering, false);
        d.readBool(13, &m_syncOrConstellation, false);
        d.readBool(14, &m_slot1On, false);
        d.readBool(15, &m_slot2On, false);
        d.readBool(16, &m_tdmaStereo, false);
        d.readString(18, &m_title, "DSD Demodulator");
        d.readBool(19, &m_highPassFilter, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

