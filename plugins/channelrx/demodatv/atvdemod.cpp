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

#include "atvdemod.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/pidcontroller.h"

MESSAGE_CLASS_DEFINITION(ATVDemod::MsgConfigureATVDemod, Message)
MESSAGE_CLASS_DEFINITION(ATVDemod::MsgConfigureRFATVDemod, Message)
MESSAGE_CLASS_DEFINITION(ATVDemod::MsgReportEffectiveSampleRate, Message)

const int ATVDemod::m_ssbFftLen = 1024;

ATVDemod::ATVDemod(BasebandSampleSink* objScopeSink) :
    m_objScopeSink(objScopeSink),
    m_objSettingsMutex(QMutex::Recursive),
    m_objRegisteredATVScreen(NULL),
    m_intImageIndex(0),
    m_intColIndex(0),
    m_intRowIndex(0),
    m_intSynchroPoints(0),
    m_blnSynchroDetected(false),
    m_blnVerticalSynchroDetected(false),
    m_intRowsLimit(0),
    m_fltEffMin(2000000000.0f),
    m_fltEffMax(-2000000000.0f),
    m_fltAmpMin(-2000000000.0f),
    m_fltAmpMax(2000000000.0f),
    m_fltAmpDelta(1.0),
    m_fltAmpLineAverage(0.0f),
    m_intNumberSamplePerTop(0),
    m_bfoPLL(200/1000000, 100/1000000, 0.01),
    m_bfoFilter(200.0, 1000000.0, 0.9),
    m_interpolatorDistanceRemain(0.0f),
    m_interpolatorDistance(1.0f),
    m_DSBFilter(0),
    m_DSBFilterBuffer(0),
    m_DSBFilterBufferIndex(0),
    m_objAvgColIndex(2)
{
    setObjectName("ATVDemod");

    //*************** ATV PARAMETERS  ***************
    m_intNumberSamplePerLine=0;
    m_intSynchroPoints=0;
    m_intNumberOfLines=0;
    m_intNumberOfRowsToDisplay=0;

    m_objMagSqAverage.resize(32, 1.0);

    m_DSBFilter = new fftfilt((2.0f * m_objRFConfig.m_fltRFBandwidth) / 1000000, 2 * m_ssbFftLen); // arbitrary 1 MS/s sample rate
    m_DSBFilterBuffer = new Complex[m_ssbFftLen];
    memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));

    memset((void*)m_fltBufferI,0,6*sizeof(float));
    memset((void*)m_fltBufferQ,0,6*sizeof(float));

    m_objPhaseDiscri.setFMScaling(1.0f);

    applyStandard();
}

ATVDemod::~ATVDemod()
{
}

void ATVDemod::setATVScreen(ATVScreen *objScreen)
{
    m_objRegisteredATVScreen = objScreen;
}

