#include "geoscandemod.h"
#include <QDebug>

// Уникальные идентификаторы нашего плагина
const char* const GeoscanDemod::m_channelIdURI = "sdrangel.channel.geoscandemod";
const char* const GeoscanDemod::m_channelId    = "GeoscanDemod";

GeoscanDemod::GeoscanDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_sampleRate(DEFAULT_SAMPLE_RATE),
    m_gfsk(DEFAULT_SAMPLE_RATE, SYMBOL_RATE),
    m_crltr((int)(DEFAULT_SAMPLE_RATE / SYMBOL_RATE), DEFAULT_THRESHOLD, [this](const std::vector<uint8_t>& packet) {
        onPacketReady(packet);
    })
{
    setObjectName(m_channelId);
    qDebug() << "GeoscanDemod: создан";
}

GeoscanDemod::~GeoscanDemod()
{
    qDebug() << "GeoscanDemod: уничтожен";
}

void GeoscanDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    m_deviceAPI = deviceAPI;
}

// Сюда приходят IQ-сэмплы от SDRangel
// begin, end — итераторы на массив сэмплов
// Каждый сэмпл: it->real() = I, it->imag() = Q
void GeoscanDemod::feed(
    const SampleVector::const_iterator& begin,
    const SampleVector::const_iterator& end,
    bool po)
{
    (void)po;
    int count = std::distance(begin, end);

    for (auto it = begin; it != end; ++it) {
        float i = it->real();
        float q = it->imag();

        uint8_t soft = m_gfsk.processSample(i, q);
        m_crltr.process(soft);
    }
}

void GeoscanDemod::start()
{
    qDebug() << "GeoscanDemod: запущен";
}

void GeoscanDemod::stop()
{
    qDebug() << "GeoscanDemod: остановлен";
}

bool GeoscanDemod::handleMessage(const Message& cmd)
{
    if (DSPSignalNotification::match(cmd)) {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) cmd;
        m_sampleRate = notif.getSampleRate();                                                                                     
        m_gfsk = GFSK(m_sampleRate, SYMBOL_RATE);
        return true;
    }
    
    return false;
}

void GeoscanDemod::onPacketReady(const std::vector<uint8_t>& packet)
{
    qDebug() << "GeoscanDemod: получен пакет, байт:" << packet.size();
}