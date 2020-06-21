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

#ifndef PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUTTHREAD_H_

// BladerRF2 is a SISO/MIMO device with a single stream supporting one or two Rx
// Therefore only one thread can be allocated for the Rx side
// All FIFOs must be registered before calling startWork() else SISO/MIMO switch will not work properly
// with unpredicatable results

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <libbladeRF.h>

#include "bladerf2/devicebladerf2shared.h"
#include "dsp/decimators.h"

class SampleSinkFifo;

class BladeRF2InputThread : public QThread {
    Q_OBJECT

public:
    BladeRF2InputThread(struct bladerf* dev, unsigned int nbRxChannels, QObject* parent = NULL);
    ~BladeRF2InputThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    unsigned int getNbChannels() const { return m_nbChannels; }
    void setLog2Decimation(unsigned int channel, unsigned int log2_decim);
    unsigned int getLog2Decimation(unsigned int channel) const;
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
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimatorsIQ;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimatorsQI;

        Channel() :
            m_sampleFifo(0),
            m_log2Decim(0),
            m_fcPos(0)
        {}

        ~Channel()
        {}
    };

    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;
    struct bladerf* m_dev;

    Channel *m_channels; //!< Array of channels dynamically allocated for the given number of Rx channels
    qint16 *m_buf; //!< Full buffer for SISO or MIMO operation
    unsigned int m_nbChannels;
    bool m_iqOrder;

    void run();
    unsigned int getNbFifos();
    void callbackSIIQ(const qint16* buf, qint32 len, unsigned int channel = 0);
    void callbackSIQI(const qint16* buf, qint32 len, unsigned int channel = 0);
    void callbackMI(const qint16* buf, qint32 samplesPerChannel);
};



#endif /* PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUTTHREAD_H_ */
