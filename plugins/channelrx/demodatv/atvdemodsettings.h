///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#ifndef PLUGINS_CHANNELRX_DEMODATV_ATVDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODATV_ATVDEMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

class Serializable;

struct ATVDemodSettings
{
    enum ATVModulation {
        ATV_FM1,  //!< Classical frequency modulation with discriminator #1
        ATV_FM2,  //!< Classical frequency modulation with discriminator #2
        ATV_FM3,  //!< Classical frequency modulation with phase derivative discriminator
        ATV_AM,   //!< Classical amplitude modulation
        ATV_USB,  //!< AM with vestigial lower side band (main signal is in the upper side)
        ATV_LSB   //!< AM with vestigial upper side band (main signal is in the lower side)
    };

    enum ATVStd
    {
        ATVStdPAL625,
        ATVStdPAL525,
        ATVStd819,
        ATVStdShortInterleaved,
        ATVStdShort,
        ATVStdHSkip
    };

    // RF settings
    qint64        m_inputFrequencyOffset; //!< Offset from baseband center frequency
    int           m_bfoFrequency;         //!< BFO frequency (Hz)
    ATVModulation m_atvModulation;        //!< RF modulation type
    float         m_fmDeviation;          //!< Expected FM deviation
    int           m_amScalingFactor;      //!< Factor in % to apply to detected signal scale
    int           m_amOffsetFactor;       //!< Factor in % to apply to adjusted signal scale
    bool          m_fftFiltering;         //!< Toggle FFT filter
    unsigned int  m_fftOppBandwidth;      //!< FFT filter lower frequency cutoff (Hz)
    unsigned int  m_fftBandwidth;         //!< FFT filter high frequency cutoff (Hz)

    // ATV settings
    int           m_nbLines;              //!< Number of lines per full frame
    int           m_fps;                  //!< Number of frames per second
    ATVStd        m_atvStd;               //!< Standard
    bool          m_hSync;                //!< Enable/disable horizontal sybchronization
    bool          m_vSync;                //!< Enable/disable vertical synchronization
    bool          m_invertVideo;          //!< Toggle invert video
    bool          m_halfFrames;           //!< Toggle half frames processing
    float         m_levelSynchroTop;      //!< Horizontal synchronization top level (0.0 to 1.0 scale)
    float         m_levelBlack;           //!< Black level (0.0 to 1.0 scale)

    // common channel settings
    quint32 m_rgbColor;
    QString m_title;
    QString m_udpAddress;
    uint16_t m_udpPort;
    Serializable *m_channelMarker;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    ATVDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    int getRFSliderDivisor(unsigned int sampleRate);

    static int getFps(int fpsIndex);
    static int getFpsIndex(int fps);
    static int getNumberOfLines(int nbLinesIndex);
    static int getNumberOfLinesIndex(int nbLines);
    static float getNominalLineTime(int nbLines, int fps);
    static float getRFBandwidthDivisor(ATVModulation modulation);
    static void getBaseValues(int sampleRate, int linesPerSecond, uint32_t& nbPointsPerLine);

private:
    int m_rfSliderDivisor;
};

#endif /* PLUGINS_CHANNELRX_DEMODATV_ATVDEMODSETTINGS_H_ */
