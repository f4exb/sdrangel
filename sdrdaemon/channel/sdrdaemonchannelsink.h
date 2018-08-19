///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx)                                                   //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSINK_H_
#define SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSINK_H_

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class SDRDaemonChannelSink : public BasebandSampleSink, public ChannelSinkAPI {
    Q_OBJECT
public:
    SDRDaemonChannelSink(DeviceSourceAPI *deviceAPI);
    virtual ~SDRDaemonChannelSink();
    virtual void destroy() { delete this; }

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = "SDRDaemon Sink"; }
    virtual qint64 getCenterFrequency() const { return 0; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    bool m_running;

};

#endif /* SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSINK_H_ */
