///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#ifndef INCLUDE_ATVDEMOD_H
#define INCLUDE_ATVDEMOD_H

#include <QElapsedTimer>
#include <QThread>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspcommands.h"
#include "util/message.h"

#include "atvdemodbaseband.h"

class DeviceAPI;
class ScopeVis;

class ATVDemod : public BasebandSampleSink, public ChannelAPI
{
public:
    class MsgConfigureATVDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ATVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureATVDemod* create(const ATVDemodSettings& settings, bool force)
        {
            return new MsgConfigureATVDemod(settings, force);
        }

    private:
        ATVDemodSettings m_settings;
        bool m_force;

        MsgConfigureATVDemod(const ATVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    ATVDemod(DeviceAPI *deviceAPI);
	virtual ~ATVDemod();
	virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void) data; return false; }

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

	ScopeVis *getScopeSink() { return m_basebandSink->getScopeSink(); }
    void setTVScreen(TVScreenAnalog *tvScreen) { m_basebandSink->setTVScreen(tvScreen); }; //!< set by the GUI
    double getMagSq() const { return m_basebandSink->getMagSq(); } //!< Beware this is scaled to 2^30
    bool getBFOLocked() { return m_basebandSink->getBFOLocked(); }
    void setVideoTabIndex(int videoTabIndex) { m_basebandSink->setVideoTabIndex(videoTabIndex); }
    uint32_t getNumberOfDeviceStreams() const;

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread m_thread;
    ATVDemodBaseband* m_basebandSink;
    ATVDemodSettings m_settings;
    qint64 m_centerFrequency; //!< center frequency stored from device message used when starting baseband sink
    int m_basebandSampleRate; //!< sample rate stored from device message used when starting baseband sink

	virtual bool handleMessage(const Message& cmd);
    void applySettings(const ATVDemodSettings& settings, bool force = false);

private slots:
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_ATVDEMOD_H
