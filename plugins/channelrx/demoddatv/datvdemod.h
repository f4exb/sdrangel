///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef INCLUDE_DATVDEMOD_H
#define INCLUDE_DATVDEMOD_H

class DeviceAPI;

#include "channel/channelapi.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspcommands.h"
#include "util/message.h"

#include "datvdemodbaseband.h"


class DATVDemod : public BasebandSampleSink, public ChannelAPI
{
	Q_OBJECT

public:

    DATVDemod(DeviceAPI *);
    ~DATVDemod();

    virtual void destroy() { delete this; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }
    virtual qint64 getCenterFrequency() const { return m_settings.m_centerFrequency; }

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void) data; return false; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_centerFrequency;
    }

    void SetTVScreen(TVScreen *objScreen) { m_basebandSink->setTVScreen(objScreen); }
    DATVideostream *SetVideoRender(DATVideoRender *objScreen) { return m_basebandSink->SetVideoRender(objScreen); }
    bool audioActive() { return m_basebandSink->audioActive(); }
    bool audioDecodeOK() { return m_basebandSink->audioDecodeOK(); }
    bool videoActive() { return m_basebandSink->videoActive(); }
    bool videoDecodeOK() { return m_basebandSink->videoDecodeOK(); }

    bool PlayVideo(bool blnStartStop) { return m_basebandSink->PlayVideo(blnStartStop); }

    double getMagSq() const { return m_basebandSink->getMagSq(); } //!< Beware this is scaled to 2^30
    int getModcodModulation() const { return m_basebandSink->getModcodModulation(); }
    int getModcodCodeRate() const { return m_basebandSink->getModcodCodeRate(); }
    bool isCstlnSetByModcod() const { return m_basebandSink->isCstlnSetByModcod(); }

    static const QString m_channelIdURI;
    static const QString m_channelId;

    class MsgConfigureDATVDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DATVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDATVDemod* create(const DATVDemodSettings& settings, bool force)
        {
            return new MsgConfigureDATVDemod(settings, force);
        }

    private:
        DATVDemodSettings m_settings;
        bool m_force;

        MsgConfigureDATVDemod(const DATVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    DATVDemodBaseband* m_basebandSink;
    DATVDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink

    void applySettings(const DATVDemodSettings& settings, bool force = false);
};

#endif // INCLUDE_DATVDEMOD_H
