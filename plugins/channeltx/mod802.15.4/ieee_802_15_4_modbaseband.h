///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_IEEE_802_15_4_MODBASEBAND_H
#define INCLUDE_IEEE_802_15_4_MODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "dsp/scopevis.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "ieee_802_15_4_modsource.h"

class UpChannelizer;
class ScopeVis;

class IEEE_802_15_4_ModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureIEEE_802_15_4_ModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const IEEE_802_15_4_ModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureIEEE_802_15_4_ModBaseband* create(const IEEE_802_15_4_ModSettings& settings, bool force)
        {
            return new MsgConfigureIEEE_802_15_4_ModBaseband(settings, force);
        }

    private:
        IEEE_802_15_4_ModSettings m_settings;
        bool m_force;

        MsgConfigureIEEE_802_15_4_ModBaseband(const IEEE_802_15_4_ModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    IEEE_802_15_4_ModBaseband();
    ~IEEE_802_15_4_ModBaseband();
    void reset();
    void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    void setSpectrumSampleSink(BasebandSampleSink* sampleSink) { m_source.setSpectrumSink(sampleSink); }
    ScopeVis *getScopeSink() { return &m_scopeSink; }


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
    IEEE_802_15_4_ModSource m_source;
    MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    IEEE_802_15_4_ModSettings m_settings;
    ScopeVis m_scopeSink;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const IEEE_802_15_4_ModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_IEEE_802_15_4_MODBASEBAND_H
