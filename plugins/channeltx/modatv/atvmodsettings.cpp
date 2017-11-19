///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include "atvmodsettings.h"

ATVModSettings::ATVModSettings() :
    m_channelMarker(0)
{
    resetToDefaults();
}

void ATVModSettings::resetToDefaults()
{
    m_outputSampleRate = 1000000;
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 1000000;
    m_rfOppBandwidth = 0;
    m_atvStd = ATVStdPAL625;
    m_nbLines = 625;
    m_fps = 25;
    m_atvModInput = ATVModInputHBars;
    m_uniformLevel = 0.5f;
    m_atvModulation = ATVModulationAM;
    m_videoPlayLoop = false;
    m_videoPlay = false;
    m_cameraPlay = false;
    m_channelMute = false;
    m_invertedVideo = false;
    m_rfScalingFactor = 29204.0f; // -1dB
    m_fmExcursion = 0.5f;         // half bandwidth
    m_forceDecimator = false;
    m_overlayText = "ATV";
    m_rgbColor = QColor(255, 255, 255).rgb();
    m_title = "ATV Modulator";
}

QByteArray ATVModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_rfBandwidth);
    s.writeS32(3, roundf(m_uniformLevel * 100.0)); // percent
    s.writeS32(4, (int) m_atvStd);
    s.writeS32(5, (int) m_atvModInput);
    s.writeU32(6, m_rgbColor);
    s.writeReal(7, m_rfOppBandwidth);
    s.writeS32(8, (int) m_atvModulation);
    s.writeBool(9, m_invertedVideo);
    s.writeS32(10, m_nbLines);
    s.writeS32(11, m_fps);
    s.writeS32(12, roundf(m_rfScalingFactor / 327.68f));
    s.writeS32(13, roundf(m_fmExcursion * 1000.0)); // pro mill
    s.writeString(14, m_overlayText);

    if (m_channelMarker) {
        s.writeBlob(15, m_channelMarker->serialize());
    }

    s.writeString(16, m_title);

    return s.final();
}

bool ATVModSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_rfBandwidth, 1000000);
        d.readS32(3, &tmp, 100);
        m_uniformLevel = tmp / 100.0; // percent
        d.readS32(4, &tmp, 0);
        m_atvStd = (ATVStd) tmp;
        d.readS32(5, &tmp, 0);
        m_atvModInput = (ATVModInput) tmp;
        d.readU32(6, &m_rgbColor, 0);
        d.readReal(7, &m_rfOppBandwidth, 0);
        d.readS32(8, &tmp, 0);
        m_atvModulation = (ATVModulation) tmp;
        d.readBool(9, &m_invertedVideo, false);
        d.readS32(10, &m_nbLines, 625);
        d.readS32(11, &m_fps, 25);
        d.readS32(12, &tmp, 80);
        m_rfScalingFactor = tmp * 327.68f;
        d.readS32(13, &tmp, 250);
        m_fmExcursion = tmp / 1000.0; // pro mill
        d.readString(14, &m_overlayText, "ATV");

        if (m_channelMarker)
        {
            d.readBlob(15, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readString(16, &m_title, "ATV Modulator");

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
