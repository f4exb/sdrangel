///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_SAMPLESOURCE_USRPINPUT_USRPINPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_USRPINPUT_USRPINPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"

class USRPInputThread : public QThread, public DeviceUSRPShared::ThreadInterface
{
    Q_OBJECT

public:
    USRPInputThread(uhd::rx_streamer::sptr stream, size_t bufSamples, SampleSinkFifo* sampleFifo, QObject* parent = 0);
    ~USRPInputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int sampleRate) { (void) sampleRate; }
    virtual bool isRunning() { return m_running; }
    void setLog2Decimation(unsigned int log2_decim);
    void getStreamStatus(bool& active, quint32& overflows, quint32& m_timeouts);
    void issueStreamCmd(bool start);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    quint64 m_packets;
    quint32 m_overflows;
    quint32 m_timeouts;

    uhd::rx_streamer::sptr m_stream;
    qint16 *m_buf;
    size_t m_bufSamples;
    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;

    unsigned int m_log2Decim; // soft decimation

    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimatorsIQ;

    void run();
    void callbackIQ(const qint16* buf, qint32 len);
};



#endif /* PLUGINS_SAMPLESOURCE_USRPINPUT_USRPINPUTTHREAD_H_ */