void ATVDemod::configure(
        MessageQueue* objMessageQueue,
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
    Message* msgCmd = MsgConfigureATVDemod::create(
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
    objMessageQueue->push(msgCmd);
}

void ATVDemod::configureRF(
        MessageQueue* objMessageQueue,
        ATVModulation enmModulation,
        float fltRFBandwidth,
        float fltRFOppBandwidth,
        bool blnFFTFiltering,
        bool blnDecimatorEnable,
        float fltBFOFrequency,
        float fmDeviation)
{
    Message* msgCmd = MsgConfigureRFATVDemod::create(
            enmModulation,
            fltRFBandwidth,
            fltRFOppBandwidth,
            blnFFTFiltering,
            blnDecimatorEnable,
            fltBFOFrequency,
            fmDeviation);
    objMessageQueue->push(msgCmd);
}

void ATVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    float fltDivSynchroBlack = 1.0f - m_objRunning.m_fltVoltLevelSynchroBlack;
    float fltI;
    float fltQ;
    float fltNormI;
    float fltNormQ;
    Complex ci;

    float fltNorm=0.00f;
    float fltVal;
    int intVal;

    qint16 * ptrBufferToRelease = 0;

    bool blnComputeImage=false;

    int intSynchroTimeSamples= (3*m_intNumberSamplePerLine)/4;
    float fltSynchroTrameLevel =  0.5f*((float)intSynchroTimeSamples) * m_objRunning.m_fltVoltLevelSynchroBlack;

    //********** Let's rock and roll buddy ! **********

    m_objSettingsMutex.lock();

    //********** Accessing ATV Screen context **********

#ifdef EXTENDED_DIRECT_SAMPLE

    qint16 * ptrBuffer;
    qint32 intLen;

    //********** Reading direct samples **********

    SampleVector::const_iterator it = begin;
    intLen = it->intLen;
    ptrBuffer = it->ptrBuffer;
    ptrBufferToRelease = ptrBuffer;
    ++it;

    for(qint32 intInd=0; intInd<intLen-1; intInd +=2)
    {

        fltI= ((qint32) (*ptrBuffer)) << 4;
        ptrBuffer ++;
        fltQ= ((qint32) (*ptrBuffer)) << 4;
        ptrBuffer ++;

#else

    for (SampleVector::const_iterator it = begin; it != end; ++it /* ++it **/)
    {

        fltI = it->real();
        fltQ = it->imag();
#endif
        Complex c(fltI, fltQ);

        if (m_objRFRunning.m_intFrequencyOffset != 0)
        {
            c *= m_nco.nextIQ();
        }

        if (m_objRFRunning.m_blndecimatorEnable)
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                demod(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else
        {
            demod(c);
        }
    }

    if ((m_objRunning.m_intVideoTabIndex == 1) && (m_objScopeSink != 0)) // do only if scope tab is selected and scope is available
    {
        m_objScopeSink->feed(m_objScopeSampleBuffer.begin(), m_objScopeSampleBuffer.end(), false); // m_ssb = positive only
    }

    m_objScopeSampleBuffer.clear();

    if (ptrBufferToRelease != 0)
    {
        delete ptrBufferToRelease;
    }

    m_objSettingsMutex.unlock();
}

void ATVDemod::demod(Complex& c)
{
    float fltDivSynchroBlack = 1.0f - m_objRunning.m_fltVoltLevelSynchroBlack;
    int intSynchroTimeSamples= (3*m_intNumberSamplePerLine)/4;
    float fltSynchroTrameLevel =  0.5f*((float)intSynchroTimeSamples) * m_objRunning.m_fltVoltLevelSynchroBlack;
    float fltNormI;
    float fltNormQ;
    float fltNorm;
    float fltVal;
    int intVal;

    //********** FFT filtering **********

    if (m_objRFRunning.m_blnFFTFiltering)
    {
        int n_out;
        fftfilt::cmplx *filtered;

        n_out = m_DSBFilter->runAsym(c, &filtered, m_objRFRunning.m_enmModulation != ATV_LSB); // all usb except explicitely lsb

        if (n_out > 0)
        {
            memcpy((void *) m_DSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
            m_DSBFilterBufferIndex = 0;
        }

        m_DSBFilterBufferIndex++;
    }

    //********** demodulation **********

#if defined(_WINDOWS_)
    float fltI = m_objRFRunning.m_blnFFTFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex-1].real() : c.real();
    float fltQ = m_objRFRunning.m_blnFFTFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex-1].imag() : c.imag();
#else
    float& fltI = m_objRFRunning.m_blnFFTFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex-1].real() : c.real();
    float& fltQ = m_objRFRunning.m_blnFFTFiltering ? m_DSBFilterBuffer[m_DSBFilterBufferIndex-1].imag() : c.imag();
#endif
    double magSq;

    if ((m_objRFRunning.m_enmModulation == ATV_FM1) || (m_objRFRunning.m_enmModulation == ATV_FM2))
    {
        //Amplitude FM
        magSq = fltI*fltI + fltQ*fltQ;
        m_objMagSqAverage.feed(magSq);
        fltNorm = sqrt(magSq);
        fltNormI= fltI/fltNorm;
        fltNormQ= fltQ/fltNorm;

        //-2 > 2 : 0 -> 1 volt
        //0->0.3 synchro  0.3->1 image

        if (m_objRFRunning.m_enmModulation == ATV_FM1)
        {
            //YDiff Cd
            fltVal = m_fltBufferI[0]*(fltNormQ - m_fltBufferQ[1]);
            fltVal -= m_fltBufferQ[0]*(fltNormI - m_fltBufferI[1]);

            fltVal += 2.0f;
            fltVal /= 4.0f;

        }
        else
        {
            //YDiff Folded
            fltVal =  m_fltBufferI[2]*((m_fltBufferQ[5]-fltNormQ)/16.0f + m_fltBufferQ[1] - m_fltBufferQ[3]);
            fltVal -= m_fltBufferQ[2]*((m_fltBufferI[5]-fltNormI)/16.0f + m_fltBufferI[1] - m_fltBufferI[3]);

            fltVal += 2.125f;
            fltVal /= 4.25f;

            m_fltBufferI[5]=m_fltBufferI[4];
            m_fltBufferQ[5]=m_fltBufferQ[4];

            m_fltBufferI[4]=m_fltBufferI[3];
            m_fltBufferQ[4]=m_fltBufferQ[3];

            m_fltBufferI[3]=m_fltBufferI[2];
            m_fltBufferQ[3]=m_fltBufferQ[2];

            m_fltBufferI[2]=m_fltBufferI[1];
            m_fltBufferQ[2]=m_fltBufferQ[1];
        }

        m_fltBufferI[1]=m_fltBufferI[0];
        m_fltBufferQ[1]=m_fltBufferQ[0];

        m_fltBufferI[0]=fltNormI;
        m_fltBufferQ[0]=fltNormQ;

        if (m_objRFRunning.m_fmDeviation != 1.0f)
        {
            fltVal = ((fltVal - 0.5f) / m_objRFRunning.m_fmDeviation) + 0.5f;
        }
    }
    else if (m_objRFRunning.m_enmModulation == ATV_AM)
    {
        //Amplitude AM
        magSq = fltI*fltI + fltQ*fltQ;
        m_objMagSqAverage.feed(magSq);
        fltNorm = sqrt(magSq);
        fltVal = fltNorm;

        //********** Mini and Maxi Amplitude tracking **********

        if(fltVal<m_fltEffMin)
        {
            m_fltEffMin=fltVal;
        }

        if(fltVal>m_fltEffMax)
        {
            m_fltEffMax=fltVal;
        }

        //Normalisation
        fltVal -= m_fltAmpMin;
        fltVal /=m_fltAmpDelta;
    }
    else if ((m_objRFRunning.m_enmModulation == ATV_USB) || (m_objRFRunning.m_enmModulation == ATV_LSB))
    {
        magSq = fltI*fltI + fltQ*fltQ;
        m_objMagSqAverage.feed(magSq);
        fltNorm = sqrt(magSq);

        Real bfoValues[2];
        float fltFiltered = m_bfoFilter.run(fltI);
        m_bfoPLL.process(fltFiltered, bfoValues);

        // do the mix

        float mixI = fltI * bfoValues[0] - fltQ * bfoValues[1];
        float mixQ = fltI * bfoValues[1] + fltQ * bfoValues[0];

        if (m_objRFRunning.m_enmModulation == ATV_USB) {
            fltVal = (mixI + mixQ);
        } else {
            fltVal = (mixI - mixQ);
        }

        //********** Mini and Maxi Amplitude tracking **********

        if(fltVal<m_fltEffMin)
        {
            m_fltEffMin=fltVal;
        }

        if(fltVal>m_fltEffMax)
        {
            m_fltEffMax=fltVal;
        }

        //Normalisation
        fltVal -= m_fltAmpMin;
        fltVal /=m_fltAmpDelta;
    }
    else if (m_objRFRunning.m_enmModulation == ATV_FM3)
    {
        float rawDeviation;
        fltVal = m_objPhaseDiscri.phaseDiscriminatorDelta(c, magSq, rawDeviation) + 0.5f;
        //fltVal = fltVal < 0.0f ? 0.0f : fltVal > 1.0f ? 1.0f : fltVal;
        m_objMagSqAverage.feed(magSq);
        fltNorm = sqrt(magSq);
    }
    else
    {
        magSq = fltI*fltI + fltQ*fltQ;
        m_objMagSqAverage.feed(magSq);
        fltNorm = sqrt(magSq);
        fltVal = 0.0f;
    }

    m_objScopeSampleBuffer.push_back(Sample(fltVal*32767.0f, 0.0f));

    fltVal = m_objRunning.m_blnInvertVideo ? 1.0f - fltVal : fltVal;

    m_fltAmpLineAverage += fltVal;

    //********** gray level **********
    //-0.3 -> 0.7
    intVal = (int) 255.0*(fltVal - m_objRunning.m_fltVoltLevelSynchroBlack) / fltDivSynchroBlack;

    //0 -> 255
    if(intVal<0)
    {
        intVal=0;
    }
    else if(intVal>255)
    {
        intVal=255;
    }

    //********** Filling pixels **********

    bool blnComputeImage = (m_objRunning.m_fltRatioOfRowsToDisplay != 0.5f); // TODO: review this

    if (!blnComputeImage)
    {
        blnComputeImage = ((m_intImageIndex/2) % 2 == 0);
    }

    if (blnComputeImage)
    {
        m_objRegisteredATVScreen->setDataColor(m_intColIndex - m_intNumberSaplesPerHSync + m_intNumberSamplePerTop, intVal, intVal, intVal);
    }

    m_intColIndex++;

    //////////////////////

    // Horizontal Synchro detection

    // Floor Detection 0
    if (fltVal < m_objRunning.m_fltVoltLevelSynchroTop)
    {
        m_intSynchroPoints++;
    }
    // Black detection 0.3
    else if (fltVal > m_objRunning.m_fltVoltLevelSynchroBlack)
    {
        m_intSynchroPoints = 0;
    }

    m_blnSynchroDetected = (m_intSynchroPoints == m_intNumberSamplePerTop);

    //********** Rendering if necessary **********

    // Vertical Synchro : 3/4 a line necessary
    if(!m_blnVerticalSynchroDetected && m_objRunning.m_blnVSync)
    {
       if(m_intColIndex >= intSynchroTimeSamples)
       {
            if(m_fltAmpLineAverage<=fltSynchroTrameLevel) //(m_fltLevelSynchroBlack*(float)(m_intColIndex-((m_intNumberSamplePerLine*12)/64)))) //75
            {
                m_blnVerticalSynchroDetected=true;

                m_intRowIndex=m_intImageIndex%2;

                if(blnComputeImage)
                {
                    m_objRegisteredATVScreen->selectRow(m_intRowIndex - m_intNumberOfSyncLines);
                }
            }
        }
    }

    //Horizontal Synchro processing

    bool blnNewLine = false;

    if (!m_objRunning.m_blnHSync && (m_intColIndex >= m_intNumberSamplePerLine)) // H Sync not active
    {
        m_intColIndex = 0;
        blnNewLine = true;
    }
    else if (m_blnSynchroDetected // Valid H sync detected
    //&& (m_intRowIndex > m_intNumberOfSyncLines) // FIXME: remove ?
    && (m_intColIndex > m_intNumberSamplePerLine - m_intNumberSamplePerTop)
    && (m_intColIndex < m_intNumberSamplePerLine + m_intNumberSamplePerTop))
    {
        m_intAvgColIndex = m_objAvgColIndex.run(m_intColIndex);
        m_intColIndex = m_intColIndex - m_intAvgColIndex;
        blnNewLine = true;
    }
    else if (m_intColIndex >= m_intNumberSamplePerLine + 2) // No valid H sync
    {
        m_intColIndex = 0;
        blnNewLine = true;
    }

    if (blnNewLine)
    {
        m_fltAmpLineAverage=0.0f;

        //New line + Interleaving
        m_intRowIndex ++;
        m_intRowIndex ++;

        if(m_intRowIndex<m_intNumberOfLines)
        {
            m_objRegisteredATVScreen->selectRow(m_intRowIndex - m_intNumberOfSyncLines);
        }
    }

    //////////////////////

    if(m_intRowIndex>=m_intRowsLimit)
    {

         m_blnVerticalSynchroDetected=false;

         m_fltAmpLineAverage=0.0f;

        //Interleave Odd/Even images
        m_intRowIndex=m_intImageIndex%2;
        m_intColIndex=0;

        if(blnComputeImage)
        {
            m_objRegisteredATVScreen->selectRow(m_intRowIndex - m_intNumberOfSyncLines);
        }

        //Rendering when odd image processed
        if(m_intImageIndex%2==1)
        {
            //interleave
            if(blnComputeImage)
            {
                m_objRegisteredATVScreen->renderImage(NULL);
            }

            if (m_objRFRunning.m_enmModulation == ATV_AM)
            {
                m_fltAmpMin=m_fltEffMin;
                m_fltAmpMax=m_fltEffMax;
                m_fltAmpDelta=m_fltEffMax-m_fltEffMin;

                if(m_fltAmpDelta<=0.0)
                {
                    m_fltAmpDelta=1.0f;
                }

                //Reset extrema
                m_fltEffMin=2000000.0f;
                m_fltEffMax=-2000000.0f;
            }

            m_intRowsLimit = m_intNumberOfLines - 1; // odd image
        }
        else
        {
            m_intRowsLimit = m_intNumberOfLines % 2 == 1 ? m_intNumberOfLines : m_intNumberOfLines-2; // even image
        }

        m_intImageIndex ++;
    }
}

