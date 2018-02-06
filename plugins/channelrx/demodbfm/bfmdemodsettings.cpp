///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
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

#include "bfmdemodsettings.h"

const int BFMDemodSettings::m_nbRFBW = 9;
const int BFMDemodSettings::m_rfBW[] = {
    80000, 100000, 120000, 140000, 160000, 180000, 200000, 220000, 250000
};

BFMDemodSettings::BFMDemodSettings() :
    m_channelMarker(0),
    m_spectrumGUI(0)
{
    resetToDefaults();
}

void BFMDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = getRFBW(5);
    m_afBandwidth = 15000;
    m_volume = 2.0;
    m_squelch = -60.0;
    m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
    m_audioStereo = false;
    m_lsbStereo = false;
    m_showPilot = false;
    m_rdsActive = false;
    m_copyAudioToUDP = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_rgbColor = QColor(80, 120, 228).rgb();
    m_title = "Broadcast FM Demod";
}

QByteArray BFMDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, getRFBWIndex(m_rfBandwidth));
    s.writeS32(3, m_afBandwidth/1000.0);
    s.writeS32(4, m_volume*10.0);
    s.writeS32(5, m_squelch);
    s.writeU32(7, m_rgbColor);

    if (m_spectrumGUI) {
        s.writeBlob(8, m_spectrumGUI->serialize());
    }

    s.writeBool(9, m_audioStereo);
    s.writeBool(10, m_lsbStereo);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeString(12, m_title);

    return s.final();
}

bool BFMDemodSettings::deserialize(const QByteArray& data)
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
        QString strtmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readS32(2, &tmp, 4);
        m_rfBandwidth = getRFBW(tmp);
        d.readS32(3, &tmp, 3);
        m_afBandwidth = tmp * 1000.0;
        d.readS32(4, &tmp, 20);
        m_volume = tmp * 0.1;
        d.readS32(5, &tmp, -60);
        m_squelch = tmp;
        d.readU32(7, &m_rgbColor);

        d.readBlob(8, &bytetmp);

        if (m_spectrumGUI) {
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readBool(9, &m_audioStereo, false);
        d.readBool(10, &m_lsbStereo, false);

        d.readBlob(11, &bytetmp);

        if (m_channelMarker) {
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(12, &m_title, "Broadcast FM Demod");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

int BFMDemodSettings::getRFBW(int index)
{
    if (index < 0) {
        return m_rfBW[0];
    } else if (index < m_nbRFBW) {
        return m_rfBW[index];
    } else {
        return m_rfBW[m_nbRFBW-1];
    }
}

int BFMDemodSettings::getRFBWIndex(int rfbw)
{
    for (int i = 0; i < m_nbRFBW; i++)
    {
        if (rfbw <= m_rfBW[i])
        {
            return i;
        }
    }

    return m_nbRFBW-1;
}
