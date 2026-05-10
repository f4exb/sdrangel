#include "geoscandemod.h"
#include <QDebug>
#include <tuple>

// Уникальные идентификаторы нашего плагина
const char* const GeoscanDemod::m_channelIdURI = "sdrangel.channel.geoscandemod";
const char* const GeoscanDemod::m_channelId    = "GeoscanDemod";
MESSAGE_CLASS_DEFINITION(GeoscanDemod::MsgConfigureGeoscan, Message)

GeoscanDemod::GeoscanDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_sampleRate(DEFAULT_SAMPLE_RATE),
    m_dcBlocker(GeoscanSettings{}.dev, DEFAULT_SAMPLE_RATE),
    m_gfsk(DEFAULT_SAMPLE_RATE, SYMBOL_RATE),
    m_lpf(GeoscanSettings{}.bw, DEFAULT_SAMPLE_RATE),
    m_crltr((int)(DEFAULT_SAMPLE_RATE / SYMBOL_RATE), GeoscanSettings{}.threshold, [this](const std::vector<uint8_t>& packet) {
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
    
    float sumSq = 0.0f;
    int count = 0;
    for (auto it = begin; it != end; ++it) {
        float i = it->real();
        float q = it->imag();

        sumSq += i*i + q*q;
        count++;

        std::tie(i, q) = m_dcBlocker.process(i, q);
        uint8_t soft = m_gfsk.processSample(i, q);
        uint8_t filteredSoft = m_lpf.process(soft);
        m_crltr.process(filteredSoft);
    }
	if (count > 0) {
	    float rms = std::sqrt(sumSq / count);
	    m_snr = 20.0f * std::log10(rms + 1e-10f);
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

void GeoscanDemod::applySettings(const GeoscanSettings& settings)
{
    m_settings = settings;
    m_dcBlocker.setDev(m_settings.dev, m_sampleRate);
    m_lpf.setBw(m_settings.bw, m_sampleRate);
    m_crltr = Correlator(
        (int)(m_sampleRate / SYMBOL_RATE),
        m_settings.threshold,
        [this](const std::vector<uint8_t>& packet) {
            onPacketReady(packet);
        }
    );
}

bool GeoscanDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureGeoscan::match(cmd)) {
        const MsgConfigureGeoscan& cfg = (const MsgConfigureGeoscan&) cmd;
        applySettings(cfg.getSettings());
        return true;
    }
    if (DSPSignalNotification::match(cmd)) {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) cmd;
        m_sampleRate = notif.getSampleRate();
        m_gfsk = GFSK(m_sampleRate, SYMBOL_RATE);
        applySettings(m_settings);
        return true;
    }
    return false;
}

void GeoscanDemod::onPacketReady(const std::vector<uint8_t>& packet)
{
    qDebug() << "GeoscanDemod: получен пакет, байт:" << packet.size();
}