void ATVDemod::start()
{
    //m_objTimer.start();
}

void ATVDemod::stop()
{
}

bool ATVDemod::handleMessage(const Message& cmd)
{
    qDebug() << "ATVDemod::handleMessage";

    if (DownChannelizer::MsgChannelizerNotification::match(cmd))
    {
        DownChannelizer::MsgChannelizerNotification& objNotif = (DownChannelizer::MsgChannelizerNotification&) cmd;
        m_objConfig.m_intSampleRate = objNotif.getSampleRate();
        m_objRFConfig.m_intFrequencyOffset = objNotif.getFrequencyOffset();

        qDebug() << "ATVDemod::handleMessage: MsgChannelizerNotification:"
                << " m_intSampleRate: " << m_objConfig.m_intSampleRate
                << " m_intFrequencyOffset: " << m_objRFConfig.m_intFrequencyOffset;

        applySettings();

        return true;
    }
    else if (MsgConfigureATVDemod::match(cmd))
    {
        MsgConfigureATVDemod& objCfg = (MsgConfigureATVDemod&) cmd;

        m_objConfig = objCfg.m_objMsgConfig;

        qDebug()  << "ATVDemod::handleMessage: MsgConfigureATVDemod:"
                << " m_fltVoltLevelSynchroBlack:" << m_objConfig.m_fltVoltLevelSynchroBlack
                << " m_fltVoltLevelSynchroTop:" << m_objConfig.m_fltVoltLevelSynchroTop
                << " m_fltFramePerS:" << m_objConfig.m_fltFramePerS
                << " m_fltLineDurationUs:" << m_objConfig.m_fltLineDuration
                << " m_fltRatioOfRowsToDisplay:" << m_objConfig.m_fltRatioOfRowsToDisplay
                << " m_fltTopDurationUs:" << m_objConfig.m_fltTopDuration
                << " m_blnHSync:" << m_objConfig.m_blnHSync
                << " m_blnVSync:" << m_objConfig.m_blnVSync;

        applySettings();

        return true;
    }
    else if (MsgConfigureRFATVDemod::match(cmd))
    {
        MsgConfigureRFATVDemod& objCfg = (MsgConfigureRFATVDemod&) cmd;

        m_objRFConfig = objCfg.m_objMsgConfig;

        qDebug()  << "ATVDemod::handleMessage: MsgConfigureRFATVDemod:"
                << " m_enmModulation:" << m_objRFConfig.m_enmModulation
                << " m_fltRFBandwidth:" << m_objRFConfig.m_fltRFBandwidth
                << " m_fltRFOppBandwidth:" << m_objRFConfig.m_fltRFOppBandwidth
                << " m_blnFFTFiltering:" << m_objRFConfig.m_blnFFTFiltering
                << " m_blnDecimatorEnable:" << m_objRFConfig.m_blndecimatorEnable
                << " m_fltBFOFrequency:" << m_objRFConfig.m_fltBFOFrequency
                << " m_fmDeviation:" << m_objRFConfig.m_fmDeviation;

        applySettings();

        return true;
    }
    else
    {
        if (m_objScopeSink != 0)
        {
           return m_objScopeSink->handleMessage(cmd);
        }
        else
        {
            return false;
        }
    }
}

