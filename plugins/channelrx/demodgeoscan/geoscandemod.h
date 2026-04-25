#ifndef INCLUDE_GEOSCANDEMOD_H
#define INCLUDE_GEOSCANDEMOD_H

#include <QObject>
#include <QThread>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "geoscanGFSK.h"
#include "dsp/dspcommands.h"

#define SYMBOL_RATE 9600.0f
#define DEFAULT_SAMPLE_RATE 96000.0f

class DeviceAPI;

class GeoscanDemod : public BasebandSampleSink, public ChannelAPI
{
public:
    GeoscanDemod(DeviceAPI *deviceAPI);
    virtual ~GeoscanDemod();
    virtual void destroy() { delete this; }

    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin,
                      const SampleVector::const_iterator& end,
                      bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual const QString& getURI() const { return getName(); }
    virtual void getTitle(QString& title) { title = "Geoscan Decoder"; }
    virtual qint64 getCenterFrequency() const { return 0; }
    virtual void setCenterFrequency(qint64 frequency) { (void)frequency; }

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void)data; return false; }

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return -1; }
    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void)streamIndex; (void)sinkElseSource; return 0;
    }

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    virtual bool handleMessage(const Message& cmd);

    float m_sampleRate;
    GFSK m_gfsk;
};

#endif // INCLUDE_GEOSCANDEMOD_H