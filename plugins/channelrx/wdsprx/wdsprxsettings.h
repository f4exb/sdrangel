///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef PLUGINS_CHANNELRX_WDSPRX_WDSPRXSETTINGS_H_
#define PLUGINS_CHANNELRX_WDSPRX_WDSPRXSETTINGS_H_

#include <array>

#include <QByteArray>
#include <QString>

#include "dsp/fftwindow.h"

class Serializable;


struct WDSPRxProfile
{
    enum WDSPRxDemod
    {
        DemodSSB,
        DemodAM,
        DemodSAM,
        DemodFMN,
    };
    enum WDSPRxAGCMode
    {
        AGCLong,
        AGCSlow,
        AGCMedium,
        AGCFast,
    };
    enum WDSPRxNRScheme
    {
        NRSchemeNR,
        NRSchemeNR2,
    };
    enum WDSPRxNBScheme
    {
        NBSchemeNB,     //!< Preemptive Wideband Blanker (ANB)
        NBSchemeNB2,    //!< Interpolating Wideband Blanker (NOB)
    };
    enum WDSPRxNR2Gain
    {
        NR2GainLinear,
        NR2GainLog,
        NR2GainGamma,
    };
    enum WDSPRxNR2NPE
    {
        NR2NPEOSMS,
        NR2NPEMMSE,
    };
    enum WDSPRxNRPosition
    {
        NRPositionPreAGC,
        NRPositionPostAGC,
    };
    enum WDSPRxNB2Mode
    {
        NB2ModeZero,
        NB2ModeSampleAndHold,
        NB2ModeMeanHold,
        NB2ModeHoldSample,
        NB2ModeInterpolate,
    };
    enum WDSPRxSquelchMode
    {
        SquelchModeVoice,
        SquelchModeAM,
        SquelchModeFM,
    };

    WDSPRxDemod m_demod;
    bool m_audioBinaural;
    bool m_audioFlipChannels;
    double m_audioPan;
    bool m_dsb;
    bool m_dbOrS;
    // Filter
    int   m_spanLog2;
    Real  m_highCutoff;
    Real  m_lowCutoff;
    int   m_fftWindow; // 0: 4-term Blackman-Harris, 1: 7-term Blackman-Harris
    // AGC
    bool  m_agc;
    WDSPRxAGCMode m_agcMode;
    int   m_agcGain;    //!< Fixed gain if AGC is off else top gain
    int   m_agcSlope;
    int   m_agcHangThreshold;
    // Noise blanker
    bool m_dnb;
    WDSPRxNBScheme m_nbScheme;
    WDSPRxNB2Mode m_nb2Mode;
    double m_nbSlewTime;  // a.k.a tau
    double m_nbLeadTime;  // a.k.a adv time
    double m_nbLagTime;   // a.k.a hang time
    int m_nbThreshold;
    double m_nbAvgTime;   // a.k.a back tau
    // Noise rediction
    bool m_dnr;
    bool m_snb;
    bool m_anf;
    WDSPRxNRScheme m_nrScheme;
    WDSPRxNR2Gain m_nr2Gain;
    WDSPRxNR2NPE m_nr2NPE;
    WDSPRxNRPosition m_nrPosition;
    bool m_nr2ArtifactReduction;
    // Demods
    bool m_amFadeLevel;
    bool m_cwPeaking;
    double m_cwPeakFrequency;
    double m_cwBandwidth;
    double m_cwGain;
    double m_fmDeviation;
    double m_fmAFLow;
    double m_fmAFHigh;
    bool m_fmAFLimiter;
    double m_fmAFLimiterGain;
    bool m_fmCTCSSNotch;
    double m_fmCTCSSNotchFrequency;
    // Squelch
    bool m_squelch;
    int m_squelchThreshold;
    WDSPRxSquelchMode m_squelchMode;
    double m_ssqlTauMute;   //!< Voice squelch tau mute
    double m_ssqlTauUnmute; //!< Voice squelch tau unmute
    double m_amsqMaxTail;
    // Equalizer
    bool m_equalizer;
    std::array<float, 11> m_eqF; //!< Frequencies vector. Index 0 is always 0 as this is the preamp position
    std::array<float, 11> m_eqG; //!< Gains vector (dB). Index 0 is the preamp (common) gain
    // RIT
    bool m_rit;
    double m_ritFrequency;

