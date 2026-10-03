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

#ifndef PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMOTHREAD_H_
#define PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMOTHREAD_H_

#include <atomic>
#include <vector>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>

#include "dsp/interpolators.h"
#include "util/messagequeue.h"

class SampleMOFifo;

/**
 * Sends samples to all Tx channels of the device, using a single multi-channel
 * stream, so samples for each channel are time aligned.
 *
 * As this object is created in USRPMIMO::startTx(), which is called from the device engine thread,
 * its slots execute in the device engine thread, which is where samples are written to the FIFO.
 * This allows the FIFO to be resized safely (See MsgResizeSampleFifo).
 */
class USRPMOThread : public QThread {
    Q_OBJECT

public:
    static constexpr int m_maxChannels = 2;

    USRPMOThread(uhd::usrp::multi_usrp::sptr device, uhd::tx_streamer::sptr stream, size_t bufSamples,
        int nbChannels, SampleMOFifo *sampleFifo, QObject* parent = nullptr);
    ~USRPMOThread();

    void startWork();
    void stopWork();
    bool isRunning() const { return m_running; }
    void setLog2Interpolation(unsigned int log2Interp) { m_log2Interp = log2Interp; }
    void getStreamStatus(bool& active, quint32& underflows, quint32& droppedPackets);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void detach();
    void releaseStream();

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    std::atomic<bool> m_running;

    // Counters are reset in startWork (device engine thread) and read in getStreamStatus (GUI / web API threads)
    std::atomic<quint64> m_packets;
    std::atomic<quint32> m_underflows;
    std::atomic<quint32> m_droppedPackets;
    std::atomic<bool> m_lateStartPending; //!< Late start of burst seen in getStreamStatus, to be handled in run()
    QMutex m_asyncMsgMutex;             //!< Serialises recv_async_msg, which is called from different threads

    uhd::usrp::multi_usrp::sptr m_device;
    uhd::tx_streamer::sptr m_stream;
    size_t m_bufSamples;
    int m_nbChannels;
    std::vector<std::vector<qint16>> m_buf; //!< Per channel buffers of interleaved I/Q
    std::vector<const void *> m_bufPtrs;
    SampleMOFifo *m_sampleFifo;
    std::atomic<unsigned int> m_log2Interp;

    //!< UHD's sc16 format is 16-bit, converted by UHD to device wire format (E.g. 12-bit for B2xx)
    Interpolators<qint16, SDR_TX_SAMP_SZ, 16> m_interpolators[m_maxChannels];

    MessageQueue m_inputMessageQueue;   //!< Requests to execute in device engine thread
    QMutex m_configMutex;               //!< Held while handling requests, so detach() can wait for them to complete
    bool m_detached;                    //!< When set, no longer handle requests

    void run();
    void setTimedStart(uhd::tx_metadata_t& md);
    bool processAsyncMessages();
    unsigned int callback();
    void callbackPart(unsigned int iBegin, unsigned int nSamples, unsigned int offset, unsigned int log2Interp);
    void resizeSampleFifo(unsigned int size);

private slots:
    void handleInputMessages();
};

#endif // PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMOTHREAD_H_
