///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMITHREAD_H_
#define PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMITHREAD_H_

#include <atomic>
#include <vector>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>

#include "dsp/decimators.h"

class SampleMIFifo;

/**
 * Receives samples from all Rx channels of the device, using a single multi-channel
 * stream, so samples from each channel are time aligned.
 */
class USRPMIThread : public QThread {
    Q_OBJECT

public:
    static constexpr int m_maxChannels = 2;

    USRPMIThread(uhd::usrp::multi_usrp::sptr device, uhd::rx_streamer::sptr stream, size_t bufSamples,
        int nbChannels, SampleMIFifo *sampleFifo, QObject* parent = nullptr);
    ~USRPMIThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_started; }
    void setLog2Decimation(unsigned int log2Decim) { m_log2Decim = log2Decim; }
    void getStreamStatus(bool& active, quint32& overflows, quint32& timeouts);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    std::atomic<bool> m_running;    //!< Set while run() loop should continue
    std::atomic<bool> m_started;    //!< Set from startWork() to stopWork()
    std::atomic<bool> m_runStarted; //!< Set when run() has started

    std::atomic<quint64> m_packets;
    std::atomic<quint32> m_overflows;
    std::atomic<quint32> m_timeouts;

    uhd::usrp::multi_usrp::sptr m_device;
    uhd::rx_streamer::sptr m_stream;
    size_t m_bufSamples;
    int m_nbChannels;
    std::vector<std::vector<qint16>> m_buf; //!< Per channel buffers of interleaved I/Q
    std::vector<void *> m_bufPtrs;
    SampleVector m_convertBuffer[m_maxChannels];
    SampleMIFifo *m_sampleFifo;
    std::atomic<unsigned int> m_log2Decim;

    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimators[m_maxChannels];

    void run();
    void issueStreamCmd(bool start);
    void flush();
    int channelCallback(const qint16* buf, qint32 len, int channel, unsigned int log2Decim);
};

#endif // PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMITHREAD_H_