void ATVDemod::applySettings()
{

    if (m_objConfig.m_intSampleRate == 0)
    {
        return;
    }

    if((m_objRFConfig.m_intFrequencyOffset != m_objRFRunning.m_intFrequencyOffset)
       || (m_objRFConfig.m_enmModulation != m_objRFRunning.m_enmModulation)
       || (m_objConfig.m_intSampleRate != m_objRunning.m_intSampleRate))
    {
        m_nco.setFreq(-m_objRFConfig.m_intFrequencyOffset, m_objConfig.m_intSampleRate);
    }

    if ((m_objConfig.m_intSampleRate != m_objRunning.m_intSampleRate)
        || (m_objRFConfig.m_fltRFBandwidth != m_objRFRunning.m_fltRFBandwidth))
    {
        m_objSettingsMutex.lock();

        m_objConfigPrivate.m_intTVSampleRate = (m_objConfig.m_intSampleRate / 500000) * 500000; // make sure working sample rate is a multiple of rate units

        if (m_objConfigPrivate.m_intTVSampleRate > 0)
        {
            m_interpolatorDistance = (Real) m_objConfigPrivate.m_intTVSampleRate / (Real) m_objConfig.m_intSampleRate;
        }
        else
        {
            m_objConfigPrivate.m_intTVSampleRate = m_objConfig.m_intSampleRate;
            m_interpolatorDistance = 1.0f;
        }

        m_interpolatorDistanceRemain = 0;
        m_interpolator.create(24,
                m_objConfigPrivate.m_intTVSampleRate,
                m_objRFConfig.m_fltRFBandwidth / getRFBandwidthDivisor(m_objRFConfig.m_enmModulation),
                3.0);
        m_objSettingsMutex.unlock();
    }

    if((m_objConfig.m_fltFramePerS != m_objRunning.m_fltFramePerS)
       || (m_objConfig.m_fltLineDuration != m_objRunning.m_fltLineDuration)
       || (m_objConfig.m_intSampleRate != m_objRunning.m_intSampleRate)
       || (m_objConfig.m_fltTopDuration != m_objRunning.m_fltTopDuration)
       || (m_objConfig.m_fltRatioOfRowsToDisplay != m_objRunning.m_fltRatioOfRowsToDisplay)
       || (m_objConfig.m_enmATVStandard != m_objRunning.m_enmATVStandard))
    {
        m_objSettingsMutex.lock();

        m_intNumberOfLines = (int) (1.0f / (m_objConfig.m_fltLineDuration * m_objConfig.m_fltFramePerS));
        m_intNumberSamplePerLine = (int) (m_objConfig.m_fltLineDuration * m_objConfig.m_intSampleRate);
        m_intNumberOfRowsToDisplay = (int) (m_objConfig.m_fltRatioOfRowsToDisplay * m_objConfig.m_fltLineDuration * m_objConfig.m_intSampleRate);
        m_intNumberSamplePerTop = (int) (m_objConfig.m_fltTopDuration * m_objConfig.m_intSampleRate);
        m_intRowsLimit = m_intNumberOfLines % 2 == 1 ? m_intNumberOfLines : m_intNumberOfLines-2; // start with even image

        applyStandard();

        m_objRegisteredATVScreen->resizeATVScreen(
                m_intNumberSamplePerLine - m_intNumberSamplePerLineSignals,
                m_intNumberOfLines - m_intNumberOfBlackLines);

        qDebug() << "ATVDemod::applySettings:"
                << " m_fltLineDuration: " << m_objConfig.m_fltLineDuration
                << " m_fltFramePerS: " << m_objConfig.m_fltFramePerS
                << " m_intNumberOfLines: " << m_intNumberOfLines
                << " m_intNumberSamplePerLine: " << m_intNumberSamplePerLine
                << " m_intNumberOfRowsToDisplay: " << m_intNumberOfRowsToDisplay
                << " m_intNumberOfBlackLines: " << m_intNumberOfBlackLines;

        m_intRowsLimit = m_intNumberOfLines-1;
        m_intImageIndex = 0;
        m_intColIndex=0;
        m_intRowIndex=0;
        m_intRowsLimit=0;

        m_objSettingsMutex.unlock();

        int sampleRate = m_objRFConfig.m_blndecimatorEnable ? m_objConfigPrivate.m_intTVSampleRate : m_objConfig.m_intSampleRate;
        MsgReportEffectiveSampleRate *report;
        report = MsgReportEffectiveSampleRate::create(sampleRate, m_intNumberSamplePerLine);
        getOutputMessageQueue()->push(report);
    }

    if ((m_objConfigPrivate.m_intTVSampleRate != m_objRunningPrivate.m_intTVSampleRate)
        || (m_objConfig.m_intSampleRate != m_objRunning.m_intSampleRate)
        || (m_objRFConfig.m_blndecimatorEnable != m_objRFRunning.m_blndecimatorEnable))
    {
        int sampleRate = m_objRFConfig.m_blndecimatorEnable ? m_objConfigPrivate.m_intTVSampleRate : m_objConfig.m_intSampleRate;
        MsgReportEffectiveSampleRate *report;
        report = MsgReportEffectiveSampleRate::create(sampleRate, m_intNumberSamplePerLine);
        getOutputMessageQueue()->push(report);
    }

    if ((m_objConfigPrivate.m_intTVSampleRate != m_objRunningPrivate.m_intTVSampleRate)
        || (m_objRFConfig.m_fltRFBandwidth != m_objRFRunning.m_fltRFBandwidth)
        || (m_objRFConfig.m_fltRFOppBandwidth != m_objRFRunning.m_fltRFOppBandwidth))
    {
        m_objSettingsMutex.lock();
        m_DSBFilter->create_asym_filter(m_objRFConfig.m_fltRFOppBandwidth / m_objConfigPrivate.m_intTVSampleRate,
                m_objRFConfig.m_fltRFBandwidth / m_objConfigPrivate.m_intTVSampleRate);
        memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));
        m_DSBFilterBufferIndex = 0;
        m_objSettingsMutex.unlock();
    }

    if ((m_objConfigPrivate.m_intTVSampleRate != m_objRunningPrivate.m_intTVSampleRate)
        || (m_objRFConfig.m_fltBFOFrequency != m_objRFRunning.m_fltBFOFrequency))
    {
        m_bfoPLL.configure(m_objRFConfig.m_fltBFOFrequency / m_objConfigPrivate.m_intTVSampleRate,
                100.0 / m_objConfigPrivate.m_intTVSampleRate,
                0.01);
        m_bfoFilter.setFrequencies(m_objRFConfig.m_fltBFOFrequency, m_objConfigPrivate.m_intTVSampleRate);
    }

    if (m_objRFConfig.m_fmDeviation != m_objRFRunning.m_fmDeviation)
    {
        m_objPhaseDiscri.setFMScaling(1.0f / m_objRFConfig.m_fmDeviation);
    }

    m_objRunning = m_objConfig;
    m_objRFRunning = m_objRFConfig;
    m_objRunningPrivate = m_objConfigPrivate;
}

