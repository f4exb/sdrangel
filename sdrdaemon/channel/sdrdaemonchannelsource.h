///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx)                                                 //
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

#ifndef SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSOURCE_H_
#define SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSOURCE_H_

#include "dsp/basebandsamplesource.h"
#include "channel/channelsourceapi.h"

class ThreadedBasebandSampleSource;
class UpChannelizer;
class DeviceSinkAPI;

class SDRDaemonChannelSource : public BasebandSampleSource, public ChannelSourceAPI {
    Q_OBJECT
public:
    SDRDaemonChannelSource(DeviceSinkAPI *deviceAPI);
    ~SDRDaemonChannelSource();
    virtual void destroy() { delete this; }

    virtual void pull(Sample& sample);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = "SDRDaemon Source"; }
    virtual qint64 getCenterFrequency() const { return 0; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceSinkAPI *m_deviceAPI;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;

    bool m_running;
    uint32_t m_samplesCount;
};


#endif /* SDRDAEMON_CHANNEL_SDRDAEMONCHANNELSOURCE_H_ */
