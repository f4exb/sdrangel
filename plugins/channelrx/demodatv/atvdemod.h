///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#ifndef INCLUDE_ATVDEMOD_H
#define INCLUDE_ATVDEMOD_H

#include <QMutex>
#include <QElapsedTimer>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/fftfilt.h"
#include "dsp/agc.h"
#include "dsp/phaselock.h"
#include "dsp/recursivefilters.h"
#include "dsp/phasediscri.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "atvscreen.h"


class ATVDemod : public BasebandSampleSink
{
	Q_OBJECT

public:

    enum ATVStd
    {
        ATVStdPAL625,
        ATVStdPAL525,
        ATVStd405
    };

	enum ATVModulation {
	    ATV_FM1,  //!< Classical frequency modulation with discriminator #1
	    ATV_FM2,  //!< Classical frequency modulation with discriminator #2
	    ATV_FM3,  //!< Classical frequency modulation with phase derivative discriminator
        ATV_AM,   //!< Classical amplitude modulation
        ATV_USB,  //!< AM with vestigial lower side band (main signal is in the upper side)
        ATV_LSB   //!< AM with vestigial upper side band (main signal is in the lower side)
	};

	struct ATVConfig
	{
	    int m_intSampleRate;
	    ATVStd m_enmATVStandard;
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

	    ATVConfig() :
	        m_intSampleRate(0),
	        m_enmATVStandard(ATVStdPAL625),
	        m_fltLineDuration(0.0f),
	        m_fltTopDuration(0.0f),
	        m_fltFramePerS(0.0f),
	        m_fltRatioOfRowsToDisplay(0.0f),
	        m_fltVoltLevelSynchroTop(0.0f),
	        m_fltVoltLevelSynchroBlack(1.0f),
	        m_blnHSync(false),
	        m_blnVSync(false),
	        m_blnInvertVideo(false),
			m_intVideoTabIndex(0)
	    {
	    }
	};

    struct ATVRFConfig
    {
	    int64_t       m_intFrequencyOffset;
        ATVModulation m_enmModulation;
        float         m_fltRFBandwidth;
        float         m_fltRFOppBandwidth;
        bool          m_blnFFTFiltering;
        bool          m_blndecimatorEnable;
        float         m_fltBFOFrequency;
        float         m_fmDeviation;

        ATVRFConfig() :
            m_intFrequencyOffset(0),
            m_enmModulation(ATV_FM1),
            m_fltRFBandwidth(0),
            m_fltRFOppBandwidth(0),
            m_blnFFTFiltering(false),
            m_blndecimatorEnable(false),
            m_fltBFOFrequency(0.0f),
            m_fmDeviation(1.0f)
        {
        }
    };

    class MsgReportEffectiveSampleRate : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getNbPointsPerLine() const { return m_nbPointsPerLine; }

        static MsgReportEffectiveSampleRate* create(int sampleRate, int nbPointsPerLine)
        {
            return new MsgReportEffectiveSampleRate(sampleRate, nbPointsPerLine);
        }

    protected:
        int m_sampleRate;
        int m_nbPointsPerLine;

        MsgReportEffectiveSampleRate(int sampleRate, int nbPointsPerLine) :
            Message(),
            m_sampleRate(sampleRate),
            m_nbPointsPerLine(nbPointsPerLine)
        { }
    };

    ATVDemod(BasebandSampleSink* objScopeSink);
	~ATVDemod();

    void configure(MessageQueue* objMessageQueue,
            float fltLineDurationUs,
            float fltTopDurationUs,
            float fltFramePerS,
            ATVStd enmATVStandard,
            float fltRatioOfRowsToDisplay,
            float fltVoltLevelSynchroTop,
            float fltVoltLevelSynchroBlack,
            bool blnHSync,
            bool blnVSync,
            bool blnInvertVideo,
			int intVideoTabIndex);

    void configureRF(MessageQueue* objMessageQueue,
            ATVModulation enmModulation,
            float fltRFBandwidth,
            float fltRFOppBandwidth,
            bool blnFFTFiltering,
            bool blndecimatorEnable,
            float fltBFOFrequency,
            float fmDeviation);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    void setATVScreen(ATVScreen *objScreen);
    int getSampleRate();
    int getEffectiveSampleRate();
    double getMagSq() const { return m_objMagSqAverage.average(); } //!< Beware this is scaled to 2^30
    bool getBFOLocked();

