///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// (C) 2015 John Greb                                                            //
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

#ifndef INCLUDE_LORADEMOD_H
#define INCLUDE_LORADEMOD_H

#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "lorademodbaseband.h"

class DeviceAPI;
class QThread;

class LoRaDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureLoRaDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LoRaDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLoRaDemod* create(const LoRaDemodSettings& settings, bool force)
        {
            return new MsgConfigureLoRaDemod(settings, force);
        }

    private:
        LoRaDemodSettings m_settings;
        bool m_force;

        MsgConfigureLoRaDemod(const LoRaDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	LoRaDemod(DeviceAPI* deviceAPI);
	virtual ~LoRaDemod();
	virtual void destroy() { delete this; }
	void setSpectrumSink(BasebandSampleSink* sampleSink) { m_basebandSink->setSpectrumSink(sampleSink); }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool pO);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return 0; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
	DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    LoRaDemodBaseband* m_basebandSink;
    LoRaDemodSettings m_settings;
    int m_basebandSampleRate;

    void applySettings(const LoRaDemodSettings& settings, bool force = false);
};

#endif // INCLUDE_LORADEMOD_H
