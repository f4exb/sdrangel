///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELTX_MODDATV_DATVMODBASEBAND_H_
#define PLUGINS_CHANNELTX_MODDATV_DATVMODBASEBAND_H_

#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/chrono.hpp>

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "datvmodsource.h"

class UpChannelizer;

class DATVModBaseband : public QObject
{
    Q_OBJECT
public:

    DATVModBaseband();
    ~DATVModBaseband();
    void reset();
    void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_source.setMessageQueueToGUI(messageQueue); }
    double getMagSq() const { return m_source.getMagSq(); }
    void geTsFileInfos(int& mpegTSBitrate, int& mpegTSSize) const { m_source.geTsFileInfos(mpegTSBitrate, mpegTSSize); }
    int64_t getUdpByteCount() const { return m_source.getUdpByteCount(); }
    int getDataRate() const { return m_source.getDataRate(); }
    int getChannelSampleRate() const;
    int getEffectiveSampleRate() const { return m_source.getEffectiveSampleRate(); }

signals:
    /**
     * Level changed
     * \param rmsLevel RMS level in range 0.0 - 1.0
     * \param peakLevel Peak level in range 0.0 - 1.0
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    SampleSourceFifo m_sampleFifo;
    UpChannelizer *m_channelizer;
    DATVModSource m_source;
    MessageQueue m_inputMessageQueue;
    DATVModSettings m_settings;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const DATVModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // PLUGINS_CHANNELTX_MODDATV_DATVMODBASEBAND_H_
