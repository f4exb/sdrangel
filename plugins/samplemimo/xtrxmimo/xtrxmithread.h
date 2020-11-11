///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMITHREAD_H_
#define PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMITHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "xtrx_api.h"

#include "xtrx/devicextrx.h"
#include "dsp/decimators.h"

class SampleMIFifo;

class XTRXMIThread : public QThread {
    Q_OBJECT

public:
    XTRXMIThread(struct xtrx_dev *dev, QObject* parent = nullptr);
    ~XTRXMIThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setLog2Decimation(unsigned int log2_decim);
    unsigned int getLog2Decimation() const;
    void setFifo(SampleMIFifo *sampleFifo) { m_sampleFifo = sampleFifo; }
    SampleMIFifo *getFifo() { return m_sampleFifo; }
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
    struct Channel
    {
        SampleVector m_convertBuffer;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, true> m_decimatorsIQ;
        Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 12, false> m_decimatorsQI;
    };

    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    struct xtrx_dev* m_dev;
    qint16 m_buf[2][2*DeviceXTRX::blockSize];
    Channel m_channels[2];
    std::vector<SampleVector::const_iterator> m_vBegin;
    SampleMIFifo* m_sampleFifo;
    unsigned int m_log2Decim;
    bool m_iqOrder;

    void run();
    int callbackSIIQ(unsigned int channel, const qint16* buf, qint32 len);
    int callbackSIQI(unsigned int channel, const qint16* buf, qint32 len);
};

#endif // PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMITHREAD_H_
