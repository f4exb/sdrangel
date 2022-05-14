///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_FILESINKSETTINGS_H_
#define INCLUDE_FILESINKSETTINGS_H_

#include <QByteArray>
#include <QString>

class Serializable;

struct FileSinkSettings
{
    qint32 m_inputFrequencyOffset;
    QString m_fileRecordName;
    quint32 m_rgbColor;
    QString m_title;
    uint32_t m_log2Decim;
    bool m_spectrumSquelchMode;
    float m_spectrumSquelch;
    int m_preRecordTime;
    int m_squelchPostRecordTime;
    bool m_squelchRecordingEnable;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    Serializable *m_spectrumGUI;
    Serializable *m_channelMarker;
    Serializable *m_rollupState;

    FileSinkSettings();
    void resetToDefaults();
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static unsigned int getNbFixedShiftIndexes(int log2Decim);
    static int getHalfBand(int sampleRate, int log2Decim);
    static unsigned int getFixedShiftIndexFromOffset(int sampleRate, int log2Decim, int frequencyOffset);
    static int getOffsetFromFixedShiftIndex(int sampleRate, int log2Decim, int shiftIndex);
};

#endif /* INCLUDE_FILESINKSETTINGS_H_ */
