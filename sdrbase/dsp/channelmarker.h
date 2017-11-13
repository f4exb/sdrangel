#ifndef INCLUDE_CHANNELMARKER_H
#define INCLUDE_CHANNELMARKER_H

#include <QObject>
#include <QColor>
#include <QByteArray>

#include "settings/serializable.h"
#include "util/export.h"

class SDRANGEL_API ChannelMarker : public QObject, public Serializable {
	Q_OBJECT

public:
	typedef enum sidebands_e
	{
		dsb,
		lsb,
		usb,
		vusb, //!< USB with vestigial LSB
		vlsb  //!< LSB with vestigial USB
	} sidebands_t;

	typedef enum frequencyScaleDisplay_e
	{
	    FScaleDisplay_freq,
	    FScaleDisplay_title,
	    FScaleDisplay_addressSend,
	    FScaleDisplay_addressReceive,
	    FScaleDisplay_none
	} frequencyScaleDisplay_t;

	ChannelMarker(QObject* parent = NULL);

	void emitChangedByAPI();

	void setTitle(const QString& title);
	const QString& getTitle() const { return m_title; }

	void setCenterFrequency(int centerFrequency);
    void setCenterFrequencyByCursor(int centerFrequency);
	int getCenterFrequency() const { return m_centerFrequency; }

	void setBandwidth(int bandwidth);
	int getBandwidth() const { return m_bandwidth; }

    void setOppositeBandwidth(int bandwidth);
    int getOppositeBandwidth() const { return m_oppositeBandwidth; }

	void setLowCutoff(int lowCutoff);
	int getLowCutoff() const { return m_lowCutoff; }

	void setSidebands(sidebands_t sidebands);
	sidebands_t getSidebands() const { return m_sidebands; }

	void setVisible(bool visible);
	bool getVisible() const { return m_visible; }

	void setHighlighted(bool highlighted);
    void setHighlightedByCursor(bool highlighted);
	bool getHighlighted() const { return m_highlighted; }

	void setColor(const QColor& color);
	const QColor& getColor() const { return m_color; }

	void setMovable(bool movable) { m_movable = movable; }
	bool getMovable() const { return m_movable; }

	void setUDPAddress(const QString& udpAddress);
	const QString& getUDPAddress() const { return m_udpAddress; }

	void setUDPReceivePort(quint16 port);
	quint16 getUDPReceivePort() const { return m_udpReceivePort; }

    void setUDPSendPort(quint16 port);
    quint16 getUDPSendPort() const { return m_udpSendPort; }

    void setFrequencyScaleDisplayType(frequencyScaleDisplay_t type) { m_fScaleDisplayType = type; }
    frequencyScaleDisplay_t getFrequencyScaleDisplayType() const { return m_fScaleDisplayType; }

    const QString& getDisplayAddressSend() const { return m_displayAddressSend; }
    const QString& getDisplayAddressReceive() const { return m_displayAddressReceive; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

protected:
	static QRgb m_colorTable[];
	static int m_nextColor;

	QString m_title;
	QString m_displayAddressSend;
    QString m_displayAddressReceive;
	int m_centerFrequency;
	int m_bandwidth;
    int m_oppositeBandwidth;
	int m_lowCutoff;
	sidebands_t m_sidebands;
	bool m_visible;
	bool m_highlighted;
	QColor m_color;
	bool m_movable;
	QString m_udpAddress;
	quint16 m_udpReceivePort;
    quint16 m_udpSendPort;
    frequencyScaleDisplay_t m_fScaleDisplayType;

    void resetToDefaults();

signals:
	void changedByAPI();
    void changedByCursor();
    void highlightedByCursor();
};

#endif // INCLUDE_CHANNELMARKER_H
