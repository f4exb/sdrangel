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

#ifndef INCLUDE_INTERFEROMETER_H
#define INCLUDE_INTERFEROMETER_H

#include <vector>
#include <QNetworkRequest>

#include "dsp/mimosamplesink.h"
#include "channel/channelapi.h"
#include "util/messagequeue.h"
#include "util/message.h"

#include "interferometersettings.h"

class QThread;
class DeviceAPI;
class InterferometerSink;
class QNetworkReply;
class QNetworkAccessManager;
class BasebandSampleSink;

class Interferometer: public MIMOSampleSink, ChannelAPI
{
    Q_OBJECT
public:
    class MsgConfigureInterferometer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const InterferometerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureInterferometer* create(const InterferometerSettings& settings, bool force)
        {
            return new MsgConfigureInterferometer(settings, force);
        }

    private:
        InterferometerSettings m_settings;
        bool m_force;

        MsgConfigureInterferometer(const InterferometerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getLog2Decim() const { return m_log2Decim; }
        int getFilterChainHash() const { return m_filterChainHash; }

        static MsgConfigureChannelizer* create(unsigned int log2Decim, unsigned int filterChainHash) {
            return new MsgConfigureChannelizer(log2Decim, filterChainHash);
        }

    private:
        unsigned int m_log2Decim;
        unsigned int m_filterChainHash;

        MsgConfigureChannelizer(unsigned int log2Decim, unsigned int filterChainHash) :
            Message(),
            m_log2Decim(log2Decim),
            m_filterChainHash(filterChainHash)
        { }
    };

    class MsgSampleRateNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgSampleRateNotification* create(int sampleRate) {
            return new MsgSampleRateNotification(sampleRate);
        }

        int getSampleRate() const { return m_sampleRate; }

    private:

        MsgSampleRateNotification(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }

        int m_sampleRate;
    };

    Interferometer(DeviceAPI *deviceAPI);
	virtual ~Interferometer();
	virtual void destroy() { delete this; }

	virtual void start(); //!< thread start()
	virtual void stop(); //!< thread exit() and wait()
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex);
	virtual bool handleMessage(const Message& cmd); //!< Processing of a message. Returns true if message has actually been processed

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

    void setSpectrumSink(BasebandSampleSink *spectrumSink);
    void setScopeSink(BasebandSampleSink *scopeSink);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const InterferometerSettings& settings);

    static void webapiUpdateChannelSettings(
            InterferometerSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    InterferometerSink* m_sink;
    BasebandSampleSink* m_spectrumSink;
    BasebandSampleSink* m_scopeSink;
    InterferometerSettings m_settings;
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const InterferometerSettings& settings, bool force = false);
    static void validateFilterChainHash(InterferometerSettings& settings);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const InterferometerSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleData(int start, int stop);
};

#endif // INCLUDE_INTERFEROMETER_H
