///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <atomic>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "dsp/replaybuffer.h"
#include "util/messagequeue.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"
#include "usrpinputsettings.h"

/**
 * Receives samples from the device in its own thread (run()).
 *
 * Also applies device settings, which can take a long time (E.g. sample rate, clock source and bandwidth),
 * via its input message queue, so they don't block the GUI thread. As this object is created in
 * USRPInput::start(), which is called from the device engine thread, its slots execute in the device
 * engine thread, which is also where the channel is acquired and released, so these can't race.
 */
class USRPInputThread : public QThread, public DeviceUSRPShared::ThreadInterface
{
    Q_OBJECT

public:
    USRPInputThread(DeviceUSRPParams *deviceParams, int channel, SampleSinkFifo* sampleFifo,
        ReplayBuffer<qint16> *replayBuffer, MessageQueue *reportQueue, QObject* parent = nullptr);
    ~USRPInputThread();

    void setStream(uhd::rx_streamer::sptr stream, size_t bufSamples);
    void releaseStream();
    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int sampleRate) { (void) sampleRate; }
    virtual bool isRunning() { return m_started; }
    void setLog2Decimation(unsigned int log2_decim);
    void getStreamStatus(bool& active, quint32& overflows, quint32& m_timeouts);
    void issueStreamCmd(bool start);
    size_t recv(uhd::rx_metadata_t& md, double timeout);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void detach();

    static void applyDeviceSettings(
        DeviceUSRPParams *deviceParams,
        int channel,
        const USRPInputSettings& settings,
        const QList<QString>& settingsKeys,
        bool force,
        bool applyChannelSettings,
        bool applyBandwidth,
        MessageQueue *reportQueue);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    std::atomic<bool> m_running;    //!< Set while run() loop should continue
    std::atomic<bool> m_started;    //!< Set from startWork() to stopWork()
    std::atomic<bool> m_runStarted; //!< Set when run() has started

    quint64 m_packets;
    quint32 m_overflows;
    quint32 m_timeouts;

    uhd::rx_streamer::sptr m_stream;
    qint16 *m_buf;
    size_t m_bufSamples;
    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;
    ReplayBuffer<qint16> *m_replayBuffer;

    std::atomic<unsigned int> m_log2Decim; // soft decimation

    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimatorsIQ;

    DeviceUSRPParams *m_deviceParams;
    int m_channel;
    MessageQueue *m_reportQueue;        //!< Where to send MsgReportDeviceSettings (USRPInput's input queue)
    MessageQueue m_inputMessageQueue;   //!< Settings to apply to the device
    QMutex m_configMutex;               //!< Held while applying settings, so detach() can wait for it to complete
    bool m_detached;                    //!< When set, no longer access device or report queue

    void run();
    void callbackIQ(const qint16* buf, qint32 len);
    void readBackSampleRate();

private slots:
    void handleInputMessages();
};

#endif /* PLUGINS_SAMPLESOURCE_USRPINPUT_USRPINPUTTHREAD_H_ */
