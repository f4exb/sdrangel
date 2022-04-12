///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Remote sink channel (Rx) UDP sender thread                                    //
//                                                                               //
// SDRangel can work as a detached SDR front end. With this plugin it can        //
// sends the I/Q samples stream to another SDRangel instance via UDP.            //
// It is controlled via a Web REST API.                                          //
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

#include "remotesinksettings.h"

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"


RemoteSinkSettings::RemoteSinkSettings()
{
    resetToDefaults();
}

void RemoteSinkSettings::resetToDefaults()
{
    m_nbFECBlocks = 0;
    m_nbTxBytes = 2;
    m_deviceCenterFrequency = 0;
    m_dataAddress = "127.0.0.1";
    m_dataPort = 9090;
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "Remote sink";
    m_log2Decim = 0;
    m_filterChainHash = 0;
    m_channelMarker = nullptr;
    m_rollupState = nullptr;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray RemoteSinkSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeU32(1, m_nbFECBlocks);
    s.writeU32(2, m_nbTxBytes);
    s.writeString(3, m_dataAddress);
    s.writeU32(4, m_dataPort);
    s.writeU32(5, m_rgbColor);
    s.writeString(6, m_title);
    s.writeBool(7, m_useReverseAPI);
    s.writeString(8, m_reverseAPIAddress);
    s.writeU32(9, m_reverseAPIPort);
    s.writeU32(10, m_reverseAPIDeviceIndex);
    s.writeU32(11, m_reverseAPIChannelIndex);
    s.writeU32(12, m_log2Decim);
    s.writeU32(13, m_filterChainHash);
    s.writeS32(14, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(15, m_rollupState->serialize());
    }

    s.writeU64(16, m_deviceCenterFrequency);

    if (m_channelMarker) {
        s.writeBlob(17, m_channelMarker->serialize());
    }

    s.writeS32(18, m_workspaceIndex);
    s.writeBlob(19, m_geometryBytes);
    s.writeBool(20, m_hidden);

    return s.final();
}

bool RemoteSinkSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t tmp;
        QString strtmp;
        QByteArray bytetmp;

        d.readU32(1, &tmp, 0);

        if (tmp < 128) {
            m_nbFECBlocks = tmp;
        } else {
            m_nbFECBlocks = 0;
        }

        d.readU32(2, &m_nbTxBytes, 2);
        d.readString(3, &m_dataAddress, "127.0.0.1");
        d.readU32(4, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_dataPort = tmp;
        } else {
            m_dataPort = 9090;
        }

        d.readU32(5, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readString(6, &m_title, "Remote sink");
        d.readBool(7, &m_useReverseAPI, false);
        d.readString(8, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(9, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_reverseAPIPort = tmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(10, &tmp, 0);
        m_reverseAPIDeviceIndex = tmp > 99 ? 99 : tmp;
        d.readU32(11, &tmp, 0);
        m_reverseAPIChannelIndex = tmp > 99 ? 99 : tmp;
        d.readU32(12, &tmp, 0);
        m_log2Decim = tmp > 6 ? 6 : tmp;
        d.readU32(13, &m_filterChainHash, 0);
        d.readS32(14, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(15, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readU64(16, &m_deviceCenterFrequency, 0);

        if (m_channelMarker)
        {
            d.readBlob(17, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(18, &m_workspaceIndex, 0);
        d.readBlob(19, &m_geometryBytes);
        d.readBool(20, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}