    WDSPRxProfile() :
        m_demod(DemodSSB),
        m_audioBinaural(false),
        m_audioFlipChannels(false),
        m_audioPan(0.5),
        m_dsb(false),
        m_dbOrS(true),
        m_spanLog2(3),
        m_highCutoff(3000),
        m_lowCutoff(300),
        m_fftWindow(0),
        m_agc(false),
        m_agcMode(AGCMedium),
        m_agcGain(80),
        m_agcSlope(35),
        m_agcHangThreshold(0),
        m_dnb(false),
        m_nbScheme(NBSchemeNB),
        m_nb2Mode(NB2ModeZero),
        m_nbSlewTime(0.1),
        m_nbLeadTime(0.1),
        m_nbLagTime(0.1),
        m_nbThreshold(30),
        m_nbAvgTime(50.0),
        m_dnr(false),
        m_snb(false),
        m_anf(false),
        m_nrScheme(NRSchemeNR),
        m_nr2Gain(NR2GainGamma),
        m_nr2NPE(NR2NPEOSMS),
        m_nrPosition(NRPositionPreAGC),
        m_nr2ArtifactReduction(true),
        m_amFadeLevel(false),
        m_cwPeaking(false),
        m_cwPeakFrequency(600.0),
        m_cwBandwidth(100.0),
        m_cwGain(2.0),
        m_fmDeviation(2500.0),
        m_fmAFLow(300.0),
        m_fmAFHigh(3000.0),
        m_fmAFLimiter(true),
        m_fmAFLimiterGain(-40.0),
        m_fmCTCSSNotch(false),
        m_fmCTCSSNotchFrequency(67.0),
        m_squelch(false),
        m_squelchThreshold(3),
        m_squelchMode(SquelchModeVoice),
        m_ssqlTauMute(0.1),
        m_ssqlTauUnmute(0.1),
        m_amsqMaxTail(1.5),
        m_equalizer(false),
        m_eqF{0.0, 32.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0},
        m_eqG{0.0,  0.0,  0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0},
        m_rit(false),
        m_ritFrequency(0)
    {}
};

struct WDSPRxSettings
{
    WDSPRxProfile::WDSPRxDemod m_demod;
    qint32 m_inputFrequencyOffset;
    // Real m_highCutoff;
    // Real m_lowCutoff;
    Real m_volume;
    // int  m_spanLog2;
    bool m_audioBinaural;
    bool m_audioFlipChannels;
    double m_audioPan;
    bool m_dsb;
    bool m_audioMute;
    bool m_dbOrS;
    // AGC
    bool m_agc;
    WDSPRxProfile::WDSPRxAGCMode m_agcMode;
    int  m_agcGain;    //!< Fixed gain if AGC is off else top gain
    int  m_agcSlope;
    int  m_agcHangThreshold;
    // Noise blanker
    bool m_dnb;
    WDSPRxProfile::WDSPRxNBScheme m_nbScheme;
    WDSPRxProfile::WDSPRxNB2Mode m_nb2Mode;
    double m_nbSlewTime;
    double m_nbLeadTime;
    double m_nbLagTime;
    int m_nbThreshold;
    double m_nbAvgTime;
    // Noise reduction
    bool m_dnr;
    bool m_snb;
    bool m_anf;
    WDSPRxProfile::WDSPRxNRScheme m_nrScheme;
    WDSPRxProfile::WDSPRxNR2Gain m_nr2Gain;
    WDSPRxProfile::WDSPRxNR2NPE m_nr2NPE;
    WDSPRxProfile::WDSPRxNRPosition m_nrPosition;
    bool m_nr2ArtifactReduction;
    // Demods
    bool m_amFadeLevel;
    bool m_cwPeaking;
    double m_cwPeakFrequency;
    double m_cwBandwidth;
    double m_cwGain;
    double m_fmDeviation;
    double m_fmAFLow;
    double m_fmAFHigh;
    bool m_fmAFLimiter;
    double m_fmAFLimiterGain;
    bool m_fmCTCSSNotch;
    double m_fmCTCSSNotchFrequency;
    // Squelch
    bool m_squelch;
    int m_squelchThreshold;
    WDSPRxProfile::WDSPRxSquelchMode m_squelchMode;
    double m_ssqlTauMute;   //!< Voice squelch tau mute
    double m_ssqlTauUnmute; //!< Voice squelch tau unmute
    double m_amsqMaxTail;
    // Equalizer
    bool m_equalizer;
    std::array<float, 11> m_eqF = {0.0, 32.0, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0}; //!< Frequencies vector. Index 0 is always 0 as this is the preamp position
    std::array<float, 11> m_eqG = {0.0,  0.0,  0.0,   0.0,   0.0,   0.0,    0.0,    0.0,    0.0,    0.0,     0.0}; //!< Gains vector (dB). Index 0 is the preamp (common) gain
    // RIT
    bool m_rit;
    double m_ritFrequency;

    quint32 m_rgbColor;
    QString m_title;
    QString m_audioDeviceName;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    std::vector<WDSPRxProfile> m_profiles;
    unsigned int m_profileIndex;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;
    Serializable *m_rollupState;

    WDSPRxSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static const int m_minPowerThresholdDB;
    static const float m_mminPowerThresholdDBf;
};


#endif /* PLUGINS_CHANNELRX_WDSPRX_WDSPRXSETTINGS_H_ */
