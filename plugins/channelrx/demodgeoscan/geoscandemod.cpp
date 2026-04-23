#include "geoscandemod.h"
#include <QDebug>

// Уникальные идентификаторы нашего плагина
const char* const GeoscanDemod::m_channelIdURI = "sdrangel.channel.geoscandemod";
const char* const GeoscanDemod::m_channelId    = "GeoscanDemod";

GeoscanDemod::GeoscanDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI)
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
    qDebug() << "GeoscanDemod: получено сэмплов:" << count;

    // Здесь будет GFSK демодулятор
    // Пример чтения одного сэмпла:
    // for (auto it = begin; it != end; ++it) {
    //     float i = it->real();
    //     float q = it->imag();
    // }
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
    (void)cmd;
    return false;
}