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

#include <dsp/basebandsamplesink.h>
#include <dsp/devicesamplesource.h>
#include <dsp/dspcommands.h>
#include <dsp/downchannelizer.h>
#include <QMutex>
#include <QElapsedTimer>
#include <vector>
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "atvscreen.h"


class ATVDemod : public BasebandSampleSink
{
	Q_OBJECT

public:

	enum ATVModulation {
	    ATV_FM1,
	    ATV_FM2,
        ATV_AM
	};

	struct ATVConfig
	{
	    int m_intSampleRate;
	    float m_fltLineDurationUs;
	    float m_fltTopDurationUs;
	    float m_fltFramePerS;
	    float m_fltRatioOfRowsToDisplay;
	    float m_fltVoltLevelSynchroTop;
	    float m_fltVoltLevelSynchroBlack;
	    ATVModulation m_enmModulation;
	    bool m_blnHSync;
	    bool m_blnVSync;

	    ATVConfig() :
	        m_intSampleRate(0),
	        m_fltLineDurationUs(0.0f),
	        m_fltTopDurationUs(0.0f),
	        m_fltFramePerS(0.0f),
	        m_fltRatioOfRowsToDisplay(0.0f),
	        m_fltVoltLevelSynchroTop(0.0f),
	        m_fltVoltLevelSynchroBlack(1.0f),
	        m_enmModulation(ATV_FM1),
	        m_blnHSync(false),
	        m_blnVSync(false)
	    {
	    }
	};

    ATVDemod();
	~ATVDemod();

    void configure(MessageQueue* objMessageQueue,
            float fltLineDurationUs,
            float fltTopDurationUs,
            float fltFramePerS,
            float fltRatioOfRowsToDisplay,
            float fltVoltLevelSynchroTop,
            float fltVoltLevelSynchroBlack,
            ATVModulation enmModulation,
            bool blnHSync,
            bool blnVSync);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    bool SetATVScreen(ATVScreen *objScreen);
    void InitATVParameters(int intMsps,
            float fltLineDurationUs,
            float fltTopDurationUs,
            float fltFramePerS,
            float fltRatioOfRowsToDisplay,
            bool blnHSync,
            bool blnVSync);
    int GetSampleRate();
    double getMagSq() const { return m_objMagSqAverage.average(); } //!< Beware this is scaled to 2^30

private:

    class MsgConfigureATVDemod : public Message
    {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgConfigureATVDemod* create(float fltLineDurationUs,
                    float fltTopDurationUs,
                    float fltFramePerS,
                    float fltRatioOfRowsToDisplay,
                    float fltVoltLevelSynchroTop,
                    float fltVoltLevelSynchroBlack,
                    ATVModulation enmModulation,
                    bool blnHSync,
                    bool blnVSync)
            {
                return new MsgConfigureATVDemod(fltLineDurationUs,
                        fltTopDurationUs,
                        fltFramePerS,
                        fltRatioOfRowsToDisplay,
                        fltVoltLevelSynchroTop,
                        fltVoltLevelSynchroBlack,
                        enmModulation,
                        blnHSync,
                        blnVSync);
            }

            ATVConfig m_objMsgConfig;

        private:
            MsgConfigureATVDemod(
                    float fltLineDurationUs,
                    float fltTopDurationUs,
                    float fltFramePerS,
                    float flatRatioOfRowsToDisplay,
                    float fltVoltLevelSynchroTop,
                    float fltVoltLevelSynchroBlack,
                    ATVModulation enmModulation,
                    bool blnHSync,
                    bool blnVSync) :
                Message()
            {
                m_objMsgConfig.m_enmModulation = enmModulation;
                m_objMsgConfig.m_fltVoltLevelSynchroBlack = fltVoltLevelSynchroBlack;
                m_objMsgConfig.m_fltVoltLevelSynchroTop = fltVoltLevelSynchroTop;
                m_objMsgConfig.m_fltFramePerS = fltFramePerS;
                m_objMsgConfig.m_fltLineDurationUs = fltLineDurationUs;
                m_objMsgConfig.m_fltTopDurationUs = fltTopDurationUs;
                m_objMsgConfig.m_fltRatioOfRowsToDisplay = flatRatioOfRowsToDisplay;
                m_objMsgConfig.m_blnHSync = blnHSync;
                m_objMsgConfig.m_blnVSync = blnVSync;
            }
    };

    //*************** ATV PARAMETERS  ***************
    ATVScreen * m_objRegisteredATVScreen;

    int m_intNumberSamplePerLine;
    int m_intNumberSamplePerTop;
    int m_intNumberOfLines;
    int m_intNumberOfRowsToDisplay;

    //*************** PROCESSING  ***************

    int m_intImageIndex;
    int m_intRowsLimit;
    int m_intSynchroPoints;

    bool m_blnSynchroDetected;
    bool m_blnLineSynchronized;
    bool m_blnImageDetecting;
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

    //*************** RF  ***************

    MovingAverage<double> m_objMagSqAverage;

    //QElapsedTimer m_objTimer;

    ATVConfig m_objRunning;
    ATVConfig m_objConfig;

    QMutex m_objSettingsMutex;

    static const float m_fltSecondToUs;

    void ApplySettings();

};

#endif // INCLUDE_ATVDEMOD_H
