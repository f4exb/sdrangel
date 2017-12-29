///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
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

#ifndef PLUGINS_CHANNELRX_DEMODATV_ATVDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODATV_ATVDEMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

struct Serializable;

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
        ATVStd405,
        ATVStdShortInterleaved,
        ATVStdShort,
        ATVStdHSkip
    };

    // Added fields that correspond directly to UI inputs
    int m_lineTimeFactor;     //!< added: +/- 100 something
    int m_topTimeFactor;      //!< added: +/-  30 something
    int m_fpsIndex;           //!< added: FPS list index
    bool m_halfImage;         //!< added: true => m_fltRatioOfRowsToDisplay = 0.5, false => m_fltRatioOfRowsToDisplay = 1.0
    int m_RFBandwidthFactor;  //!< added: [1:100]
    int m_OppBandwidthFactor; //!< added: [0:100]
    int m_nbLinesIndex;       //!< added: #lines list index

    // Former RF
    int           m_intFrequencyOffset;
    ATVModulation m_enmModulation;
    float         m_fltRFBandwidth;
    float         m_fltRFOppBandwidth;
    bool          m_blnFFTFiltering;
    bool          m_blndecimatorEnable;
    float         m_fltBFOFrequency;
    float         m_fmDeviation;

    // Former <none>
    int m_intSampleRate;
    ATVStd m_enmATVStandard;
    int m_intNumberOfLines;
    float m_fltLineDuration;
    float m_fltTopDuration;
    float m_fltFramePerS;
    float m_fltRatioOfRowsToDisplay;
    float m_fltVoltLevelSynchroTop;
    float m_fltVoltLevelSynchroBlack;
    bool m_blnHSync;
    bool m_blnVSync;
    bool m_blnInvertVideo;
    int m_intVideoTabIndex;

    // Former private
    int m_intTVSampleRate;
    int m_intNumberSamplePerLine;

    // new
    quint32 m_rgbColor;
    QString m_title;
    QString m_udpAddress;
    uint16_t m_udpPort;
    Serializable *m_channelMarker;

    ATVDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    int getEffectiveSampleRate();
    float getLineTime();
    int getLineTimeFactor();
    float getTopTime();
    int getTopTimeFactor();
    int getRFSliderDivisor();

    void convertFromUIValues();
    void convertToUIValues();

    static float getFps(int fpsIndex);
    static int getNumberOfLines(int nbLinesIndex);
    static int getFpsIndex(float fps);
    static int getNumberOfLinesIndex(int nbLines);
    static float getNominalLineTime(int nbLinesIndex, int fpsIndex);

private:
    void lineTimeUpdate();
    void topTimeUpdate();

    float m_fltLineTimeMultiplier;
    float m_fltTopTimeMultiplier;
    int m_rfSliderDivisor;
};



#endif /* PLUGINS_CHANNELRX_DEMODATV_ATVDEMODSETTINGS_H_ */
