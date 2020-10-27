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

#ifndef PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/interpolators.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"

class SampleSourceFifo;

class USRPOutputThread : public QThread, public DeviceUSRPShared::ThreadInterface
{
    Q_OBJECT

public:
    USRPOutputThread(uhd::tx_streamer::sptr stream, size_t bufSamples, SampleSourceFifo* sampleFifo, QObject* parent = 0);
    ~USRPOutputThread();

    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int sampleRate) { (void) sampleRate; }
    virtual bool isRunning() { return m_running; }
    void setLog2Interpolation(unsigned int log2_ioterp);
    void getStreamStatus(bool& active, quint32& underflows, quint32& droppedPackets);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    bool m_running;

    quint64 m_packets;
    quint32 m_underflows;
    quint32 m_droppedPackets;

    uhd::tx_streamer::sptr m_stream;
    qint16 *m_buf;
    size_t m_bufSamples;
    SampleSourceFifo* m_sampleFifo;

    unsigned int m_log2Interp; // soft decimation

    Interpolators<qint16, SDR_TX_SAMP_SZ, 12> m_interpolators;

    void run();
    void callback(qint16* buf, qint32 len);
    void callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd);
};



#endif /* PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTTHREAD_H_ */
