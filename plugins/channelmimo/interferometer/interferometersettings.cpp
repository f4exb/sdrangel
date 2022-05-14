///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "interferometersettings.h"

InterferometerSettings::InterferometerSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void InterferometerSettings::resetToDefaults()
{
    m_correlationType = CorrelationAdd;
    m_rgbColor = QColor(128, 128, 128).rgb();
    m_title = "Interferometer";
    m_log2Decim = 0;
    m_filterChainHash = 0;
    m_phase = 0;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray InterferometerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(2, (int) m_correlationType);
    s.writeU32(3, m_rgbColor);
    s.writeString(4, m_title);
    s.writeU32(5, m_log2Decim);
    s.writeU32(6, m_filterChainHash);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIDeviceIndex);
    s.writeU32(11, m_reverseAPIChannelIndex);
    s.writeS32(12, m_phase);
    s.writeS32(13,m_workspaceIndex);
    s.writeBlob(14, m_geometryBytes);
    s.writeBool(15, m_hidden);

    if (m_spectrumGUI) {
        s.writeBlob(20, m_spectrumGUI->serialize());
    }
    if (m_scopeGUI) {
        s.writeBlob(21, m_scopeGUI->serialize());
    }
    if (m_channelMarker) {
        s.writeBlob(22, m_channelMarker->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(23, m_rollupState->serialize());
    }

    return s.final();
}

bool InterferometerSettings::deserialize(const QByteArray& data)
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
        int tmp;
        quint32 utmp;

        d.readS32(2, &tmp, 0);
        m_correlationType = (CorrelationType) tmp;
        d.readU32(3, &m_rgbColor);
        d.readString(4, &m_title, "Interpolator");
        d.readU32(5, &utmp, 0);
        m_log2Decim = utmp > 6 ? 6 : utmp;
        d.readU32(6, &m_filterChainHash, 0);
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(11, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readS32(12, &tmp, 0);
        m_phase = tmp < -180 ? -180 : tmp > 180 ? 180 : tmp;
        d.readS32(13, &m_workspaceIndex);
        d.readBlob(14, &m_geometryBytes);
        d.readBool(15, &m_hidden, false);

        if (m_spectrumGUI)
        {
            d.readBlob(20, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        if (m_scopeGUI)
        {
            d.readBlob(21, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(22, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        if (m_rollupState)
        {
            d.readBlob(23, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
