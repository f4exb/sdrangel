///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "limerfesettings.h"

LimeRFESettings::LimeRFESettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void LimeRFESettings::resetToDefaults()
{
    m_devicePath = "";
    m_title = "Lime RFE";
    m_rgbColor = QColor(50, 205, 50).rgb();
    m_rxChannels = ChannelsWideband;
    m_rxWidebandChannel = WidebandLow;
    m_rxHAMChannel = HAM_144_146MHz;
    m_rxCellularChannel = CellularBand38;
    m_rxPort = RxPortJ3;
    m_amfmNotch = false;
    m_attenuationFactor = 0;
    m_txChannels = ChannelsWideband;
    m_txWidebandChannel = WidebandLow;
    m_txHAMChannel = HAM_144_146MHz;
    m_txCellularChannel = CellularBand38;
    m_txPort = TxPortJ3;
    m_swrEnable = false;
    m_swrSource = SWRExternal;
    m_txRxDriven = false;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
}

QByteArray LimeRFESettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, (int) m_rxChannels);
    s.writeS32(2, (int) m_rxWidebandChannel);
    s.writeS32(3, (int) m_rxHAMChannel);
    s.writeS32(4, (int) m_rxCellularChannel);
    s.writeS32(5, (int) m_rxPort);
    s.writeBool(6, m_amfmNotch);
    s.writeU32(7, m_attenuationFactor);

    s.writeS32(10, (int) m_txChannels);
    s.writeS32(11, (int) m_txWidebandChannel);
    s.writeS32(12, (int) m_txHAMChannel);
    s.writeS32(13, (int) m_txCellularChannel);
    s.writeS32(14, (int) m_txPort);
    s.writeBool(15, m_swrEnable);
    s.writeS32(16, (int) m_swrSource);

    s.writeBool(20, m_txRxDriven);

    s.writeString(30, m_title);
    s.writeU32(31, m_rgbColor);
    s.writeBool(32, m_useReverseAPI);
    s.writeString(33, m_reverseAPIAddress);
    s.writeU32(34, m_reverseAPIPort);
    s.writeU32(35, m_reverseAPIFeatureSetIndex);
    s.writeU32(36, m_reverseAPIFeatureIndex);

    if (m_rollupState) {
        s.writeBlob(37, m_rollupState->serialize());
    }

    s.writeS32(38, m_workspaceIndex);
    s.writeBlob(39, m_geometryBytes);
    s.writeString(40, m_devicePath);
    s.writeBlob(41, m_calib.serialize());

    return s.final();
}

bool LimeRFESettings::deserialize(const QByteArray& data)
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
        uint32_t utmp;
        int tmp;

        d.readS32(1, &tmp, (int) ChannelsWideband);
        m_rxChannels = (ChannelGroups) tmp;
        d.readS32(2, &tmp, (int) WidebandLow);
        m_rxWidebandChannel = (WidebandChannel) tmp;
        d.readS32(3, &tmp, (int) HAM_144_146MHz);
        m_rxHAMChannel = (HAMChannel) tmp;
        d.readS32(4, &tmp, (int) CellularBand38);
        m_rxCellularChannel = (CellularChannel) tmp;
        d.readS32(5, &tmp, (int) RxPortJ3);
        m_rxPort = (RxPort) tmp;
        d.readBool(6, &m_amfmNotch, false);
        d.readU32(7, &m_attenuationFactor, 0);

        d.readS32(10, &tmp, (int) ChannelsWideband);
        m_txChannels = (ChannelGroups) tmp;
        d.readS32(11, &tmp, (int) WidebandLow);
        m_txWidebandChannel = (WidebandChannel) tmp;
        d.readS32(12, &tmp, (int) HAM_144_146MHz);
        m_txHAMChannel = (HAMChannel) tmp;
        d.readS32(13, &tmp, (int) CellularBand38);
        m_txCellularChannel = (CellularChannel) tmp;
        d.readS32(14, &tmp, (int) TxPortJ3);
        m_txPort = (TxPort) tmp;
        d.readBool(15, &m_swrEnable, false);
        d.readS32(16, &tmp, (int) SWRExternal);
        m_swrSource = (SWRSource) tmp;

        d.readBool(20, &m_txRxDriven, false);

        d.readString(30, &m_title, "Lime RFE");
        d.readU32(31, &m_rgbColor, QColor(50, 205, 50).rgb());
        d.readBool(32, &m_useReverseAPI, false);
        d.readString(33, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(34, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(35, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(36, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(37, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(38, &m_workspaceIndex, 0);
        d.readBlob(39, &m_geometryBytes);
        d.readString(40, &m_devicePath, "");
        d.readBlob(41, &bytetmp);
        m_calib.deserialize(bytetmp);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
