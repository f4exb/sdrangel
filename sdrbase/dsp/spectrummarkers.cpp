///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#include "util/simpleserializer.h"
#include "spectrummarkers.h"

QByteArray SpectrumHistogramMarker::serialize() const
{
    SimpleSerializer s(1);

    s.writeFloat(1, m_frequency);
    s.writeFloat(2, m_power);
    s.writeS32(3, (int) m_markerType);
    int r, g, b;
    m_markerColor.getRgb(&r, &g, &b);
    s.writeS32(4, r);
    s.writeS32(5, g);
    s.writeS32(6, b);
    s.writeBool(7, m_show);

    return s.final();
}

bool SpectrumHistogramMarker::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid()) {
		return false;
	}

    if (d.getVersion() == 1)
    {
        int tmp;

        d.readFloat(1, &m_frequency, 0);
        d.readFloat(2, &m_power, 0);
        d.readS32(3, &tmp, 0);
        m_markerType = (SpectrumMarkerType) tmp;
        int r, g, b;
        d.readS32(4, &r, 255);
        m_markerColor.setRed(r);
        d.readS32(5, &g, 255);
        m_markerColor.setGreen(g);
        d.readS32(6, &b, 255);
        m_markerColor.setBlue(b);

        d.readBool(7, &m_show);

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SpectrumWaterfallMarker::serialize() const
{
    SimpleSerializer s(1);

    s.writeFloat(1, m_frequency);
    s.writeFloat(2, m_time);
    int r, g, b;
    m_markerColor.getRgb(&r, &g, &b);
    s.writeS32(4, r);
    s.writeS32(5, g);
    s.writeS32(6, b);
    s.writeS32(7, (int) m_show);

    return s.final();
}

bool SpectrumWaterfallMarker::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid()) {
		return false;
	}

    if (d.getVersion() == 1)
    {
        d.readFloat(1, &m_frequency, 0);
        d.readFloat(2, &m_time, 0);
        int r, g, b;
        d.readS32(4, &r, 255);
        m_markerColor.setRed(r);
        d.readS32(5, &g, 255);
        m_markerColor.setGreen(g);
        d.readS32(6, &b, 255);
        m_markerColor.setBlue(b);
        d.readBool(7, &m_show);

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SpectrumAnnotationMarker::serialize() const
{
    SimpleSerializer s(1);

    s.writeS64(1, m_startFrequency);
    s.writeU32(2, m_bandwidth);
    int r, g, b;
    m_markerColor.getRgb(&r, &g, &b);
    s.writeS32(4, r);
    s.writeS32(5, g);
    s.writeS32(6, b);
    s.writeBool(7, m_show);
    s.writeString(8, m_text);

    return s.final();
}

bool SpectrumAnnotationMarker::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid()) {
		return false;
	}

    if (d.getVersion() == 1)
    {
        int tmp;

        d.readS64(1, &m_startFrequency, 0);
        d.readU32(2, &m_bandwidth, 0);
        int r, g, b;
        d.readS32(4, &r, 255);
        m_markerColor.setRed(r);
        d.readS32(5, &g, 255);
        m_markerColor.setGreen(g);
        d.readS32(6, &b, 255);
        m_markerColor.setBlue(b);
        d.readS32   (7, &tmp, 1);
        m_show = (ShowState) tmp;
        d.readString(8, &m_text);

        return true;
    }
    else
    {
        return false;
    }

}
