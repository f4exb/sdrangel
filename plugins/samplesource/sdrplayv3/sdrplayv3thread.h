///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2016, 2018-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021 Franco Venturi <fventuri@comcast.net>                      //
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

#ifndef PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3THREAD_H_
#define PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3THREAD_H_

#include <atomic>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <sdrplay_api.h>
#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "dsp/replaybuffer.h"

#define SDRPLAYV3_INIT_NBSAMPLES (1<<14)

class SDRPlayV3Input;

class SDRPlayV3Thread : public QThread {
    Q_OBJECT

public:
    SDRPlayV3Thread(sdrplay_api_DeviceT* dev, SampleSinkFifo* sampleFifo, ReplayBuffer<qint16> *replayBuffer, SDRPlayV3Input *input, QObject* parent = NULL);
    ~SDRPlayV3Thread();

    void startWork();
    bool stopWork(); //!< Returns false if sdrplay_api_Uninit failed, in which case the API may still call the callbacks
    bool isRunning() const { return m_running; }
    void setSamplerate(int samplerate);
    void setLog2Decimation(unsigned int log2_decim);
    void setFcPos(int fcPos);
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

    void resetRfChanged();
    bool waitForRfChanged();

private:
    QMutex m_startWaitMutex;
    QWaitCondition m_startWaiter;
    std::atomic<bool> m_running;

    // Watchdog that reports when the API stops delivering samples
    QMutex m_watchdogMutex;
    QWaitCondition m_watchdogWaiter;
    std::atomic<quint64> m_sampleCount;
    int m_overloadCount;
    std::atomic<bool> m_deviceLost; // Device removed or failed - don't also report the resulting stall
    static constexpr unsigned long m_watchdogPeriodMs = 1000;

    sdrplay_api_DeviceT *m_dev;
    SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;
	ReplayBuffer<qint16> *m_replayBuffer;
    SDRPlayV3Input *m_input;

    int m_samplerate;
    unsigned int m_log2Decim;
    int m_fcPos;
    bool m_iqOrder;

    qint16 m_iq[8192];
    int m_iqCount;

    std::atomic<int> m_rfChanged;
    static const unsigned int m_rfChangedTimeout = 500;

    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimatorsIQ;
    Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, false> m_decimatorsQI;

    void run();
    void callbackIQ(const qint16* buf, qint32 len);
    void callbackQI(const qint16* buf, qint32 len);

    static void callbackHelper(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *ctx);
    static void eventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext);
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAYV3_SDRPLAYV3THREAD_H_ */
