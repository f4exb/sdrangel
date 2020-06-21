///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, 2018 Edouard Griffiths, F4EXB                             //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#ifndef PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "xtrx_api.h"

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "xtrx/devicextrxshared.h"

struct xtrx_dev;

class XTRXInputThread : public QThread, public DeviceXTRXShared::ThreadInterface
{
    Q_OBJECT

public:
    XTRXInputThread(struct xtrx_dev *dev, unsigned int nbChannels, unsigned int uniqueChannelIndex = 0, QObject* parent = 0);
    ~XTRXInputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual bool isRunning() { return m_running; }
    unsigned int getNbChannels() const { return m_nbChannels; }
    void setLog2Decimation(unsigned int channel, unsigned int log2_decim);
    unsigned int getLog2Decimation(unsigned int channel) const;
    void setFifo(unsigned int channel, SampleSinkFifo *sampleFifo);
    SampleSinkFifo *getFifo(unsigned int channel);
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
    struct Channel
    {
        SampleVector m_convertBuffer;
        SampleSinkFifo* m_sampleFifo;
        unsigned int m_log2Decim;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimatorsIQ;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimatorsQI;

        Channel() :
            m_sampleFifo(0),
            m_log2Decim(0)
        {}

        ~Channel()
        {}
    };

    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;
    struct xtrx_dev *m_dev;

    Channel *m_channels; //!< Array of channels dynamically allocated for the given number of Rx channels
    unsigned int m_nbChannels;
    unsigned int m_uniqueChannelIndex;
    bool m_iqOrder;

    void run();
    unsigned int getNbFifos();
    void callbackSIQI(const qint16* buf, qint32 len);
    void callbackSIIQ(const qint16* buf, qint32 len);
    void callbackMI(const qint16* buf0, const qint16* buf1, qint32 len);
};



#endif /* PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUTTHREAD_H_ */
