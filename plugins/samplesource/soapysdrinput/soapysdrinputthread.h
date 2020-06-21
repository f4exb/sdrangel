///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUTTHREAD_H_

// SoapySDR is a device wrapper with a single stream supporting one or many Rx
// Therefore only one thread can be allocated for the Rx side
// All FIFOs must be registered before calling startWork()

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <SoapySDR/Device.hpp>

#include "soapysdr/devicesoapysdrshared.h"
#include "dsp/decimators.h"
#include "dsp/decimatorsfi.h"

class SampleSinkFifo;

class SoapySDRInputThread : public QThread {
    Q_OBJECT

public:
    SoapySDRInputThread(SoapySDR::Device* dev, unsigned int nbRxChannels, QObject* parent = 0);
    ~SoapySDRInputThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    unsigned int getNbChannels() const { return m_nbChannels; }
    void setLog2Decimation(unsigned int channel, unsigned int log2_decim);
    unsigned int getLog2Decimation(unsigned int channel) const;
    void setSampleRate(unsigned int sampleRate) { m_sampleRate = sampleRate; }
    unsigned int getSampleRate() const { return m_sampleRate; }
    void setFcPos(unsigned int channel, int fcPos);
    int getFcPos(unsigned int channel) const;
    void setFifo(unsigned int channel, SampleSinkFifo *sampleFifo);
    SampleSinkFifo *getFifo(unsigned int channel);
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
    struct Channel
    {
        SampleVector m_convertBuffer;
        SampleSinkFifo* m_sampleFifo;
        unsigned int m_log2Decim;
        int m_fcPos;
        Decimators<qint32, qint8, SDR_RX_SAMP_SZ, 8, true> m_decimators8IQ;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimators12IQ;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimators16IQ;
        Decimators<qint32, qint8, SDR_RX_SAMP_SZ, 8, false> m_decimators8QI;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimators12QI;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, false> m_decimators16QI;
        DecimatorsFI<true> m_decimatorsFloatIQ;
        DecimatorsFI<false> m_decimatorsFloatQI;

        Channel() :
            m_sampleFifo(0),
            m_log2Decim(0),
            m_fcPos(0)
        {}

        ~Channel()
        {}
    };

    enum DecimatorType
    {
        Decimator8,
        Decimator12,
        Decimator16,
        DecimatorFloat
    };

    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;
    SoapySDR::Device* m_dev;

    Channel *m_channels; //!< Array of channels dynamically allocated for the given number of Rx channels
    unsigned int m_sampleRate;
    unsigned int m_nbChannels;
    DecimatorType m_decimatorType;
    bool m_iqOrder;

    void run();
    unsigned int getNbFifos();

    void callbackSI8IQ(const qint8* buf, qint32 len, unsigned int channel = 0);
    void callbackSI12IQ(const qint16* buf, qint32 len, unsigned int channel = 0);
    void callbackSI16IQ(const qint16* buf, qint32 len, unsigned int channel = 0);
    void callbackSIFIQ(const float* buf, qint32 len, unsigned int channel = 0);

    void callbackSI8QI(const qint8* buf, qint32 len, unsigned int channel = 0);
    void callbackSI12QI(const qint16* buf, qint32 len, unsigned int channel = 0);
    void callbackSI16QI(const qint16* buf, qint32 len, unsigned int channel = 0);
    void callbackSIFQI(const float* buf, qint32 len, unsigned int channel = 0);

    void callbackMIIQ(std::vector<void *>& buffs, qint32 samplesPerChannel);
    void callbackMIQI(std::vector<void *>& buffs, qint32 samplesPerChannel);
};



#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUTTHREAD_H_ */
