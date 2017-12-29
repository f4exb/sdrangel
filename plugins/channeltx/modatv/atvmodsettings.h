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

#ifndef PLUGINS_CHANNELTX_MODATV_ATVMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODATV_ATVMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

struct Serializable;

struct ATVModSettings
{
    typedef enum
    {
        ATVStdPAL625,
        ATVStdPAL525,
        ATVStd405,
        ATVStdShortInterleaved,
        ATVStdShort,
        ATVStdHSkip,
    } ATVStd;

    typedef enum
    {
        ATVModInputUniform,
        ATVModInputHBars,
        ATVModInputVBars,
        ATVModInputChessboard,
        ATVModInputHGradient,
        ATVModInputVGradient,
        ATVModInputImage,
        ATVModInputVideo,
        ATVModInputCamera
    } ATVModInput;

    typedef enum
    {
        ATVModulationAM,
        ATVModulationFM,
        ATVModulationUSB,
        ATVModulationLSB,
        ATVModulationVestigialUSB,
        ATVModulationVestigialLSB
    } ATVModulation;

    qint64        m_inputFrequencyOffset; //!< offset from baseband center frequency
    Real          m_rfBandwidth;          //!< Bandwidth of modulated signal or direct sideband for SSB / vestigial SSB
    Real          m_rfOppBandwidth;       //!< Bandwidth of opposite sideband for vestigial SSB
    ATVStd        m_atvStd;               //!< Standard
    int           m_nbLines;              //!< Number of lines per full frame
    int           m_fps;                  //!< Number of frames per second
    ATVModInput   m_atvModInput;          //!< Input source type
    Real          m_uniformLevel;         //!< Percentage between black and white for uniform screen display
    ATVModulation m_atvModulation;        //!< RF modulation type
    bool          m_videoPlayLoop;        //!< Play video in a loop
    bool          m_videoPlay;            //!< True to play video and false to pause
    bool          m_cameraPlay;           //!< True to play camera video and false to pause
    bool          m_channelMute;          //!< Mute channel baseband output
    bool          m_invertedVideo;        //!< True if video signal is inverted before modulation
    float         m_rfScalingFactor;      //!< Scaling factor from +/-1 to +/-2^15
    float         m_fmExcursion;          //!< FM excursion factor relative to full bandwidth
    bool          m_forceDecimator;       //!< Forces decimator even when channel and source sample rates are equal
    QString       m_overlayText;
    quint32       m_rgbColor;
    QString       m_title;

    QString m_udpAddress;
    uint16_t m_udpPort;

    Serializable *m_channelMarker;

    ATVModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};

#endif /* PLUGINS_CHANNELTX_MODATV_ATVMODSETTINGS_H_ */
