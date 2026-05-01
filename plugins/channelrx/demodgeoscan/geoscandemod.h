#ifndef INCLUDE_GEOSCANDEMOD_H
#define INCLUDE_GEOSCANDEMOD_H

#include <QObject>
#include <QString>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "geoscanDCBlocker.h"
#include "geoscanGFSK.h"
#include "geoscanLowPassFilter.h"
#include "geoscanCorrelator.h"
#include "dsp/dspcommands.h"

constexpr float SYMBOL_RATE         = 9600.0f;
constexpr float DEFAULT_SAMPLE_RATE = 96000.0f;

class DeviceAPI;

struct GeoscanSettings {
    float   dev        = 150.0f;
    float   bw         = 4800.0f;
    int     threshold  = 6900;
    float   deltaF     = 0.0f;
    QString udpAddress = "127.0.0.1";
    int     udpPort    = 9999;
    int     decoder    = 0; // 0=RAW, 1=ADS-B, 2=AIS, 3=IMAGE
};

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

    class MsgConfigureGeoscan : public Message
    {
        MESSAGE_CLASS_DECLARATION
    public:
        const GeoscanSettings& getSettings() const { return m_settings; }

        static MsgConfigureGeoscan* create(const GeoscanSettings& settings) {
            return new MsgConfigureGeoscan(settings);
        }

    private:
        GeoscanSettings m_settings;

        MsgConfigureGeoscan(const GeoscanSettings& settings) :
            Message(), m_settings(settings) {}
    };

    float getSnr() const { return m_snr; }

private:
    DeviceAPI *m_deviceAPI;
    virtual bool handleMessage(const Message& cmd);
    void applySettings(const GeoscanSettings& settings);
    void onPacketReady(const std::vector<uint8_t>& packet);

    float m_snr = 0.0f;

    float           m_sampleRate;
    GeoscanSettings m_settings;
    DCBlocker       m_dcBlocker;
    GFSK            m_gfsk;
    LowPassFilter   m_lpf;
    Correlator      m_crltr;
};

#endif // INCLUDE_GEOSCANDEMOD_H