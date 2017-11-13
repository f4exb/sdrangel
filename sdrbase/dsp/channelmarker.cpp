#include "dsp/channelmarker.h"
#include "util/simpleserializer.h"

QRgb ChannelMarker::m_colorTable[] = {
	qRgb(0xc0, 0x00, 0x00),
	qRgb(0x00, 0xc0, 0x00),
	qRgb(0x00, 0x00, 0xc0),

	qRgb(0xc0, 0xc0, 0x00),
	qRgb(0xc0, 0x00, 0xc0),
	qRgb(0x00, 0xc0, 0xc0),

	qRgb(0xc0, 0x60, 0x00),
	qRgb(0xc0, 0x00, 0x60),
	qRgb(0x60, 0x00, 0xc0),

	qRgb(0x60, 0x00, 0x00),
	qRgb(0x00, 0x60, 0x00),
	qRgb(0x00, 0x00, 0x60),

	qRgb(0x60, 0x60, 0x00),
	qRgb(0x60, 0x00, 0x60),
	qRgb(0x00, 0x60, 0x60),

	0
};
int ChannelMarker::m_nextColor = 0;

ChannelMarker::ChannelMarker(QObject* parent) :
	QObject(parent),
	m_centerFrequency(0),
	m_bandwidth(0),
    m_oppositeBandwidth(0),
	m_lowCutoff(0),
	m_sidebands(dsb),
	m_visible(false),
	m_highlighted(false),
	m_color(m_colorTable[m_nextColor]),
	m_movable(true),
	m_udpReceivePort(9999),
	m_udpSendPort(9998),
	m_fScaleDisplayType(FScaleDisplay_freq)
{
	++m_nextColor;
	if(m_colorTable[m_nextColor] == 0)
		m_nextColor = 0;
	setUDPAddress("127.0.0.1");
}

void ChannelMarker::emitChangedByAPI()
{
    emit changedByAPI();
}

void ChannelMarker::setTitle(const QString& title)
{
	m_title = title;
	emit changedByAPI();
}

void ChannelMarker::setCenterFrequency(int centerFrequency)
{
	m_centerFrequency = centerFrequency;
	emit changedByAPI();
}

void ChannelMarker::setCenterFrequencyByCursor(int centerFrequency)
{
    m_centerFrequency = centerFrequency;
    emit changedByCursor();
}

void ChannelMarker::setBandwidth(int bandwidth)
{
	m_bandwidth = bandwidth;
	emit changedByAPI();
}

void ChannelMarker::setOppositeBandwidth(int bandwidth)
{
    m_oppositeBandwidth = bandwidth;
    emit changedByAPI();
}

void ChannelMarker::setLowCutoff(int lowCutoff)
{
	m_lowCutoff = lowCutoff;
	emit changedByAPI();
}

void ChannelMarker::setSidebands(sidebands_t sidebands)
{
	m_sidebands = sidebands;
	emit changedByAPI();
}

void ChannelMarker::setVisible(bool visible)
{
	m_visible = visible;
	emit changedByAPI();
}

void ChannelMarker::setHighlighted(bool highlighted)
{
	m_highlighted = highlighted;
	emit changedByAPI();
}

void ChannelMarker::setHighlightedByCursor(bool highlighted)
{
    m_highlighted = highlighted;
    emit highlightedByCursor();
}

void ChannelMarker::setColor(const QColor& color)
{
	m_color = color;
	emit changedByAPI();
}

void ChannelMarker::setUDPAddress(const QString& udpAddress)
{
    m_udpAddress = udpAddress;
    m_displayAddressReceive = QString(tr("%1:%2").arg(getUDPAddress()).arg(getUDPSendPort()));
    m_displayAddressReceive = QString(tr("%1:%2").arg(getUDPAddress()).arg(getUDPReceivePort()));
    emit changedByAPI();
}

void ChannelMarker::setUDPReceivePort(quint16 port)
{
    m_udpReceivePort = port;
    m_displayAddressReceive = QString(tr("%1:%2").arg(getUDPAddress()).arg(getUDPReceivePort()));
    emit changedByAPI();
}

void ChannelMarker::setUDPSendPort(quint16 port)
{
    m_udpSendPort = port;
    m_displayAddressSend = QString(tr("%1:%2").arg(getUDPAddress()).arg(getUDPSendPort()));
    emit changedByAPI();
}

void ChannelMarker::resetToDefaults()
{
    setTitle("Channel");
    setCenterFrequency(0);
    setColor(Qt::white);
    setUDPAddress("127.0.0.1");
    setUDPSendPort(9999);
    setUDPReceivePort(9999);
    setFrequencyScaleDisplayType(FScaleDisplay_freq);
}

QByteArray ChannelMarker::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, getCenterFrequency());
    s.writeU32(2, getColor().rgb());
    s.writeString(3, getTitle());
    s.writeString(4, getUDPAddress());
    s.writeU32(5, (quint32) getUDPReceivePort());
    s.writeU32(6, (quint32) getUDPSendPort());
    s.writeS32(7, (int) getFrequencyScaleDisplayType());
    return s.final();
}

bool ChannelMarker::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        quint32 u32tmp;
        qint32 tmp;
        QString strtmp;

        blockSignals(true);

        d.readS32(1, &tmp, 0);
        setCenterFrequency(tmp);
        if(d.readU32(2, &u32tmp)) {
            setColor(u32tmp);
        }
        d.readString(3, &strtmp, "Channel");
        setTitle(strtmp);
        d.readString(4, &strtmp, "127.0.0.1");
        setUDPAddress(strtmp);
        d.readU32(5, &u32tmp, 9999);
        setUDPReceivePort(u32tmp);
        d.readU32(6, &u32tmp, 9999);
        setUDPSendPort(u32tmp);
        d.readS32(7, &tmp, 0);
        if ((tmp >= 0) || (tmp < FScaleDisplay_none)) {
            setFrequencyScaleDisplayType((frequencyScaleDisplay_t) tmp);
        } else {
            setFrequencyScaleDisplayType(FScaleDisplay_freq);
        }

        blockSignals(false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