void ATVDemod::applyStandard()
{
    switch(m_objConfig.m_enmATVStandard)
    {
    case ATVStd405: // Follows loosely the 405 lines standard
        // what is left in a 64 us line for the image
        m_intNumberOfSyncLines  = 24; // (15+7)*2 - 20
        m_intNumberOfBlackLines = 28; // above + 4
        break;
    case ATVStdPAL525: // Follows PAL-M standard
        // what is left in a 64/1.008 us line for the image
        m_intNumberOfSyncLines  = 40; // (15+15)*2 - 20
        m_intNumberOfBlackLines = 44; // above + 4
        break;
    case ATVStdPAL625: // Follows PAL-B/G/H standard
    default:
        // what is left in a 64 us line for the image
        m_intNumberOfSyncLines  = 44; // (15+17)*2 - 20
        m_intNumberOfBlackLines = 48; // above + 4
    }

    // for now all standards apply this
    m_intNumberSamplePerLineSignals = (int) ((12.0f/64.0f)*m_objConfig.m_fltLineDuration * m_objConfig.m_intSampleRate);  // 12.0 = 7.3 + 4.7
    m_intNumberSaplesPerHSync = (int) ((9.4f/64.0f)*m_objConfig.m_fltLineDuration * m_objConfig.m_intSampleRate);      // 9.4 = 4.7 + 4.7
}

int ATVDemod::getSampleRate()
{
    return m_objRunning.m_intSampleRate;
}

int ATVDemod::getEffectiveSampleRate()
{
    return m_objRFRunning.m_blndecimatorEnable ? m_objRunningPrivate.m_intTVSampleRate : m_objRunning.m_intSampleRate;
}

bool ATVDemod::getBFOLocked()
{
    if ((m_objRFRunning.m_enmModulation == ATV_USB) || (m_objRFRunning.m_enmModulation == ATV_LSB))
    {
        return m_bfoPLL.locked();
    }
    else
    {
        return false;
    }
}

float ATVDemod::getRFBandwidthDivisor(ATVModulation modulation)
{
    switch(modulation)
    {
    case ATV_USB:
    case ATV_LSB:
        return 1.05f;
        break;
    case ATV_FM1:
    case ATV_FM2:
    case ATV_AM:
    default:
        return 2.2f;
    }
}

