///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_ATVMODBASEBAND_H
#define INCLUDE_ATVMODBASEBAND_H

#include <QObject>
#include <QMutex>

#include "dsp/samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"

#include "atvmodsource.h"

class UpChannelizer;

class ATVModBaseband : public QObject
{
    Q_OBJECT
public:
    class MsgConfigureATVModBaseband : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ATVModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureATVModBaseband* create(const ATVModSettings& settings, bool force)
        {
            return new MsgConfigureATVModBaseband(settings, force);
        }

    private:
        ATVModSettings m_settings;
        bool m_force;

        MsgConfigureATVModBaseband(const ATVModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSourceSampleRate() const { return m_sourceSampleRate; }
        int getSourceCenterFrequency() const { return m_sourceCenterFrequency; }

        static MsgConfigureChannelizer* create(int sourceSampleRate, int sourceCenterFrequency)
        {
            return new MsgConfigureChannelizer(sourceSampleRate, sourceCenterFrequency);
        }

    private:
        int m_sourceSampleRate;
        int m_sourceCenterFrequency;

        MsgConfigureChannelizer(int sourceSampleRate, int sourceCenterFrequency) :
            Message(),
            m_sourceSampleRate(sourceSampleRate),
            m_sourceCenterFrequency(sourceCenterFrequency)
        { }
    };

    class MsgConfigureImageFileName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureImageFileName* create(const QString& fileName)
        {
            return new MsgConfigureImageFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureImageFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureVideoFileName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureVideoFileName* create(const QString& fileName)
        {
            return new MsgConfigureVideoFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureVideoFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureVideoFileSourceSeek : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureVideoFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureVideoFileSourceSeek(seekPercentage);
        }

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureVideoFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
        { }
    };

    class MsgConfigureVideoFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgConfigureVideoFileSourceStreamTiming* create()
        {
            return new MsgConfigureVideoFileSourceStreamTiming();
        }

    private:

        MsgConfigureVideoFileSourceStreamTiming() :
            Message()
        { }
    };

    class MsgConfigureCameraIndex : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }

        static MsgConfigureCameraIndex* create(int index)
        {
            return new MsgConfigureCameraIndex(index);
        }

    private:
        int m_index;

        MsgConfigureCameraIndex(int index) :
            Message(),
			m_index(index)
        { }
    };

    class MsgConfigureCameraData : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }
        float getManualFPS() const { return m_manualFPS; }
        bool getManualFPSEnable() const { return m_manualFPSEnable; }

        static MsgConfigureCameraData* create(
        		int index,
				float manualFPS,
				bool manualFPSEnable)
        {
            return new MsgConfigureCameraData(index, manualFPS, manualFPSEnable);
        }

    private:
        int m_index;
        float m_manualFPS;
        bool m_manualFPSEnable;

        MsgConfigureCameraData(int index, float manualFPS, bool manualFPSEnable) :
            Message(),
			m_index(index),
			m_manualFPS(manualFPS),
			m_manualFPSEnable(manualFPSEnable)
        { }
    };

    ATVModBaseband();
    ~ATVModBaseband();
    void reset();
	void pull(const SampleVector::iterator& begin, unsigned int nbSamples);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_source.setMessageQueueToGUI(messageQueue); }
    double getMagSq() const { return m_source.getMagSq(); }
    int getChannelSampleRate() const;
    void getCameraNumbers(std::vector<int>& numbers);

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
    ATVModSource m_source;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    ATVModSettings m_settings;
    QMutex m_mutex;

    void processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd);
    bool handleMessage(const Message& cmd);
    void applySettings(const ATVModSettings& settings, bool force = false);

private slots:
    void handleInputMessages();
    void handleData(); //!< Handle data when samples have to be processed
};


#endif // INCLUDE_ATVMODBASEBAND_H
