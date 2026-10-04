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
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Jon Beniston, M7RCE <jon@beniston.com>                     //

#ifndef PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTTHREAD_H_
#define PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTTHREAD_H_

#include <atomic>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/interpolators.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"
#include "util/messagequeue.h"
#include "usrpoutputsettings.h"

class SampleSourceFifo;

/**
 * Sends samples to the device in its own thread (run()).
 *
 * Also applies device settings, which can take a long time (E.g. sample rate, clock source and bandwidth),
 * via its input message queue, so they don't block the GUI thread. As this object is created in
 * USRPOutput::start(), which is called from the device engine thread, its slots execute in the device
 * engine thread, which is also where the channel is acquired and released, so these can't race.
 */
class USRPOutputThread : public QThread, public DeviceUSRPShared::ThreadInterface
{
    Q_OBJECT

public:
    USRPOutputThread(DeviceUSRPParams *deviceParams, int channel, SampleSourceFifo* sampleFifo,
        MessageQueue *reportQueue, QObject* parent = nullptr);
    ~USRPOutputThread();

    void setStream(uhd::tx_streamer::sptr stream, size_t bufSamples);
    void releaseStream();
    virtual void startWork();
    virtual void stopWork();
    virtual void setDeviceSampleRate(int sampleRate) { (void) sampleRate; }
    virtual bool isRunning() { return m_running; }
    void setLog2Interpolation(unsigned int log2_ioterp);
    void getStreamStatus(bool& active, quint32& underflows, quint32& droppedPackets);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void detach();

    static void applyDeviceSettings(
        DeviceUSRPParams *deviceParams,
        int channel,
        const USRPOutputSettings& settings,
        const QList<QString>& settingsKeys,
        bool force,
        bool applyChannelSettings,
        bool applyBandwidth,
        MessageQueue *reportQueue);

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    std::atomic<bool> m_running;

    // Counters are reset in startWork (device engine threads) and read in getStreamStatus (GUI / web API threads)
    std::atomic<quint64> m_packets;
    std::atomic<quint32> m_underflows;
    std::atomic<quint32> m_droppedPackets;
    QMutex m_asyncMsgMutex;             //!< Serialises recv_async_msg, which is called from different threads

    uhd::tx_streamer::sptr m_stream;
    qint16 *m_buf;
    size_t m_bufSamples;
    SampleSourceFifo* m_sampleFifo;

    std::atomic<unsigned int> m_log2Interp; // soft interpolation

    Interpolators<qint16, SDR_TX_SAMP_SZ, 16> m_interpolators; //!< UHD's sc16 format is 16-bit, converted by UHD to device wire format (E.g. 12-bit for B2xx)

    DeviceUSRPParams *m_deviceParams;
    int m_channel;
    MessageQueue *m_reportQueue;        //!< Where to send MsgReportDeviceSettings (USRPOutput's input queue)
    MessageQueue m_inputMessageQueue;   //!< Settings to apply to the device
    QMutex m_configMutex;               //!< Held while applying settings, so detach() can wait for it to complete
    bool m_detached;                    //!< When set, no longer access device or report queue

    void run();
    qint32 callback(qint16* buf, qint32 len);
    void callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int log2Interp);
    void readBackSampleRate();
    void resizeSampleFifo(unsigned int size);

private slots:
    void handleInputMessages();
};

#endif /* PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUTTHREAD_H_ */
