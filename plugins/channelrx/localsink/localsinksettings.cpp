///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "localsinksettings.h"

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"


LocalSinkSettings::LocalSinkSettings()
{
    resetToDefaults();
}

void LocalSinkSettings::resetToDefaults()
{
    m_localDeviceIndex = -1;
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "Local sink";
    m_log2Decim = 0;
    m_filterChainHash = 0;
    m_channelMarker = nullptr;
    m_rollupState = nullptr;
    m_play = false;
    m_dsp = false;
    m_gaindB = 0;
    m_fftOn = false;
    m_log2FFT = 10;
    m_fftWindow = FFTWindow::Function::Bartlett;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray LocalSinkSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_localDeviceIndex);

    if (m_channelMarker) {
        s.writeBlob(2, m_channelMarker->serialize());
    }

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

    s.writeS32(16, m_workspaceIndex);
    s.writeBlob(17, m_geometryBytes);
    s.writeBool(18, m_hidden);
    s.writeBool(19, m_dsp);
    s.writeS32(20, m_gaindB);

    if (m_spectrumGUI) {
        s.writeBlob(21, m_spectrumGUI->serialize());
    }

    s.writeBool(22, m_fftOn);
    s.writeU32(23, (int) m_fftWindow);

    s.writeU32(99, m_fftBands.size());
    int i = 0;

    for (auto fftBand : m_fftBands)
    {
        s.writeFloat(100 + 2*i, fftBand.first);
        s.writeFloat(101 + 2*i, fftBand.second);
        i++;

        if (i == m_maxFFTBands) {
            break;
        }
    }

    return s.final();
}

bool LocalSinkSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t utmp;
        QString strtmp;
        QByteArray bytetmp;

        d.readS32(1, &m_localDeviceIndex, -1);

        if (m_channelMarker)
        {
            d.readBlob(2, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(5, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readString(6, &m_title, "Local sink");
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
        d.readU32(12, &utmp, 0);
        m_log2Decim = utmp > 6 ? 6 : utmp;
        d.readU32(13, &m_filterChainHash, 0);
        d.readS32(14, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(15, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(16, &m_workspaceIndex, 0);
        d.readBlob(17, &m_geometryBytes);
        d.readBool(18, &m_hidden, false);
        d.readBool(19, &m_dsp, false);
        d.readS32(20, &m_gaindB, 0);

        if (m_spectrumGUI)
        {
            d.readBlob(21, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }

        d.readBool(22, &m_fftOn, false);
        d.readU32(23, &utmp, 0);
        m_fftWindow = (utmp > (uint32_t) FFTWindow::Function::BlackmanHarris7) ?
            FFTWindow::Function::BlackmanHarris7 :
            (FFTWindow::Function) utmp;

        uint32_t nbBands;
        d.readU32(99, &nbBands, 0);
        m_fftBands.clear();

        for (uint32_t i = 0; i < std::min(nbBands, m_maxFFTBands); i++)
        {
            float f1, w;
            d.readFloat(100 + 2*i, &f1, 0);
            d.readFloat(101 + 2*i, &w, 0);
            m_fftBands.push_back(std::pair<float, float>{f1, w});
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}