private:
    struct ATVConfigPrivate
    {
        int m_intTVSampleRate;

        ATVConfigPrivate() :
            m_intTVSampleRate(0)
        {}
    };

    class MsgConfigureATVDemod : public Message
    {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgConfigureATVDemod* create(
                    float fltLineDurationUs,
                    float fltTopDurationUs,
                    float fltFramePerS,
                    ATVStd enmATVStandard,
                    float fltRatioOfRowsToDisplay,
                    float fltVoltLevelSynchroTop,
                    float fltVoltLevelSynchroBlack,
                    bool blnHSync,
                    bool blnVSync,
                    bool blnInvertVideo,
					int intVideoTabIndex)
            {
                return new MsgConfigureATVDemod(
                        fltLineDurationUs,
                        fltTopDurationUs,
                        fltFramePerS,
                        enmATVStandard,
                        fltRatioOfRowsToDisplay,
                        fltVoltLevelSynchroTop,
                        fltVoltLevelSynchroBlack,
                        blnHSync,
                        blnVSync,
                        blnInvertVideo,
						intVideoTabIndex);
            }

            ATVConfig m_objMsgConfig;

        private:
            MsgConfigureATVDemod(
                    float fltLineDurationUs,
                    float fltTopDurationUs,
                    float fltFramePerS,
                    ATVStd enmATVStandard,
                    float flatRatioOfRowsToDisplay,
                    float fltVoltLevelSynchroTop,
                    float fltVoltLevelSynchroBlack,
                    bool blnHSync,
                    bool blnVSync,
                    bool blnInvertVideo,
					int intVideoTabIndex) :
                Message()
            {
                m_objMsgConfig.m_fltVoltLevelSynchroBlack = fltVoltLevelSynchroBlack;
                m_objMsgConfig.m_fltVoltLevelSynchroTop = fltVoltLevelSynchroTop;
                m_objMsgConfig.m_fltFramePerS = fltFramePerS;
                m_objMsgConfig.m_enmATVStandard = enmATVStandard;
                m_objMsgConfig.m_fltLineDuration = fltLineDurationUs;
                m_objMsgConfig.m_fltTopDuration = fltTopDurationUs;
                m_objMsgConfig.m_fltRatioOfRowsToDisplay = flatRatioOfRowsToDisplay;
                m_objMsgConfig.m_blnHSync = blnHSync;
                m_objMsgConfig.m_blnVSync = blnVSync;
                m_objMsgConfig.m_blnInvertVideo = blnInvertVideo;
                m_objMsgConfig.m_intVideoTabIndex = intVideoTabIndex;
            }
    };

    class MsgConfigureRFATVDemod : public Message
    {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgConfigureRFATVDemod* create(
                    ATVModulation enmModulation,
                    float fltRFBandwidth,
                    float fltRFOppBandwidth,
                    bool blnFFTFiltering,
                    bool blndecimatorEnable,
                    int intBFOFrequency,
                    float fmDeviation)
            {
                return new MsgConfigureRFATVDemod(
                        enmModulation,
                        fltRFBandwidth,
                        fltRFOppBandwidth,
                        blnFFTFiltering,
                        blndecimatorEnable,
                        intBFOFrequency,
                        fmDeviation);
            }

            ATVRFConfig m_objMsgConfig;

        private:
            MsgConfigureRFATVDemod(
                    ATVModulation enmModulation,
                    float fltRFBandwidth,
                    float fltRFOppBandwidth,
                    bool blnFFTFiltering,
                    bool blndecimatorEnable,
                    float fltBFOFrequency,
                    float fmDeviation) :
                Message()
            {
                m_objMsgConfig.m_enmModulation = enmModulation;
                m_objMsgConfig.m_fltRFBandwidth = fltRFBandwidth;
                m_objMsgConfig.m_fltRFOppBandwidth = fltRFOppBandwidth;
                m_objMsgConfig.m_blnFFTFiltering = blnFFTFiltering;
                m_objMsgConfig.m_blndecimatorEnable = blndecimatorEnable;
                m_objMsgConfig.m_fltBFOFrequency = fltBFOFrequency;
                m_objMsgConfig.m_fmDeviation = fmDeviation;
            }
    };

    /**
     * Exponential average using integers and alpha as the inverse of a power of two
     */
    class AvgExpInt
    {
    public:
        AvgExpInt(int log2Alpha) : m_log2Alpha(log2Alpha), m_m1(0), m_start(true) {}
        void reset() { m_start = true; }

        int run(int m0)
        {
            if (m_start)
            {
                m_m1 = m0;
                m_start = false;
                return m0;
            }
            else
            {
                m_m1 = m0 + m_m1 - (m_m1>>m_log2Alpha);
                return m_m1>>m_log2Alpha;
            }
        }

    private:
        int m_log2Alpha;
        int m_m1;
        bool m_start;
    };

    //*************** SCOPE  ***************

    BasebandSampleSink* m_objScopeSink;
    SampleVector m_objScopeSampleBuffer;

    //*************** ATV PARAMETERS  ***************
    ATVScreen * m_objRegisteredATVScreen;

    int m_intNumberSamplePerLine;
    int m_intNumberSamplePerTop;
    int m_intNumberOfLines;
    int m_intNumberOfRowsToDisplay;
    int m_intNumberOfSyncLines;          //!< this is the number of non displayable lines at the start of a frame. First displayable row comes next.
    int m_intNumberOfBlackLines;         //!< this is the total number of lines not part of the image and is used for vertical screen size
    int m_intNumberSamplePerLineSignals; //!< number of samples in the non image part of the line (signals)
    int m_intNumberSaplesPerHSync;       //!< number of samples per horizontal synchronization pattern (pulse + back porch)

    //*************** PROCESSING  ***************

    int m_intImageIndex;
    int m_intRowsLimit;
    int m_intSynchroPoints;

    bool m_blnSynchroDetected;
    bool m_blnVerticalSynchroDetected;

    float m_fltAmpLineAverage;

    float m_fltEffMin;
    float m_fltEffMax;

    float m_fltAmpMin;
    float m_fltAmpMax;
    float m_fltAmpDelta;

    float m_fltBufferI[6];
    float m_fltBufferQ[6];

    int m_intColIndex;
    int m_intRowIndex;
    int m_intLineIndex;

    AvgExpInt m_objAvgColIndex;
    int m_intAvgColIndex;

    //*************** RF  ***************

    MovingAverage<double> m_objMagSqAverage;

    NCO m_nco;
    SimplePhaseLock m_bfoPLL;
    SecondOrderRecursiveFilter m_bfoFilter;

    // Interpolator group for decimation and/or double sideband RF filtering
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    // Used for vestigial SSB with asymmetrical filtering (needs double sideband scheme)
    fftfilt* m_DSBFilter;
    Complex* m_DSBFilterBuffer;
    int m_DSBFilterBufferIndex;
    static const int m_ssbFftLen;

    // Used for FM
    PhaseDiscriminators m_objPhaseDiscri;

    //QElapsedTimer m_objTimer;

    ATVConfig m_objRunning;
    ATVConfig m_objConfig;

    ATVRFConfig m_objRFRunning;
    ATVRFConfig m_objRFConfig;

    ATVConfigPrivate m_objRunningPrivate;
    ATVConfigPrivate m_objConfigPrivate;

    QMutex m_objSettingsMutex;

    void applySettings();
    void applyStandard();
    void demod(Complex& c);
    static float getRFBandwidthDivisor(ATVModulation modulation);
};

#endif // INCLUDE_ATVDEMOD_H
